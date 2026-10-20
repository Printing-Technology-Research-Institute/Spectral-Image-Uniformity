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
#include <set>

using namespace std;
using namespace mvIMPACT::acquire;

static bool s_boTerminated = false;

//-----------------------------------------------------------------------------
struct ChannelSpecificData
//-----------------------------------------------------------------------------
{
#ifdef USE_DISPLAY
    unique_ptr<ImageDisplayWindow> pDisplayWindow;
#endif // #ifdef USE_DISPLAY
    int lastRequestNr;
    unsigned int frameCnt;
    explicit ChannelSpecificData() : lastRequestNr( INVALID_ID ), frameCnt( 0 ) {}
    ChannelSpecificData( const ChannelSpecificData& src ) = delete;
    ChannelSpecificData& operator=( const ChannelSpecificData& rhs ) = delete;
};

using ChannelContainer = vector<shared_ptr<ChannelSpecificData>>;

//-----------------------------------------------------------------------------
class CaptureParameters
//-----------------------------------------------------------------------------
{
    Device* pDev_;
    Connector connector_;
    FunctionInterface fi_;
    Statistics statistics_;
    SystemSettings ss_;
    ChannelContainer channelSpecificData_;
public:
    explicit CaptureParameters( Device* p ) : pDev_( p ), connector_( p ), fi_( p ), statistics_( p ), ss_( p ), channelSpecificData_()
    {
        if( !connector_.videoChannel.isValid() )
        {
            cout << "Error! This device does not support the video channel property. Terminating!" << endl;
            exit( 42 );
        }

        const int videoChannelMax = connector_.videoChannel.read( plMaxValue );
        for( int i = 0; i <= videoChannelMax; i++ )
        {
            channelSpecificData_.push_back( make_shared<ChannelSpecificData>() );
#ifdef USE_DISPLAY
            ostringstream windowTitle;
            windowTitle << "Video channel " << i;
            channelSpecificData_.back()->pDisplayWindow = unique_ptr<ImageDisplayWindow>( new ImageDisplayWindow( windowTitle.str() ) );
#endif // #ifdef USE_DISPLAY
        }
    }
    CaptureParameters( const CaptureParameters& src ) = delete;
    CaptureParameters& operator=( const CaptureParameters& rhs ) = delete;
    Device* device( void ) const
    {
        return pDev_;
    }
    Connector& connector( void )
    {
        return connector_;
    }
    FunctionInterface& functionInterface( void )
    {
        return fi_;
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
};

//-----------------------------------------------------------------------------
// select a camera description from the list of available camera descriptions for
// each channel that is part of the capture sequence
void selectCameraDescriptionsFromUserInput( Device* pDev )
//-----------------------------------------------------------------------------
{
    bool boRun = true;
    while( boRun )
    {
        try
        {
            CameraSettingsFrameGrabber cs( pDev );
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
            cout << "ERROR while selecting camera description: " << e.getErrorString() << endl;
        }
    }
}

//-----------------------------------------------------------------------------
void liveThread( CaptureParameters* pThreadParameter )
//-----------------------------------------------------------------------------
{
    const unsigned int videoChannelCount = static_cast<unsigned int>( pThreadParameter->channelSpecificData().size() );
    // make sure there are at least 4 buffers for each channel of the device
    // in a 'real' application more buffers might be needed or certain channels require more buffers then others depending on
    // the connected video sources
    const unsigned int requestCount = videoChannelCount * 4;
    pThreadParameter->systemSettings().requestCount.write( requestCount );
    FunctionInterface& fi = pThreadParameter->functionInterface();
    Connector& connector = pThreadParameter->connector();
    chrono::high_resolution_clock::time_point startTime = chrono::high_resolution_clock::now();

    for( unsigned int i = 0; i < requestCount; i++ )
    {
        // request frames from all channel of the device
        connector.videoChannel.write( i % videoChannelCount );
        fi.imageRequestSingle();
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
                if( pChannelData.get() != nullptr )
                {
                    ++pChannelData->frameCnt;
#ifdef USE_DISPLAY
                    pChannelData->pDisplayWindow->GetImageDisplay().SetImage( pRequest );
                    pChannelData->pDisplayWindow->GetImageDisplay().Update();
#endif // #ifdef USE_DISPLAY
                    if( fi.isRequestNrValid( pThreadParameter->channelSpecificData()[videoChannel]->lastRequestNr ) )
                    {
                        fi.imageRequestUnlock( pThreadParameter->channelSpecificData()[videoChannel]->lastRequestNr );
                    }
                    pThreadParameter->channelSpecificData()[videoChannel]->lastRequestNr = requestNr;
                }
                else
                {
                    cout << "ERROR: A request has been returned from channel " << videoChannel << " however no display has been allocated for this channel." << endl;
                    fi.imageRequestUnlock( requestNr );
                }
                // here we can display some statistical information every 100th image
                if( cnt % 100 == 0 )
                {
                    ostringstream oss;
                    for( unsigned int j = 0; j < videoChannelCount; j++ )
                    {
                        if( !oss.str().empty() )
                        {
                            oss << ", ";
                        }
                        shared_ptr<ChannelSpecificData> pCD = pThreadParameter->channelSpecificData()[j];
                        const double elapsedTime = chrono::duration_cast<chrono::duration<double>>( chrono::high_resolution_clock::now() - startTime ).count();
                        oss << "channel " << j << " " << pCD->frameCnt << "(" << static_cast<double>( pCD->frameCnt ) / elapsedTime << "fps)";
                    }
                    cout << pThreadParameter->statistics().framesPerSecond.readS() << ", " << pThreadParameter->statistics().errorCount.readS() << ", frames captured: " << oss.str() << endl;
                }
            }
            else
            {
                cout << "Error: " << pRequest->requestResult.readS() << endl;
                fi.imageRequestUnlock( requestNr );
            }
            connector.videoChannel.write( videoChannel );
            fi.imageRequestSingle();
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

    for( shared_ptr<ChannelSpecificData> pChannelSpecificData : pThreadParameter->channelSpecificData() )
    {
#ifdef USE_DISPLAY
        // stop the display from showing freed memory
        pChannelSpecificData->pDisplayWindow->GetImageDisplay().RemoveImage();
#endif // #ifdef USE_DISPLAY
        if( fi.isRequestNrValid( pChannelSpecificData->lastRequestNr ) )
        {
            fi.imageRequestUnlock( pChannelSpecificData->lastRequestNr );
            pChannelSpecificData->lastRequestNr = INVALID_ID;
        }
    }

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

    if( pDev->deviceClass.read() == dcFrameGrabber )
    {
        // only frame grabbers support camera descriptions
        selectCameraDescriptionsFromUserInput( pDev );
    }

    CaptureParameters params( pDev );

    // start the execution of the 'live' thread.
    cout << "Press [ENTER] to end the application" << endl;
    thread myThread( liveThread, &params );
    cin.get();
    s_boTerminated = true;
    myThread.join();
    return 0;
}
