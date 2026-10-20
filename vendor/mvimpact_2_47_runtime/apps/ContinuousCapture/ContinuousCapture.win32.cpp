#ifdef _MSC_VER // is Microsoft compiler?
#   if _MSC_VER < 1300  // is 'old' VC 6 compiler?
#       pragma warning( disable : 4786 ) // 'identifier was truncated to '255' characters in the debug information'
#   endif // #if _MSC_VER < 1300
#endif // #ifdef _MSC_VER
#include <windows.h>
#include <process.h>
#include <iostream>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvDisplay/Include/mvIMPACT_acquire_display.h>

using namespace mvIMPACT::acquire;
using namespace mvIMPACT::acquire::display;
using namespace std;

static bool s_boTerminated = false;

//=============================================================================
//================= Data type definitions =====================================
//=============================================================================
//-----------------------------------------------------------------------------
struct ThreadParameter
//-----------------------------------------------------------------------------
{
    Device*             pDev;
    ImageDisplayWindow  displayWindow;
    ThreadParameter( Device* p, const string& windowTitle ) : pDev( p ), displayWindow( windowTitle ) {}
};

//-----------------------------------------------------------------------------
unsigned int __stdcall liveThread( void* pData )
//-----------------------------------------------------------------------------
{
    ThreadParameter* pThreadParameter = reinterpret_cast<ThreadParameter*>( pData );
    cout << "Initialising the device. This might take some time..." << endl;
    try
    {
        pThreadParameter->pDev->open();
    }
    catch( const ImpactAcquireException& e )
    {
        // this e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while opening the device " << pThreadParameter->pDev->serial.read()
             << "(error code: " << e.getErrorCodeAsString() << ")." << endl
             << "Press [ENTER] to end the application..." << endl;
        cin.get();
        return 0;
    }

    ImageDisplay& display = pThreadParameter->displayWindow.GetImageDisplay();
    // establish access to the statistic properties
    Statistics statistics( pThreadParameter->pDev );
    // create an interface to the device found
    mvIMPACT::acquire::FunctionInterface fi( pThreadParameter->pDev );

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
        cout << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << result
             << "(" << mvIMPACT::acquire::ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
    }

    manuallyStartAcquisitionIfNeeded( pThreadParameter->pDev, fi );
    // run thread loop
    mvIMPACT::acquire::Request* pRequest = 0;
    // we always have to keep at least 2 images as the displayWindow module might want to repaint the image, thus we
    // can free it unless we have a assigned the displayWindow to a new buffer.
    mvIMPACT::acquire::Request* pPreviousRequest = 0;
    const unsigned int timeout_ms = 500;
    unsigned int cnt = 0;
    while( !s_boTerminated )
    {
        // wait for results from the default capture queue
        int requestNr = fi.imageRequestWaitFor( timeout_ms );
        pRequest = fi.isRequestNrValid( requestNr ) ? fi.getRequest( requestNr ) : 0;
        if( pRequest )
        {
            if( pRequest->isOK() )
            {
                ++cnt;
                // here we can display some statistical information every 100th image
                if( cnt % 100 == 0 )
                {
                    cout << "Info from " << pThreadParameter->pDev->serial.read()
                         << ": " << statistics.framesPerSecond.name() << ": " << statistics.framesPerSecond.readS()
                         << ", " << statistics.errorCount.name() << ": " << statistics.errorCount.readS()
                         << ", " << statistics.captureTime_s.name() << ": " << statistics.captureTime_s.readS() << endl;
                }
                display.SetImage( pRequest );
                display.Update();
            }
            else
            {
                cout << "Error: " << pRequest->requestResult.readS() << endl;
            }
            if( pPreviousRequest )
            {
                // this image has been displayed thus the buffer is no longer needed...
                pPreviousRequest->unlock();
            }
            pPreviousRequest = pRequest;
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
    manuallyStopAcquisitionIfNeeded( pThreadParameter->pDev, fi );

    // stop the displayWindow from showing freed memory
    display.RemoveImage();
    // In this sample all the next lines are redundant as the device driver will be
    // closed now, but in a real world application a thread like this might be started
    // several times an then it becomes crucial to clean up correctly.

    // free the last potentially locked request
    if( pRequest )
    {
        pRequest->unlock();
    }
    // clear all queues
    fi.imageRequestReset( 0, 0 );
    return 0;
}

//-----------------------------------------------------------------------------
int main( void )
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;
    Device* pDev = getDeviceFromUserInput( devMgr );
    if( !pDev )
    {
        cout << "Unable to continue! Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    // start the execution of the 'live' thread.
    cout << "Press [ENTER] to end the application" << endl;
    unsigned int dwThreadID;
    string windowTitle( "mvIMPACT_acquire sample, Device " + pDev->serial.read() );
    // initialise displayWindow window
    // IMPORTANT: It's NOT safe to create multiple displayWindow windows in multiple threads!!!
    ThreadParameter threadParam( pDev, windowTitle );
    HANDLE hThread = ( HANDLE )_beginthreadex( 0, 0, liveThread, ( LPVOID )( &threadParam ), 0, &dwThreadID );
    cin.get();
    s_boTerminated = true;
    WaitForSingleObject( hThread, INFINITE );
    CloseHandle( hThread );
    return 0;
}
