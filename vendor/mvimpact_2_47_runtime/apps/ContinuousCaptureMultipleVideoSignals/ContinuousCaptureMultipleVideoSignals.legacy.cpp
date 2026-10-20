#ifdef _MSC_VER // is Microsoft compiler?
#   if _MSC_VER < 1300  // is 'old' VC 6 compiler?
#       pragma warning( disable : 4786 ) // 'identifier was truncated to '255' characters in the debug information'
#   endif // #if _MSC_VER < 1300
#endif // #ifdef _MSC_VER
#include <windows.h>
#include <process.h>
#include <cassert>
#include <iostream>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvDisplay/Include/mvIMPACT_acquire_display.h>

using namespace std;
using namespace mvIMPACT::acquire;
using namespace mvIMPACT::acquire::display;

static bool s_boTerminated = false;

//-----------------------------------------------------------------------------
class CTime
//-----------------------------------------------------------------------------
{
    //-----------------------------------------------------------------------------
    struct CTimeImpl
    //-----------------------------------------------------------------------------
    {
        LARGE_INTEGER frequency;
        LARGE_INTEGER start;
        LARGE_INTEGER end;
        CTimeImpl()
        {
            QueryPerformanceFrequency( &frequency );
            QueryPerformanceCounter( &start );
        }
    };
    struct CTimeImpl* m_pImpl;
public:
    /// \brief Constructs a new time measurement object
    ///
    /// This constructs the new object and starts a new measurement.
    ///
    /// \sa
    /// <b>CTime::start()</b>
    explicit CTime()
    {
        m_pImpl = new CTimeImpl();
    }
    ~CTime()
    {
        delete m_pImpl;
    }
    /// \brief Starts a new time measurement.
    ///
    /// Call <b>CTime::elapsed()</b> to get the timespan in sec. after the last call
    /// to <b>CTime::start()</b>.
    void start( void )
    {
        QueryPerformanceCounter( &( m_pImpl->start ) );
    }
    /// \brief Returns the elapsed time in sec.
    ///
    /// Returns the elapsed time in sec. after the last call to <b>CTime::start()</b>
    double elapsed( void )
    {
        QueryPerformanceCounter( &( m_pImpl->end ) );
        return static_cast<double>( m_pImpl->end.QuadPart - m_pImpl->start.QuadPart ) / m_pImpl->frequency.QuadPart;
    }
    /// \brief Returns the elapsed time in sec. and sets this time to the current time
    ///
    /// Returns the elapsed time in sec. after the last call to <b>CTime::start()</b>
    /// or <b>CTime::restart()</b> and sets this time to the current time.
    double restart( void )
    {
        QueryPerformanceCounter( &( m_pImpl->end ) );
        double result = static_cast<double>( m_pImpl->end.QuadPart - m_pImpl->start.QuadPart ) / m_pImpl->frequency.QuadPart;
        m_pImpl->start = m_pImpl->end;
        return result;
    }
};

//-----------------------------------------------------------------------------
struct ChannelSpecificData
//-----------------------------------------------------------------------------
{
    ImageDisplayWindow* pCaptureDisplay;
    ComponentList setting;
    int lastRequestNr;
    unsigned int frameCnt;
    ChannelSpecificData( ComponentList s ) : pCaptureDisplay( 0 ), setting( s ), lastRequestNr( INVALID_ID ), frameCnt( 0 ) {}
};

typedef vector<ChannelSpecificData*> ChannelContainer;

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
    int videoChannelMax_;
public:
    explicit CaptureParameters( Device* p ) : pDev_( p ), fi_( p ), irc_( p ), statistics_( p ), ss_( p ),
        channelSpecificData_(), captureSequence_(), channelsUsed_(), videoChannelMax_( -1 )
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
                channelSpecificData_.push_back( new ChannelSpecificData( setting ) );
            }
            else
            {
                cout << "ERROR while creating setting " << oss.str() << "(" << ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
                assert( !"ERROR while creating setting" );
            }
        }
    }
    ~CaptureParameters()
    {
        const ChannelContainer::size_type cnt = channelSpecificData_.size();
        for( ChannelContainer::size_type i = 0; i < cnt; i++ )
        {
            delete channelSpecificData_[i];
        }
    }
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
            delete channelSpecificData_[i]->pCaptureDisplay;
            channelSpecificData_[i]->pCaptureDisplay = 0;
            channelSpecificData_[i]->lastRequestNr = INVALID_ID;
            channelSpecificData_[i]->frameCnt = 0;
        }
        // create a display for each channel that is part of the sequence
        vector<int>::const_iterator it = channelsUsed_.begin();
        vector<int>::const_iterator itEND = channelsUsed_.end();
        while( it != itEND )
        {
            ostringstream oss;
            oss << "video channel " << *it;
            channelSpecificData_[*it]->pCaptureDisplay = new ImageDisplayWindow( oss.str() );
            ++it;
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

    const size_t BUF_SIZE = 1024;
    char buf[BUF_SIZE];
    cin.getline( buf, BUF_SIZE, '\n' );
    string captureSequenceString( buf );
    // parse input
    string::size_type start = 0;
    string::size_type end = string::npos;
    vector<int> captureSequence;
    while( ( start = captureSequenceString.find_first_of( "0123456789", start ) ) != string::npos )
    {
        end = captureSequenceString.find_first_not_of( "0123456789", start + 1 );
        string channelString( captureSequenceString.substr( start, ( end != string::npos ) ? end - start : end ) );
        int channel = atoi( channelString.c_str() );
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
void selectCameraDescriptionsFromUserInput( CaptureParameters& params )
//-----------------------------------------------------------------------------
{
    vector<int>::const_iterator it = params.channelsUsed().begin();
    vector<int>::const_iterator itEND = params.channelsUsed().end();
    while( it != itEND )
    {
        bool boRun = true;
        while( boRun )
        {
            try
            {
                CameraSettingsFrameGrabber cs( params.device(), params.channelSpecificData()[*it]->setting.name() );
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
                cout << "ERROR while selecting camera description for channel " << *it << ": " << e.getErrorString() << endl;
            }
        }
        ++it;
    }
}

//-----------------------------------------------------------------------------
unsigned int __stdcall liveThread( void* pData )
//-----------------------------------------------------------------------------
{
    CaptureParameters* pThreadParameter = reinterpret_cast<CaptureParameters*>( pData );
    const unsigned int requestCount = pThreadParameter->functionInterface().requestCount();
    const unsigned int captureSequenceCount = static_cast<unsigned int>( pThreadParameter->captureSequence().size() );
    FunctionInterface& fi = pThreadParameter->functionInterface();
    ImageRequestControl& irc = pThreadParameter->imageRequestControl();
    CTime timer;
    for( unsigned int i = 0; i < requestCount; i++ )
    {
        // request frames in the order of the capture sequence by using the settings belonging to each channel
        irc.setting.write( pThreadParameter->channelSpecificData()[pThreadParameter->captureSequence()[i % captureSequenceCount]]->setting.hObj() );
        fi.imageRequestSingle( &irc );
    }

    manuallyStartAcquisitionIfNeeded( pThreadParameter->device(), fi );
    // run thread loop
    const Request* pRequest = 0;
    const unsigned int timeout_ms = 500;
    int requestNr = INVALID_ID;
    unsigned int cnt = 0;
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
                ChannelSpecificData* pChannelData = pThreadParameter->channelSpecificData()[videoChannel];
                if( pChannelData )
                {
                    ++( pChannelData->frameCnt );
                    pChannelData->pCaptureDisplay->GetImageDisplay().SetImage( pRequest );
                    pChannelData->pCaptureDisplay->GetImageDisplay().Update();
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
                    vector<int>::const_iterator it = pThreadParameter->channelsUsed().begin();
                    vector<int>::const_iterator itEND = pThreadParameter->channelsUsed().end();
                    ostringstream oss;
                    oss << pThreadParameter->statistics().framesPerSecond.readS() << ", " << pThreadParameter->statistics().errorCount.readS() << ", frames captured: ";
                    while( it != itEND )
                    {
                        ChannelSpecificData* pCD = pThreadParameter->channelSpecificData()[*it];
                        oss << "channel " << *it << " " << pCD->frameCnt << "(" << static_cast<double>( pCD->frameCnt ) / timer.elapsed() << "fps), ";
                        ++it;
                    }
                    cout << oss.str() << endl;
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
        else
        {
            // If the error code is -2119(DEV_WAIT_FOR_REQUEST_FAILED), the documentation will provide
            // additional information under TDMR_ERROR in the interface reference
            cout << "imageRequestWaitFor failed (" << ImpactAcquireException::getErrorCodeAsString( requestNr ) << "), timeout value too small?" << endl;
        }
    }
    manuallyStopAcquisitionIfNeeded( pThreadParameter->device(), fi );

    const ChannelContainer::size_type channelDataSize = pThreadParameter->channelSpecificData().size();
    for( ChannelContainer::size_type j = 0; j < channelDataSize; j++ )
    {
        ImageDisplayWindow* pCaptureDisplay = pThreadParameter->channelSpecificData()[j]->pCaptureDisplay;
        if( pCaptureDisplay )
        {
            // stop the display from showing freed memory
            pCaptureDisplay->GetImageDisplay().RemoveImage();
        }
        if( fi.isRequestNrValid( pThreadParameter->channelSpecificData()[j]->lastRequestNr ) )
        {
            fi.imageRequestUnlock( pThreadParameter->channelSpecificData()[j]->lastRequestNr );
            pThreadParameter->channelSpecificData()[j]->lastRequestNr = INVALID_ID;
        }
    }

    // In this sample all the next lines are redundant as the device driver will be
    // closed now, but in a real world application a thread like this might be started
    // several times an then it becomes crucial to clean up correctly.

    // clear all queues
    fi.imageRequestReset( 0, 0 );
    return 0;
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
    if( !pDev )
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
        selectCameraDescriptionsFromUserInput( params );
    }

    // start the execution of the 'live' thread.
    cout << "Press [ENTER] to end the application" << endl;
    unsigned int dwThreadID;
    HANDLE hThread = ( HANDLE )_beginthreadex( 0, 0, liveThread, ( LPVOID )( &params ), 0, &dwThreadID );
    cin.get();
    s_boTerminated = true;
    WaitForSingleObject( hThread, INFINITE );
    CloseHandle( hThread );
    return 0;
}
