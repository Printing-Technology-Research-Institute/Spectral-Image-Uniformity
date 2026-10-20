#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#ifdef _WIN32
#   define USE_DISPLAY
#   include <mvDisplay/Include/mvIMPACT_acquire_display.h>
using namespace mvIMPACT::acquire::display;
#endif // #ifdef _WIN32

using namespace std;
using namespace mvIMPACT::acquire;

static mutex s_mutex;
static bool s_boTerminated = false;

//-----------------------------------------------------------------------------
class ThreadParameter
//-----------------------------------------------------------------------------
{
    Device*             pDev_;
#ifdef USE_DISPLAY
    unique_ptr<ImageDisplayWindow> pDisplayWindow_;
#endif // #ifdef USE_DISPLAY
public:
    explicit ThreadParameter( Device* pDev ) : pDev_( pDev ) {}
    ThreadParameter( const ThreadParameter& src ) = delete;
    Device* device( void ) const
    {
        return pDev_;
    }
#ifdef USE_DISPLAY
    void createDisplayWindow( const string& windowTitle )
    {
        pDisplayWindow_ = unique_ptr<ImageDisplayWindow>( new ImageDisplayWindow( windowTitle ) );
    }
    ImageDisplayWindow& displayWindow( void )
    {
        return *pDisplayWindow_;
    }
#endif // #ifdef USE_DISPLAY
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
void writeToStdout( const string& msg )
//-----------------------------------------------------------------------------
{
    lock_guard<mutex> lockedScope( s_mutex );
    cout << msg << endl;
}

//-----------------------------------------------------------------------------
void liveThread( shared_ptr<ThreadParameter> parameter )
//-----------------------------------------------------------------------------
{
    Device* pDev = parameter->device();

    writeToStdout( "Trying to open " + pDev->serial.read() );
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
        lock_guard<mutex> lockedScope( s_mutex );
        // this e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while opening the device " << pDev->serial.read()
             << "(error code: " << e.getErrorCode() << "(" << e.getErrorCodeAsString() << ")). Terminating thread." << endl
             << "Press [ENTER] to end the application..."
             << endl;
        return;
    }

    writeToStdout( "Opened " + pDev->serial.read() );

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
        lock_guard<mutex> lockedScope( s_mutex );
        cout << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << result
             << "(" << ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
    }

    manuallyStartAcquisitionIfNeeded( pDev, fi );
    // run thread loop
    const Request* pRequest = nullptr;
    const unsigned int timeout_ms = {500};
    int requestNr = INVALID_ID;
    // we always have to keep at least 2 images as the display module might want to repaint the image, thus we
    // can't free it unless we have a assigned the display to a new buffer.
    int lastRequestNr = {INVALID_ID};
    unsigned int cnt = {0};
    while( !s_boTerminated )
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
                    lock_guard<mutex> lockedScope( s_mutex );
                    cout << "Info from " << pDev->serial.read()
                         << ": " << statistics.framesPerSecond.name() << ": " << statistics.framesPerSecond.readS()
                         << ": " << statistics.bandwidthConsumed.name() << ": " << statistics.bandwidthConsumed.readS()
                         << ", " << statistics.errorCount.name() << ": " << statistics.errorCount.readS()
                         << ", " << statistics.captureTime_s.name() << ": " << statistics.captureTime_s.readS() << endl;
                }
#ifdef USE_DISPLAY
                ImageDisplay& display = parameter->displayWindow().GetImageDisplay();
                display.SetImage( pRequest );
                display.Update();
#endif // #ifdef USE_DISPLAY
            }
            else
            {
                writeToStdout( "Error: " + pRequest->requestResult.readS() );
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

#ifdef USE_DISPLAY
    // stop the display from showing freed memory
    parameter->displayWindow().GetImageDisplay().RemoveImage();
#endif // #ifdef USE_DISPLAY
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
    // scan command line
    if( argc > 1 )
    {
        bool boInvalidCommandLineParameterDetected = false;
        for( int i = 1; i < argc; i++ )
        {
            const string param( argv[i] );
            const auto keyEnd = param.find_first_of( "=" );
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
    map<shared_ptr<ThreadParameter>, shared_ptr<thread>> threads;
    for( unsigned int i = 0; i < devCnt; i++ )
    {
        if( match( devMgr[i]->product.read(), productFilter, '*' ) == 0 )
        {
            shared_ptr<ThreadParameter> pParameter = make_shared<ThreadParameter>( devMgr[i] );
#ifdef USE_DISPLAY
            // IMPORTANT: It's NOT safe to create multiple display windows in multiple threads!!!
            // Therefore this must be done before starting the threads
            pParameter->createDisplayWindow( "mvIMPACT_acquire sample, Device " + devMgr[i]->serial.read() );
#endif // #ifdef USE_DISPLAY
            threads[pParameter] = make_shared<thread>( liveThread, pParameter );
            writeToStdout( devMgr[i]->family.read() + "(" + devMgr[i]->serial.read() + ")" );
        }
    }

    if( threads.empty() )
    {
        cout << "No MATRIX VISION device found that matches the product filter '" << productFilter << "'! Unable to continue!" << endl;
        return 1;
    }

    // now all threads will start running...
    writeToStdout( "Press [ENTER] to end the acquisition( the initialisation of the devices might take some time )" );

    if( getchar() == EOF )
    {
        writeToStdout( "'getchar()' did return EOF..." );
    }

    // stop all threads again
    writeToStdout( "Terminating live threads..." );
    s_boTerminated = true;
    for( auto& it : threads )
    {
        it.second->join();
    }

    return 0;
}
