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
#include <set>

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
    ImageDisplayWindow displayWindow;
    int lastRequestNr;
    unsigned int frameCnt;
    ChannelSpecificData( const string& windowTitle ) : displayWindow( windowTitle ), lastRequestNr( INVALID_ID ), frameCnt( 0 ) {}
};

typedef vector<ChannelSpecificData*> ChannelContainer;

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
            ostringstream windowTitle;
            windowTitle << "Video channel " << i;
            channelSpecificData_.push_back( new ChannelSpecificData( windowTitle.str() ) );
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
unsigned int __stdcall liveThread( void* pData )
//-----------------------------------------------------------------------------
{
    CaptureParameters* pThreadParameter = reinterpret_cast<CaptureParameters*>( pData );
    const unsigned int videoChannelCount = static_cast<unsigned int>( pThreadParameter->channelSpecificData().size() );
    // make sure there are at least 4 buffers for each channel of the device
    // in a 'real' application more buffers might be needed or certain channels require more buffers then others depending on
    // the connected video sources
    const unsigned int requestCount = videoChannelCount * 4;
    pThreadParameter->systemSettings().requestCount.write( requestCount );
    FunctionInterface& fi = pThreadParameter->functionInterface();
    Connector& connector = pThreadParameter->connector();
    CTime timer;
    for( unsigned int i = 0; i < requestCount; i++ )
    {
        // request frames from all channel of the device
        connector.videoChannel.write( i % videoChannelCount );
        fi.imageRequestSingle();
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
                    pChannelData->displayWindow.GetImageDisplay().SetImage( pRequest );
                    pChannelData->displayWindow.GetImageDisplay().Update();
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
                    oss << pThreadParameter->statistics().framesPerSecond.readS() << ", " << pThreadParameter->statistics().errorCount.readS() << ", frames captured: ";

                    for( unsigned int j = 0; j < videoChannelCount; j++ )
                    {
                        ChannelSpecificData* pCD = pThreadParameter->channelSpecificData()[j];
                        oss << "channel " << j << " " << pCD->frameCnt << "(" << static_cast<double>( pCD->frameCnt ) / timer.elapsed() << "fps), ";
                    }
                    cout << oss.str() << endl;
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
        else
        {
            // If the error code is -2119(DEV_WAIT_FOR_REQUEST_FAILED), the documentation will provide
            // additional information under TDMR_ERROR in the interface reference
            cout << "imageRequestWaitFor failed (" << ImpactAcquireException::getErrorCodeAsString( requestNr ) << "), timeout value too small?" << endl;
        }
    }
    manuallyStopAcquisitionIfNeeded( pThreadParameter->device(), fi );

    for( unsigned int k = 0; k < videoChannelCount; k++ )
    {
        ChannelSpecificData* pChannelSpecificData = pThreadParameter->channelSpecificData()[k];
        if( pChannelSpecificData )
        {
            // stop the display from showing freed memory
            pChannelSpecificData->displayWindow.GetImageDisplay().RemoveImage();
            if( fi.isRequestNrValid( pChannelSpecificData->lastRequestNr ) )
            {
                fi.imageRequestUnlock( pChannelSpecificData->lastRequestNr );
                pChannelSpecificData->lastRequestNr = INVALID_ID;
            }
        }
    }

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

    if( pDev->deviceClass.read() == dcFrameGrabber )
    {
        // only frame grabbers support camera descriptions
        selectCameraDescriptionsFromUserInput( pDev );
    }

    CaptureParameters params( pDev );

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
