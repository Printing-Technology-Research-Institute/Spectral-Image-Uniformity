import mvIMPACT.acquire.*;

public class CaptureThread extends Thread
{
    //=============================================================================
    //=== private static member variables =========================================
    //=============================================================================
    private static final boolean isWindows_ = System.getProperty( "os.name" ).startsWith( "Windows" );

    //=============================================================================
    //=== private member variables ================================================
    //=============================================================================
    private Device pDev_;
    //private ImageDisplayWindow window_; // Windows only but there is no such thing as #ifdef in Java!
    private boolean terminated_ = false;

    //=============================================================================
    //=== public constructor ======================================================
    //=============================================================================
    //-----------------------------------------------------------------------------
    public CaptureThread( Device pDev )
    //-----------------------------------------------------------------------------
    {
        pDev_ = pDev;
        //window_ = new ImageDisplayWindow( "mvIMPACT_acquire sample, Device " + pDev.getSerial().read() );
    }

    //=============================================================================
    //=== public member functions =================================================
    //=============================================================================
    //-----------------------------------------------------------------------------
    public void run()
    //-----------------------------------------------------------------------------
    {
        if( isWindows_ )
        {
            System.out.println( "\n\nSince you are running on a Windows platform you could use mvIMPACT Acquire's display module. To try this simply remove the comments wherever the 'window_' variable is used. As Java does not support something like '#ifdef' we did not come up with anything smarter. Suggestions welcome!!!\n\n" );
        }

        // establish access to the statistic properties
        Statistics statistics = new Statistics( pDev_ );
        // create an interface to the device found
        FunctionInterface fi = new FunctionInterface( pDev_ );

        // Send all requests to the capture queue. There can be more than 1 queue for some devices, but for this sample
        // we will work with the default capture queue. If a device supports more than one capture or result
        // queue, this will be stated in the manual. If nothing is mentioned about it, the device supports one
        // queue only. This loop will send all requests currently available to the driver. To modify the number of requests
        // use the property mvIMPACT.acquire.SystemSettings.requestCount at runtime or the property
        // mvIMPACT.acquire.Device.defaultRequestCount BEFORE opening the device.
        int result = TDMR_ERROR.DMR_NO_ERROR;
        while( ( result = fi.imageRequestSingle() ) == TDMR_ERROR.DMR_NO_ERROR ) { };
        if( result != TDMR_ERROR.DEV_NO_FREE_REQUEST_AVAILABLE )
        {
            System.out.println( String.format( "'FunctionInterface.imageRequestSingle' returned with an unexpected result: %d(%s)", result, ImpactAcquireException.getErrorCodeAsString( result ) ) );
        }

        mvIMPACT.acquire.examples.helper.DeviceAccess.manuallyStartAcquisitionIfNeeded( pDev_, fi );
        // run thread loop
        Request pRequest = null;
        // we always have to keep at least 2 images as the display module might want to repaint the image, thus we
        // cannot free it unless we have a assigned the display to a new buffer.
        Request pPreviousRequest = null;
        int timeout_ms = 500;
        int cnt = 0;
        int requestNr = acquire.getINVALID_ID();
        while( !terminated_ )
        {
            // wait for results from the default capture queue
            requestNr = fi.imageRequestWaitFor( timeout_ms );
            pRequest = fi.isRequestNrValid( requestNr ) ? fi.getRequest( requestNr ) : null;
            if( pRequest != null )
            {
                if( pRequest.isOK() )
                {
                    ++cnt;
                    // here we can display some statistical information every 100th image
                    if( cnt % 100 == 0 )
                    {
                        System.out.println( String.format( "Info from %s: %s: %s, %s: %s, %s: %s", pDev_.getSerial().read(),
                                                           statistics.getFramesPerSecond().name(), statistics.getFramesPerSecond().readS(),
                                                           statistics.getErrorCount().name(), statistics.getErrorCount().readS(),
                                                           statistics.getCaptureTime_s().name(), statistics.getCaptureTime_s().readS() ) );
                    }
                    //window_.GetImageDisplay().SetImage( pRequest );
                    //window_.GetImageDisplay().Update();
                }
                else
                {
                    System.out.println( "Error: " + pRequest.getRequestResult().readS() );
                }
                if( pPreviousRequest != null )
                {
                    // this image has been displayed thus the buffer is no longer needed...
                    pPreviousRequest.unlock();
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
            // System.out.println("imageRequestWaitFor failed (%d, %s), timeout value too small?", requestNr, ImpactAcquireException.getErrorCodeAsString(requestNr));
            //}
        }
        mvIMPACT.acquire.examples.helper.DeviceAccess.manuallyStopAcquisitionIfNeeded( pDev_, fi );

        // stop the display from showing freed memory
        //window_.GetImageDisplay().RemoveImage();

        // In this sample all the next lines are redundant as the device driver will be
        // closed now, but in a real world application a thread like this might be started
        // several times an then it becomes crucial to clean up correctly.

        // free the last potentially locked request
        if( pRequest != null )
        {
            pRequest.unlock();
        }
        // clear all queues
        fi.imageRequestReset( 0, 0 );
    }

    //-----------------------------------------------------------------------------
    public void terminate()
    //-----------------------------------------------------------------------------
    {
        terminated_ = true;
    }
}