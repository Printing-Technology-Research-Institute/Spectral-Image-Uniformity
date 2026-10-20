#ifdef _MSC_VER // is Microsoft compiler?
#   if _MSC_VER < 1300  // is 'old' VC 6 compiler?
#       pragma warning( disable : 4786 ) // 'identifier was truncated to '255' characters in the debug information'
#   endif // #if _MSC_VER < 1300
#endif // #ifdef _MSC_VER
#include <windows.h>
#include <process.h>
#include <iostream>
#include <limits>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#include <mvDisplay/Include/mvIMPACT_acquire_display.h>

#undef min // otherwise we can't work with the 'numeric_limits' template here as Windows defines a macro 'min'
#undef max // otherwise we can't work with the 'numeric_limits' template here as Windows defines a macro 'max'

// comment this line to see the effect of queue underruns
#define DROP_OUTDATED_IMAGES
// uncomment this line to get more output into the console window
//#define VERBOSE_OUTPUT

using namespace mvIMPACT::acquire;
using namespace mvIMPACT::acquire::display;
using namespace std;

static bool s_boTerminated = false;

//-----------------------------------------------------------------------------
struct ThreadParameter
//-----------------------------------------------------------------------------
{
    Device*             pDev;
    ImageDisplayWindow  displayWindow;
    ThreadParameter( Device* p, const string& windowTitle ) : pDev( p ), displayWindow( windowTitle ) {}
};

//-----------------------------------------------------------------------------
class TimeStampProvider
//-----------------------------------------------------------------------------
{
private:
    unsigned long long timestampTickFrequency_;
    mutable PropertyI64* pTimestamp_;
    Method* pTimestampLatch_;
public:
    explicit TimeStampProvider( Device* pDev ) : timestampTickFrequency_( 1000000000 ), pTimestamp_( 0 ), pTimestampLatch_( 0 )
    {
        GenICam::DeviceControl dc( pDev );
        GenICam::TransportLayerControl tlc( pDev );
        if( dc.timestamp.isValid() )
        {
            pTimestamp_ = new PropertyI64( dc.timestamp.hObj() );
        }
        else if( dc.timestampLatchValue.isValid() )
        {
            pTimestamp_ = new PropertyI64( dc.timestampLatchValue.hObj() );
        }
        else if( tlc.gevTimestampValue.isValid() )
        {
            pTimestamp_ = new PropertyI64( tlc.gevTimestampValue.hObj() );
            if( tlc.gevTimestampTickFrequency.isValid() )
            {
                timestampTickFrequency_ = tlc.gevTimestampTickFrequency.read();
            }
        }

        if( dc.timestampLatch.isValid() )
        {
            pTimestampLatch_ = new Method( dc.timestampLatch.hObj() );
        }
        else if( tlc.gevTimestampControlLatch.isValid() )
        {
            pTimestampLatch_ = new Method( tlc.gevTimestampControlLatch.hObj() );
        }

        if( dc.timestampReset.isValid() )
        {
            dc.timestampReset.call();
        }
        else if( tlc.gevTimestampControlReset.isValid() )
        {
            tlc.gevTimestampControlReset.call();
        }
    }
    ~TimeStampProvider()
    {
        delete pTimestamp_;
        delete pTimestampLatch_;
    }
    unsigned long long getTimestamp_us( void ) const
    {
        if( pTimestamp_ && pTimestampLatch_ && ( static_cast<TDMR_ERROR>( pTimestampLatch_->call() ) == DMR_NO_ERROR ) )
        {
            const double t = static_cast<double>( pTimestamp_->read() ) / static_cast<double>( timestampTickFrequency_ );
            return static_cast<unsigned long long>( t * 1000000 );
        }
        return numeric_limits<unsigned long long>::max();
    }
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
    FunctionInterface fi( pThreadParameter->pDev );

    TimeStampProvider* pTimestampProvider = 0;
    if( pThreadParameter->pDev->interfaceLayout.read() == dilGenICam )
    {
        pTimestampProvider = new TimeStampProvider( pThreadParameter->pDev );
    }

    // Send all requests to the capture queue. There can be more than 1 queue for some devices, but for this sample
    // we will work with the default capture queue. If a device supports more than one capture or result
    // queue, this will be stated in the manual. If nothing is said about it, the device supports one
    // queue only. This loop will send all requests currently available to the driver. To modify the number of requests
    // use the property mvIMPACT::acquire::SystemSettings::requestCount at runtime like so
    //     SystemSettings ss(pThreadParameter->pDev);
    //     ss.requestCount.write(10);
    // or the property mvIMPACT::acquire::Device::defaultRequestCount BEFORE opening the device.
    // Please note that reducing the 'requestCount' at runtime is only possible when the driver is current not holding
    // ANY request and EVERY request returned to the application has already been unlocked again.
    TDMR_ERROR result = DMR_NO_ERROR;
    unsigned long long previousTimestamp = 0;
    while( ( result = static_cast<TDMR_ERROR>( fi.imageRequestSingle() ) ) == DMR_NO_ERROR ) {};
    if( pTimestampProvider )
    {
        previousTimestamp = pTimestampProvider->getTimestamp_us();
    }
    if( result != DEV_NO_FREE_REQUEST_AVAILABLE )
    {
        cout << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << result
             << "(" << ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
    }

    manuallyStartAcquisitionIfNeeded( pThreadParameter->pDev, fi );
    // run thread loop
    int requestNr = INVALID_ID;
    Request* pRequest = 0;
    // we always have to keep at least 2 images as the displayWindow module might want to repaint the image, thus we
    // can not free request buffer unless we have assigned the displayWindow to a new buffer.
    Request* pPreviousRequest = 0;
    const unsigned int timeout_ms = 500;
    unsigned int cnt = 0;
    while( !s_boTerminated )
    {
        // Queue underruns can be simulated by switching of this section and maybe fine-tune the value of
        // the sleep time in the call to 'Sleep' below.
#ifdef DROP_OUTDATED_IMAGES
        // if results are queued because the image processing was to slow for the current framerate
        // get the newest valid image and throw away all others
        do
        {
            // get a queued result from the default capture queue without waiting(just check if there are pending results)
            requestNr = fi.imageRequestWaitFor( 0 );
            if( fi.isRequestNrValid( requestNr ) )
            {
                pRequest = fi.getRequest( requestNr );
                if( pRequest )
                {
#   ifdef VERBOSE_OUTPUT
                    cout << "Unlocking outdated request:            request_number=" << pRequest->getNumber() << "." << endl;
#   endif // #ifdef VERBOSE_OUTPUT
                    // discard an outdated buffer
                    pRequest->unlock();
                    // get timestamp to always have latest timestamp after dumping
                    if( pTimestampProvider )
                    {
                        previousTimestamp = pTimestampProvider->getTimestamp_us();
                    }
                    else
                    {
                        previousTimestamp = pPreviousRequest->infoTimeStamp_us.read();
                    }
                    // and send it to the driver again to request new data into it
                    fi.imageRequestSingle();
                }
                pRequest = 0;
            }
        }
        while( fi.isRequestNrValid( requestNr ) );
#endif // #ifdef DROP_OUTDATED_IMAGES

        if( !pRequest )
        {
            // there was no image in the queue we have to wait for results from the default capture queue
#ifdef VERBOSE_OUTPUT
            cout << "Waiting for a request to become ready:";
#endif // #ifdef VERBOSE_OUTPUT
            requestNr = fi.imageRequestWaitFor( timeout_ms );
            pRequest = fi.isRequestNrValid( requestNr ) ? fi.getRequest( requestNr ) : 0;
        }
#ifdef VERBOSE_OUTPUT
        else
        {
            cout << "Processing already received request:  ";
        }
        cout << " request_number=" << pRequest->getNumber() << " and timestamp=" << pRequest->infoTimeStamp_us.read() << " and last_timestamp=" << previousTimestamp << " and difference=" << ( signed int )( pRequest->infoTimeStamp_us.read() - previousTimestamp ) << "usec." << endl;
#endif // #ifdef VERBOSE_OUTPUT

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

                // simulates lengthy image processing time. Note that for devices or applications using a frame rate lower
                // than 5 fps this will have no effect. To simulate the desired behaviour of this example, this sleep time
                // needs to be longer than 1/frames per second!
                // Queue underruns will occur when e.g. the image processing time of your application (in this sample simulated
                // with \c sleep()) is longer than 1/frames per second as then new images will become ready faster than new
                // capture buffers will be sent to the driver. Applications might want to process the latest frame only then.
                // However it is crucial to understand that this still can only work, if the driver has enough requests
                // to stay busy during the image processing time. Then when picking up the latest frame and
                // re-queuing all the other buffer during that pickup operation the application will show the desired
                // behaviour. If not, queue underruns will continue to happen!
                // Note that mvBlueFOX3-* cameras might return outdated images under certain conditions even after image
                // dropping, as images are currently streamed out of the device only on user request, i.e. only if USB
                // buffers are provided from the host. This example can show this behaviour, if the sleep time is longer
                // than 1/frames per second, as this leads to the accumulation of "old" images in the framebuffer of the
                // mvBlueFOX3.
                Sleep( 200 );
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
            // this line is crucial for this application only. Without it buffers
            // will not be picked up in the correct way!
            pRequest = 0;
            // get timestamp at request time
            if( pTimestampProvider )
            {
                previousTimestamp = pTimestampProvider->getTimestamp_us();
            }
            else
            {
                previousTimestamp = pPreviousRequest->infoTimeStamp_us.read();
            }
            // send a new image request into the capture queue
            fi.imageRequestSingle();
        }
        else
        {
            // If the error code is -2119(DEV_WAIT_FOR_REQUEST_FAILED), the documentation will provide
            // additional information under TDMR_ERROR in the interface reference
            cout << "imageRequestWaitFor failed (" << requestNr << ", " << ImpactAcquireException::getErrorCodeAsString( requestNr ) << ")"
                 << ", timeout value too small?" << endl;
        }
    }
    manuallyStopAcquisitionIfNeeded( pThreadParameter->pDev, fi );

    // stop the displayWindow from showing freed memory
    display.RemoveImage();
    // In this sample all the next lines are redundant as the device driver will be
    // closed now, but in a real world application a thread like this might be started
    // several times an then it becomes crucial to clean up correctly.

    // free the last potentially locked request
    if( pPreviousRequest )
    {
        pPreviousRequest->unlock();
    }
    // clear all queues
    fi.imageRequestReset( 0, 0 );
    delete pTimestampProvider;
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
