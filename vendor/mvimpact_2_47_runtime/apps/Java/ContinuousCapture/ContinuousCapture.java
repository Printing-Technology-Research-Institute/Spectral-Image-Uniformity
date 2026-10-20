import mvIMPACT.acquire.*;

//-----------------------------------------------------------------------------
public class ContinuousCapture
//-----------------------------------------------------------------------------
{
    //=============================================================================
    //=== Static member functions =================================================
    //=============================================================================
    static
    {
        try
        {
            System.loadLibrary( "mvIMPACT_Acquire.java" );
        }
        catch( UnsatisfiedLinkError e )
        {
            System.err.println( "Native code library failed to load. Make sure the 'mvIMPACT_Acquire.java' library can be found in the systems search path.\n" + e );
            System.exit( 1 );
        }
    }

    //-----------------------------------------------------------------------------
    public static void main( String[] args )
    //-----------------------------------------------------------------------------
    {
        DeviceManager devMgr = new DeviceManager();
        Device pDev = mvIMPACT.acquire.examples.helper.DeviceAccess.getDeviceFromUserInput( devMgr );
        if( pDev == null )
        {
            System.out.print( "Unable to continue! " );
            mvIMPACT.acquire.examples.helper.DeviceAccess.waitForENTER();
            System.exit( 1 );
        }

        System.out.println( "Initialising the device. This might take some time..." );
        try
        {
            pDev.open();
        }
        catch( ImpactAcquireException e )
        {
            // this e.g. might happen if the same device is already opened in another process...
            System.out.println( "An error occurred while opening device " + pDev.getSerial().read() +
                                "(error code: " + e.getMessage() + ")." );
            mvIMPACT.acquire.examples.helper.DeviceAccess.waitForENTER();
            System.exit( 1 );
        }

        CaptureThread captureThread = new CaptureThread( pDev );
        captureThread.start();
        mvIMPACT.acquire.examples.helper.DeviceAccess.waitForENTER();
        captureThread.terminate();
        try
        {
            captureThread.join();
        }
        catch( Exception e )
        {
            System.out.println( e.getMessage() );
        }
    }
}