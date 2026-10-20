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

using namespace std;
using namespace mvIMPACT::acquire;
using namespace mvIMPACT::acquire::display;

static bool s_boTerminated = false;

//-----------------------------------------------------------------------------
struct ThreadParameter
//-----------------------------------------------------------------------------
{
    Device*             pDev;
    ImageDisplayWindow  dispRaw;
    ImageDisplayWindow  dispConverted;
    ThreadParameter( Device* p, const string& windowTitle ) : pDev( p ), dispRaw( windowTitle ), dispConverted( windowTitle + "(converted)" ) {}
};

//-----------------------------------------------------------------------------
unsigned int __stdcall liveThread( void* pData )
//-----------------------------------------------------------------------------
{
    ThreadParameter* pThreadParameter = reinterpret_cast<ThreadParameter*>( pData );
    unsigned int cnt = 0;

    // establish access to the statistic properties
    Statistics statistics( pThreadParameter->pDev );
    // create an interface to the device found
    FunctionInterface fi( pThreadParameter->pDev );

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
             << "(" << ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
    }

    manuallyStartAcquisitionIfNeeded( pThreadParameter->pDev, fi );
    // run thread loop
    const Request* pRequest = 0;
    const unsigned int timeout_ms = 500;
    int requestNr = INVALID_ID;
    // we always have to keep at least 2 images as the display module might want to repaint the image, thus we
    // cannot free it unless we have a assigned the display to a new buffer.
    int lastRequestNr = INVALID_ID;
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
                    cout << "Info from " << pThreadParameter->pDev->serial.read()
                         << ": " << statistics.framesPerSecond.name() << ": " << statistics.framesPerSecond.readS()
                         << ", " << statistics.errorCount.name() << ": " << statistics.errorCount.readS()
                         << ", " << statistics.captureTime_s.name() << ": " << statistics.captureTime_s.readS()
                         << ", " << pRequest->requestResult.readS() << endl;
                }
                // raw (this might not always display a correct image but is fast and for certain applications the raw data will be the data to process
                pThreadParameter->dispRaw.GetImageDisplay().SetImage( pRequest->imageData.read(), pRequest->imageWidth.read(), pRequest->imageHeight.read(), pRequest->imagePixelPitch.read() * 8, pRequest->imageLinePitch.read() );
                pThreadParameter->dispRaw.GetImageDisplay().Update();

                // converted. This might be slower as conversions might be needed but will display a correct image (that is not necessarily the data
                // transferred from the device) e.g. 12 bit mono data can't be displayed directly
                pThreadParameter->dispConverted.GetImageDisplay().SetImage( pRequest );
                pThreadParameter->dispConverted.GetImageDisplay().Update();
            }
            else
            {
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

    // stop the displays from showing freed memory
    pThreadParameter->dispRaw.GetImageDisplay().RemoveImage();
    pThreadParameter->dispConverted.GetImageDisplay().RemoveImage();
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

    cout << "Initialising the device. This might take some time..." << endl;
    try
    {
        pDev->open();
    }
    catch( const ImpactAcquireException& e )
    {
        // this e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while opening device " << pDev->serial.read()
             << "(error code: " << e.getErrorCodeAsString() << ")." << endl
             << "Press [ENTER] to end the application..." << endl;
        cin.get();
        return 1;
    }

    cout << "Please note that only the window displaying the converted image will display correct" << endl
         << "images all the time. The other window will simply blit the raw image data in the client" << endl
         << "area. This is faster but e.g. for 12 bit mono data or YUV data will result in unexpected" << endl
         << "results. For planar formats e.g. just the first plane might be visible." << endl
         << "For certain other formats the unconverted window might even blit no data at all." << endl
         << "This e.g. will be the case for 10-16 bit RGB packed data." << endl;

    ImageDestination id( pDev );
    unsigned int pixelFormats = id.pixelFormat.dictSize();

    // display all available pixel formats and there numerical representation.
    cout << "The following formats are available:" << endl;
    for( unsigned int i = 0; i < pixelFormats; i++ )
    {
        cout << "[" << id.pixelFormat.getTranslationDictValue( i ) << "]: " << id.pixelFormat.getTranslationDictString( i ) << endl;
    }

    for( unsigned int j = 0; j < pixelFormats; j++ )
    {
        s_boTerminated = false;
        id.pixelFormat.write( id.pixelFormat.getTranslationDictValue( j ) );
        // initialise display windows
        // IMPORTANT: It's NOT safe to create multiple display windows in multiple threads!!!
        const string windowTitle( "Device " + pDev->serial.read() + "(pixel format " + id.pixelFormat.getTranslationDictString( j ) + ")" );
        cout << "Currently using " << id.pixelFormat.getTranslationDictString( j ) << ". Press [ENTER] to switch to the next format" << endl;
        unsigned int dwThreadID;
        ThreadParameter threadParam( pDev, windowTitle );
        // start the execution of the 'live' thread.
        HANDLE hThread = ( HANDLE )_beginthreadex( 0, 0, liveThread, ( LPVOID )( &threadParam ), 0, &dwThreadID );
        cin.get();
        s_boTerminated = true;
        WaitForSingleObject( hThread, INFINITE );
        CloseHandle( hThread );
    }
    cout << "Press [ENTER] to end the application" << endl;
    cin.get();
    return 0;
}
