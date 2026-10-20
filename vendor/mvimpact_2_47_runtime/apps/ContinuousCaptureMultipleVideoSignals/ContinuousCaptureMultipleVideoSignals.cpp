#include <cassert>
#include <iostream>
#include <memory>
#include <thread>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#ifdef _WIN32
#   include <mvDisplay/Include/mvIMPACT_acquire_display.h>
using namespace mvIMPACT::acquire::display;
#   define USE_DISPLAY
#endif // #ifdef _WIN32

using namespace std;
using namespace mvIMPACT::acquire;

static bool s_boTerminated = false;

//-----------------------------------------------------------------------------
struct ChannelSpecificData
//-----------------------------------------------------------------------------
{
#ifdef USE_DISPLAY
    unique_ptr<ImageDisplayWindow> pCaptureDisplay;
#endif // #ifdef USE_DISPLAY
    ComponentList setting;
    int lastRequestNr;
    unsigned int frameCnt;
    ChannelSpecificData( ComponentList s ) :
#ifdef USE_DISPLAY
        pCaptureDisplay(),
#endif // #ifdef USE_DISPLAY
        setting( s ), lastRequestNr( INVALID_ID ), frameCnt( 0 ) {}
    ChannelSpecificData( const ChannelSpecificData& src ) = delete;
    ChannelSpecificData& operator=( const ChannelSpecificData& rhs ) = delete;
};

using ChannelContainer = vector<shared_ptr<ChannelSpecificData>>;

//-----------------------------------------------------------------------------
class CaptureParameters
//-----------------------------------------------------------------------------
{
    Device* pDev_;
    FunctionInterface fi_;
    ImageRequestControl irc_;
    Statistics statistics_;
    SystemSettings ss_;
    ChannelContainer channelSpecificData_;
    vector<int> captureSequence_;
    vector<int> channelsUsed_;
    int videoChannelMax_ = -1;
public:
    explicit CaptureParameters( Device* p ) : pDev_( p ), fi_( p ), irc_( p ), statistics_( p ), ss_( p ),
        channelSpecificData_(), captureSequence_(), channelsUsed_()
    {
        Connector connector( p );
        if( connector.videoChannel.isValid() )
        {
            videoChannelMax_ = connector.videoChannel.read( plMaxValue );
        }
        for( int i = 0; i <= videoChannelMax_; i++ )
        {
            ostringstream oss;
            oss << "VideoChannel" << i;
            ComponentList setting;
            // create a setting for this video channel (more efficient for switching)
            int result = fi_.createSetting( oss.str(), "Base", &setting );
            if( result == DMR_NO_ERROR )
            {
                // select a fixed video channel for each setting
                Connector c( p, oss.str() );
                c.videoChannel.write( i );
                channelSpecificData_.push_back( make_shared<ChannelSpecificData>( setting ) );
            }
            else
            {
                cout << "ERROR while creating setting " << oss.str() << "(" << ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
                assert( !"ERROR while creating setting" );
            }
        }
    }
    CaptureParameters( const CaptureParameters& src ) = delete;
    CaptureParameters& operator=( const CaptureParameters& rhs ) = delete;
    Device* device( void ) const
    {
        return pDev_;
    }
    FunctionInterface& functionInterface( void )
    {
        return fi_;
    }
    ImageRequestControl& imageRequestControl( void )
    {
        return irc_;
    }
    Statistics& statistics( void )
    {
        return statistics_;
    }
    SystemSettings& systemSettings( void )
    {
        return ss_;
    }
    const ChannelContainer& channelSpecificData( void )
    {
        return channelSpecificData_;
    }
    const vector<int>& captureSequence( void )
    {
        return captureSequence_;
    }
    const vector<int>& channelsUsed( void )
    {
        return channelsUsed_;
    }
    void setCaptureSequence( const vector<int>& c )
    {
        captureSequence_ = c;
        // find out how many DIFFERENT channels are part of the sequence
        channelsUsed_ = c;
        sort( channelsUsed_.begin(), channelsUsed_.end() );
        channelsUsed_.erase( unique( channelsUsed_.begin(), channelsUsed_.end() ), channelsUsed_.end() );
        // make sure that each channel has at least 4 buffers to work with
        ss_.requestCount.write( static_cast<int>( channelsUsed_.size() * 4 ) );
        ChannelContainer::size_type channelDataCnt = channelSpecificData_.size();
        for( ChannelContainer::size_type i = 0; i < channelDataCnt; i++ )
        {
            channelSpecificData_[i]->lastRequestNr = INVALID_ID;
            channelSpecificData_[i]->frameCnt = 0;
        }
        // create a display for each channel that is part of the sequence
        for( int channel : channelsUsed_ )
        {
            ostringstream oss;
            oss << "video channel " << channel;
#ifdef USE_DISPLAY
            channelSpecificData_[channel]->pCaptureDisplay = unique_ptr<ImageDisplayWindow>( new ImageDisplayWindow( oss.str() ) );
#endif // #ifdef USE_DISPLAY
        }
    }
    int videoChannelMax( void ) const
    {
        return videoChannelMax_;
    }
};

//-----------------------------------------------------------------------------
void getCaptureSequenceFromUser( CaptureParameters& params )
//-----------------------------------------------------------------------------
{
    const int videoChannelMax = params.videoChannelMax();
    cout << endl
         << "This " << params.device()->product.readS() << " has " << videoChannelMax + 1 << " separate video inputs(";
    for( int i = 0; i <= videoChannelMax; i++ )
    {
        if( i > 0 )
        {
            cout << ", ";
        }
        cout << i;
    }
    cout << ").";
    cout << endl
         << "Please enter the zero based sequence of channels to capture from and press [ENTER]: ";

    static const size_t s_BUF_SIZE = {1024};
    char buf[s_BUF_SIZE];
    cin.getline( buf, s_BUF_SIZE, '\n' );
    string captureSequenceString( buf );
    // parse input
    string::size_type start = 0;
    vector<int> captureSequence;
    while( ( start = captureSequenceString.find_first_of( "0123456789", start ) ) != string::npos )
    {
        const auto end = captureSequenceString.find_first_not_of( "0123456789", start + 1 );
        const string channelString( captureSequenceString.substr( start, ( end != string::npos ) ? end - start : end ) );
        const int channel = atoi( channelString.c_str() );
        if( channel > videoChannelMax )
        {
            cout << "Channel " << channel << " is invalid for this device. Skipped!" << endl;
        }
        else
        {
            captureSequence.push_back( channel );
        }
        if( end == string::npos )
        {
            break;
        }
        start = end + 1;
    }
    params.setCaptureSequence( captureSequence );
}

//-----------------------------------------------------------------------------
// select a camera description from the list of available camera descriptions for
// each channel that is part of the capture sequence
void selectCameraDescriptionForChannelFromUserInput( CaptureParameters& params, int channel )
//-----------------------------------------------------------------------------
{
    bool boRun = true;
    while( boRun )
    {
        try
        {
            CameraSettingsFrameGrabber cs( params.device(), params.channelSpecificData()[channel]->setting.name() );
            // display the name of every camera description available for this device.
            // this might be less than the number of camera descriptions available on the system as e.g.
            // an analog frame grabber can't use descriptions for digital cameras
            vector<pair<string, int> > vAvailableDescriptions;
            cs.type.getTranslationDict( vAvailableDescriptions );
            cout << endl << "Available camera descriptions: " << vAvailableDescriptions.size() << endl
                 << "----------------------------------" << endl;
            for_each( vAvailableDescriptions.begin(), vAvailableDescriptions.end(), DisplayDictEntry<int>() );
            cout << endl << "Please select a camera description to use for this sample (numerical value): ";

            int cameraDescriptionToUse;
            cin >> cameraDescriptionToUse;
            // remove the '\n' from the stream
            cin.get();
            // wrong input will raise an exception
            // The camera description contains all the parameters needed to capture the associated video
            // signal. Camera descriptions for a certain camera can be obtained from MATRIX VISION. Experienced
            // users can also create their own camera descriptions using wxPropView. The documentation will
            // explain how to create and configure camera description files
            cs.type.write( cameraDescriptionToUse );
            boRun = false;
        }
        catch( const ImpactAcquireException& e )
        {
            cout << "ERROR while selecting camera description for channel " << channel << ": " << e.getErrorString() << endl;
        }
    }
}

//-----------------------------------------------------------------------------
void liveThread( CaptureParameters* pThreadParameter )
//-----------------------------------------------------------------------------
{
    const unsigned int requestCount = pThreadParameter->functionInterface().requestCount();
    const unsigned int captureSequenceCount = static_cast<unsigned int>( pThreadParameter->captureSequence().size() );
    FunctionInterface& fi = pThreadParameter->functionInterface();
    ImageRequestControl& irc = pThreadParameter->imageRequestControl();
    chrono::high_resolution_clock::time_point startTime = chrono::high_resolution_clock::now();
    for( unsigned int i = 0; i < requestCount; i++ )
    {
        // request frames in the order of the capture sequence by using the settings belonging to each channel
        irc.setting.write( pThreadParameter->channelSpecificData()[pThreadParameter->captureSequence()[i % captureSequenceCount]]->setting.hObj() );
        fi.imageRequestSingle( &irc );
    }

    manuallyStartAcquisitionIfNeeded( pThreadParameter->device(), fi );
    // run thread loop
    const Request* pRequest = nullptr;
    const unsigned int timeout_ms = {500};
    int requestNr = {INVALID_ID};
    unsigned int cnt = {0};
    // each display has to buffer the current frame as the display module might want to repaint the image, thus we
    // cannot free it unless we have a assigned the display to a new buffer.
    while( !s_boTerminated )
    {
        // wait for results from the default capture queue
        requestNr = fi.imageRequestWaitFor( timeout_ms );
        if( fi.isRequestNrValid( requestNr ) )
        {
            pRequest = fi.getRequest( requestNr );
            const int videoChannel = pRequest->infoVideoChannel.read();
            if( pRequest->isOK() )
            {
                ++cnt;
                shared_ptr<ChannelSpecificData> pChannelData = pThreadParameter->channelSpecificData()[videoChannel];
                if( pChannelData )
                {
                    ++pChannelData->frameCnt;
#ifdef USE_DISPLAY
                    pChannelData->pCaptureDisplay->GetImageDisplay().SetImage( pRequest );
                    pChannelData->pCaptureDisplay->GetImageDisplay().Update();
#endif // #ifdef USE_DISPLAY
                    if( fi.isRequestNrValid( pThreadParameter->channelSpecificData()[videoChannel]->lastRequestNr ) )
                    {
                        fi.imageRequestUnlock( pThreadParameter->channelSpecificData()[videoChannel]->lastRequestNr );
                    }
                    pThreadParameter->channelSpecificData()[videoChannel]->lastRequestNr = requestNr;
                }
                else
                {
                    cout << "ERROR: A request has been returned from channel " << videoChannel << " however no data structure has been allocated for this channel." << endl;
                    fi.imageRequestUnlock( requestNr );
                }
                // here we can display some statistical information every 100th image
                if( cnt % 100 == 0 )
                {
                    ostringstream oss;
                    for( auto channel : pThreadParameter->channelsUsed() )
                    {
                        if( !oss.str().empty() )
                        {
                            oss << ", ";
                        }
                        shared_ptr<ChannelSpecificData> pCD = pThreadParameter->channelSpecificData()[channel];
                        const double elapsedTime = chrono::duration_cast<chrono::duration<double>>( chrono::high_resolution_clock::now() - startTime ).count();
                        oss << "channel " << channel << " " << pCD->frameCnt << "(" << static_cast<double>( pCD->frameCnt ) / elapsedTime << "fps)";
                    }
                    cout << pThreadParameter->statistics().framesPerSecond.readS() << ", " << pThreadParameter->statistics().errorCount.readS() << ", frames captured: "
                         << oss.str() << endl;
                }
            }
            else
            {
                cout << "Error: " << pRequest->requestResult.readS() << endl;
                fi.imageRequestUnlock( requestNr );
            }
            // request the next image with the same settings as the buffer that just became ready
            irc.setting.write( pThreadParameter->channelSpecificData()[videoChannel]->setting.hObj() );
            fi.imageRequestSingle( &irc );
        }
        //else
        //{
        // Please note that slow systems or interface technologies in combination with high resolution sensors
        // might need more time to transmit an image than the timeout value which has been passed to imageRequestWaitFor().
        // If this is the case simply wait multiple times OR increase the timeout(not recommended as usually not necessary
        // and potentially makes the capture thread less responsive) and rebuild this application.
        // Once the device is configured for triggered image acquisition and the timeout elapsed before
        // the device has been triggered this might happen as well.
        // The return code would be -2119(DEV_WAIT_FOR_REQUEST_FAILED) in that case, the documentation will provide
        // additional information under TDMR_ERROR in the interface reference.
        // If waiting with an infinite timeout(-1) it will be necessary to call 'imageRequestReset' from another thread
        // to force 'imageRequestWaitFor' to return when no data is coming from the device/can be captured.
        // cout << "imageRequestWaitFor failed (" << requestNr << ", " << ImpactAcquireException::getErrorCodeAsString( requestNr ) << ")"
        //   << ", timeout value too small?" << endl;
        //}
    }
    manuallyStopAcquisitionIfNeeded( pThreadParameter->device(), fi );

    for( const auto& pChannel : pThreadParameter->channelSpecificData() )
    {
#ifdef USE_DISPLAY
        ImageDisplayWindow* pCaptureDisplay = pChannel->pCaptureDisplay.get();
        if( pCaptureDisplay )
        {
            // stop the display from showing freed memory
            pCaptureDisplay->GetImageDisplay().RemoveImage();
        }
#endif // #ifdef USE_DISPLAY
        if( fi.isRequestNrValid( pChannel->lastRequestNr ) )
        {
            fi.imageRequestUnlock( pChannel->lastRequestNr );
            pChannel->lastRequestNr = INVALID_ID;
        }
    }

    // In this sample all the next lines are redundant as the device driver will be
    // closed now, but in a real world application a thread like this might be started
    // several times an then it becomes crucial to clean up correctly.

    // clear all queues
    fi.imageRequestReset( 0, 0 );
}

//-----------------------------------------------------------------------------
bool isDeviceSupportedBySample( const Device* const pDev )
//-----------------------------------------------------------------------------
{
    return pDev->hasCapability( dcSelectableVideoInputs );
}

//-----------------------------------------------------------------------------
int main( void )
//-----------------------------------------------------------------------------
{
    cout << "This sample is meant for devices that support multiple video channels only. Other devices might be installed" << endl
         << "but won't be recognized by the application." << endl
         << endl;

    DeviceManager devMgr;
    Device* pDev = getDeviceFromUserInput( devMgr, isDeviceSupportedBySample );
    if( pDev == nullptr )
    {
        cout << "Unable to continue! Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    cout << "Initialising device " << pDev->serial.read() << ". This might take some time..." << endl;
    try
    {
        pDev->open();
    }
    catch( const ImpactAcquireException& e )
    {
        // this e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while opening the device(error code: " << e.getErrorCode() << ")." << endl
             << "Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    CaptureParameters params( pDev );
    getCaptureSequenceFromUser( params );
    if( params.captureSequence().empty() )
    {
        cout << "User defined capture sequence is empty. Unable to continue!" << endl
             << "Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    if( pDev->deviceClass.read() == dcFrameGrabber )
    {
        // only frame grabbers support camera descriptions
        for( auto channel : params.channelsUsed() )
        {
            selectCameraDescriptionForChannelFromUserInput( params, channel );
        }
    }

    // start the execution of the 'live' thread.
    cout << "Press [ENTER] to end the application" << endl;
    thread myThread( liveThread, &params );
    cin.get();
    s_boTerminated = true;
    myThread.join();
    return 0;
}
