#ifdef _MSC_VER // is Microsoft compiler?
#   if _MSC_VER < 1300  // is 'old' VC 6 compiler?
#       pragma warning( disable : 4786 ) // 'identifier was truncated to '255' characters in the debug information'
#   endif // #if _MSC_VER < 1300
#endif // #ifdef _MSC_VER
#include <stdio.h>
#include <windows.h>
#include <process.h>
#include <iostream>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvDisplay/Include/mvIMPACT_acquire_display.h>

using namespace std;
using namespace mvIMPACT::acquire;
using namespace mvIMPACT::acquire::display;

//-----------------------------------------------------------------------------
/// to get thread safe access to the std out and the display module
class CriticalSection
//-----------------------------------------------------------------------------
{
    CRITICAL_SECTION m_criticalSection;
public:
    CriticalSection()
    {
        InitializeCriticalSection( &m_criticalSection );
    }
    ~CriticalSection()
    {
        DeleteCriticalSection( &m_criticalSection );
    }
    void lock( void )
    {
        EnterCriticalSection( &m_criticalSection );
    }
    void unlock( void )
    {
        LeaveCriticalSection( &m_criticalSection );
    }
} g_critSect;

//-----------------------------------------------------------------------------
class LockedScope
//-----------------------------------------------------------------------------
{
    CriticalSection&    m_l;
    LockedScope( const LockedScope& );              // do not allow copy constructor
    LockedScope& operator=( const LockedScope& );   // do not allow assignments
public:
    explicit LockedScope( CriticalSection& l ) : m_l( l )
    {
        m_l.lock();
    }
    ~LockedScope()
    {
        m_l.unlock();
    }
};

//-----------------------------------------------------------------------------
class ThreadParameter
//-----------------------------------------------------------------------------
{
    Device*             m_pDev;
    ImageDisplayWindow  m_displayWindow;
    volatile bool       m_boTerminateThread;
public:
    explicit ThreadParameter( Device* pDev, const std::string& windowTitle ) : m_pDev( pDev ), m_displayWindow( windowTitle ), m_boTerminateThread( false ) {}
    Device* device( void ) const
    {
        return m_pDev;
    }
    ImageDisplayWindow& displayWindow( void )
    {
        return m_displayWindow;
    }
    bool terminated( void ) const
    {
        return m_boTerminateThread;
    }
    void terminateThread( void )
    {
        m_boTerminateThread = true;
    }
};

//-----------------------------------------------------------------------------
void displayCommandLineOptions( void )
//-----------------------------------------------------------------------------
{
    cout << "Available parameters:" << endl
         << "  'product' or 'p' to specify a certain product type. All other products will be ignored then" << endl
         << "      a '*' serves as a wildcard." << endl
         << endl
         << "USAGE EXAMPLE:" << endl
         << "  ContinuousCaptureAllDevices p=mvBlue* " << endl << endl;
}

//-----------------------------------------------------------------------------
unsigned int __stdcall liveThread( void* pData )
//-----------------------------------------------------------------------------
{
    ThreadParameter* pThreadParameter = reinterpret_cast<ThreadParameter*>( pData );
    Device* pDev = pThreadParameter->device();

    {
        LockedScope lockedScope( g_critSect );
        cout << "Trying to open " << pDev->serial.read() << endl;
    }

    try
    {
        // if this device offers the 'GenICam' interface switch it on, as this will
        // allow are better control over GenICam compliant devices
        conditionalSetProperty( pDev->interfaceLayout, dilGenICam );
        // if this device offers a user defined acquisition start/stop behaviour
        // enable it as this allows finer control about the streaming behaviour
        conditionalSetProperty( pDev->acquisitionStartStopBehaviour, assbUser );
        pDev->open();
    }
    catch( const ImpactAcquireException& e )
    {
        LockedScope lockedScope( g_critSect );
        // this e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while opening the device " << pDev->serial.read()
             << "(error code: " << e.getErrorCode() << "(" << e.getErrorCodeAsString() << ")). Terminating thread." << endl
             << "Press [ENTER] to end the application..."
             << endl;
        return 0;
    }

    {
        LockedScope lockedScope( g_critSect );
        cout << "Opened " << pDev->serial.read() << endl;
    }

    ImageDisplay& display = pThreadParameter->displayWindow().GetImageDisplay();
    // establish access to the statistic properties
    Statistics statistics( pDev );
    // create an interface to the device found
    FunctionInterface fi( pDev );

    // Send all requests to the capture queue. There can be more than 1 queue for some devices, but for this sample
    // we will work with the default capture queue. If a device supports more than one capture or result
    // queue, this will be stated in the manual. If nothing is mentioned about it, the device supports one
    // queue only. This loop will send all requests currently available to the driver. To modify the number of requests
    // use the property mvIMPACT::acquire::SystemSettings::requestCount at runtime or the property
    // mvIMPACT::acquire::Device::defaultRequestCount BEFORE opening the device.
    TDMR_ERROR result = DMR_NO_ERROR;
    while( ( result = static_cast<TDMR_ERROR>( fi.imageRequestSingle() ) ) == DMR_NO_ERROR ) {};
    if( result != DEV_NO_FREE_REQUEST_AVAILABLE )
    {
        LockedScope lockedScope( g_critSect );
        cout << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << result
             << "(" << ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
    }

    manuallyStartAcquisitionIfNeeded( pDev, fi );
    // run thread loop
    const Request* pRequest = 0;
    const unsigned int timeout_ms = 500;
    int requestNr = INVALID_ID;
    // we always have to keep at least 2 images as the display module might want to repaint the image, thus we
    // can't free it unless we have a assigned the display to a new buffer.
    int lastRequestNr = INVALID_ID;
    unsigned int cnt = 0;
    while( !pThreadParameter->terminated() )
    {
        // wait for results from the default capture queue
        requestNr = fi.imageRequestWaitFor( timeout_ms );
        if( fi.isRequestNrValid( requestNr ) )
        {
            pRequest = fi.getRequest( requestNr );
            if( pRequest->isOK() )
            {
                ++cnt;
                // here we can display some statistical information every 100th image
                if( cnt % 100 == 0 )
                {
                    LockedScope lockedScope( g_critSect );
                    cout << "Info from " << pDev->serial.read()
                         << ": " << statistics.framesPerSecond.name() << ": " << statistics.framesPerSecond.readS()
                         << ", " << statistics.errorCount.name() << ": " << statistics.errorCount.readS()
                         << ", " << statistics.captureTime_s.name() << ": " << statistics.captureTime_s.readS() << endl;
                }
                display.SetImage( pRequest );
                display.Update();
            }
            else
            {
                LockedScope lockedScope( g_critSect );
                cout << "Error: " << pRequest->requestResult.readS() << endl;
            }
            if( fi.isRequestNrValid( lastRequestNr ) )
            {
                // this image has been displayed thus the buffer is no longer needed...
                fi.imageRequestUnlock( lastRequestNr );
            }
            lastRequestNr = requestNr;
            // send a new image request into the capture queue
            fi.imageRequestSingle();
        }
        //else
        //{
        //LockedScope lockedScope( g_critSect );
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
    manuallyStopAcquisitionIfNeeded( pDev, fi );

    // stop the display from showing freed memory
    display.RemoveImage();
    // In this sample all the next lines are redundant as the device driver will be
    // closed now, but in a real world application a thread like this might be started
    // several times an then it becomes crucial to clean up correctly.

    // free the last potentially locked request
    if( fi.isRequestNrValid( requestNr ) )
    {
        fi.imageRequestUnlock( requestNr );
    }
    // clear all queues
    fi.imageRequestReset( 0, 0 );
    return 0;
}

//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;
    const unsigned int devCnt = devMgr.deviceCount();
    if( devCnt == 0 )
    {
        cout << "No MATRIX VISION device found! Unable to continue!" << endl;
        return 1;
    }

    string productFilter( "*" );
    bool boInvalidCommandLineParameterDetected = false;
    // scan command line
    if( argc > 1 )
    {
        for( int i = 1; i < argc; i++ )
        {
            const string param( argv[i] );
            string::size_type keyEnd = param.find_first_of( "=" );
            if( ( keyEnd == string::npos ) || ( keyEnd == param.length() - 1 ) )
            {
                cout << "Invalid command line parameter: '" << param << "' (ignored)." << endl;
                boInvalidCommandLineParameterDetected = true;
            }
            else
            {
                const string key( param.substr( 0, keyEnd ) );
                if( ( key == "product" ) || ( key == "p" ) )
                {
                    productFilter = param.substr( keyEnd + 1 );
                }
                else
                {
                    cout << "Invalid command line parameter: '" << param << "' (ignored)." << endl;
                    boInvalidCommandLineParameterDetected = true;
                }
            }
        }
        if( boInvalidCommandLineParameterDetected )
        {
            displayCommandLineOptions();
        }
    }
    else
    {
        cout << "No command line parameters specified." << endl;
        displayCommandLineOptions();
    }

    // store all device infos in a vector
    // and start the execution of a 'live' thread for each device.
    std::vector<ThreadParameter*> threadParams;
    for( unsigned int i = 0; i < devCnt; i++ )
    {
        if( match( devMgr[i]->product.read(), productFilter, '*' ) == 0 )
        {
            // initialise display window
            // IMPORTANT: It's NOT safe to create multiple display windows in multiple threads!!!
            // Therefore this must be done before starting the threads
            string windowtitle( "mvIMPACT_acquire sample, Device " + devMgr[i]->serial.read() );
            // store thread parameter for this device in the vector
            threadParams.push_back( new ThreadParameter( devMgr[i], windowtitle ) );
            cout << devMgr[i]->family.read() << "(" << devMgr[i]->serial.read() << ")" << endl;
        }
    }

    if( threadParams.empty() )
    {
        cout << "No MATRIX VISION device found that matches the product filter '" << productFilter << "'! Unable to continue!" << endl;
        return 1;
    }

    size_t vSize = threadParams.size();
    HANDLE* pHandles = new HANDLE[vSize];
    // start live threads
    for( unsigned int j = 0; j < vSize; j++ )
    {
        unsigned int dwThreadID;
        pHandles[j] = ( HANDLE )_beginthreadex( 0, 0, liveThread, ( LPVOID )threadParams[j], 0, &dwThreadID );
    }

    // now all threads will start running...
    {
        LockedScope lockedScope( g_critSect );
        cout << "Press [ENTER] to end the acquisition( the initialisation of the devices might take some time )" << endl;
    }
    if( getchar() == EOF )
    {
        LockedScope lockedScope( g_critSect );
        cout << "'getchar()' did return EOF..." << endl;
    }

    // stop all threads again
    {
        LockedScope lockedScope( g_critSect );
        cout << "Terminating live threads..." << endl;
    }

    for( unsigned int k = 0; k < vSize; k++ )
    {
        threadParams[k]->terminateThread();
    }

    // wait until each live thread has terminated.
    WaitForMultipleObjects( static_cast<DWORD>( vSize ), pHandles, true, INFINITE );

    // free resources
    for( unsigned int l = 0; l < vSize; l++ )
    {
        CloseHandle( pHandles[l] );
        delete threadParams[l];
    }
    delete [] pHandles;

    return 0;
}
