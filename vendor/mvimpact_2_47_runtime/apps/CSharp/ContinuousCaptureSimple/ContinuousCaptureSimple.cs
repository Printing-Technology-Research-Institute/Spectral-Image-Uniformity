using System;

#if USE_DISPLAY
using mv.impact.acquire.display;
#endif // #if USE_DISPLAY
using mv.impact.acquire.examples.helper;
using mv.impact.acquire.helper;

namespace mv.impact.acquire.examples
{
    // A very simple listener that simply writes some information to
    // the console each time a request is reported as ready.
    public class MyConsoleRequestListener
    {
        public void requestReady(Object sender, RequestReadyEventArgs e)
        {
            // At the end of this scope the lock will automatically be freed.
            // Never directly call mv.impact.acquire.Request.unlock, when working with
            // mv.impact.acquire.hepler.RequestReadyEventArgs objects
            using (RequestReadyEventData data = e.data)
            {
                if (data.request.isOK)
                {
                    Console.WriteLine("Request {0} became ready. Image format: {1}({2}x{3}).", data.request.number, data.request.imagePixelFormat.readS(), data.request.imageWidth.read(), data.request.imageHeight.read());
                }
                else
                {
                    Console.WriteLine("Request {0} has been reported with status '{1}'.", data.request.number, data.request.requestResult.readS());
                }
            }
        }
    }

#if USE_DISPLAY
    // A more complex listener that draws the image in a display
    // window each time a request is reported as ready. The message handler
    // of this class will show how to handle situations, where a request cannot
    // be unlocked immediately as here the display might want to redraw the image
    // before a new request is reported as ready.
    public class MyDisplayRequestListener : ImageDisplayWindow
    {
        private RequestReadyEventData data_ = null;

        protected override void Dispose(bool disposing)
        {
            unlockData();
            base.Dispose(disposing);
        }

        public MyDisplayRequestListener(String title) : base(title) { }

        public void requestReady(Object sender, RequestReadyEventArgs e)
        {
            RequestReadyEventData data = e.data;
            if (data.request.isOK)
            {
#   if CLR_AT_LEAST_3_DOT_5
                // Extension methods are not supported by CLR versions smaller than 3.5 and this next function
                // is therefore not available then.
                imageDisplay.SetImage(data.request);
#   else
                // If extension methods are not available, the following function can be used instead. It is
                // not as convenient and will only work for some pixel formats. For more complex pixel formats
                // use other overloads of this method
                imageDisplay.SetImage(data.request.imageData.read(), data.request.imageWidth.read(), data.request.imageHeight.read(), data.request.imageBytesPerPixel.read() * 8, data.request.imageLinePitch.read());
#   endif
                imageDisplay.Update();
                if (data_ != null)
                {
                    // now it is save to unlock the OLD request that has been attached to the display previously.
                    // Never directly call mv.impact.acquire.Request.unlock, when working with
                    // mv.impact.acquire.hepler.RequestReadyEventArgs objects
                    data_.Dispose();
                }
                data_ = data;
            }
            else
            {
                Console.WriteLine("Request {0} has been reported with status '{1}'.", data.request.number, data.request.requestResult.readS());
                // Never directly call mv.impact.acquire.Request.unlock, when working with
                // mv.impact.acquire.hepler.RequestReadyEventArgs objects
                data.Dispose();
            }
        }

        public void unlockData()
        {
            if (data_ != null)
            {
                imageDisplay.SetImage(IntPtr.Zero, 0, 0, 0, 0);
                data_.Dispose();
                data_ = null;
            }
        }
    }
#endif // #if USE_DISPLAY

    class ContinuousCaptureSimple
    {
        static void Main(string[] args)
        {
            mv.impact.acquire.LibraryPath.init(); // this will add the folders containing unmanaged libraries to the PATH variable.
            Device pDev = DeviceAccess.getDeviceFromUserInput();
            if (pDev == null)
            {
                Console.WriteLine("Unable to continue! Press any key to end the program.");
                Console.Read();
                Environment.Exit(1);
            }

            try
            {
                RequestProvider rp = new RequestProvider(pDev);
                // register a very simple listener
                MyConsoleRequestListener consoleListener = new MyConsoleRequestListener();
                rp.onRequestReady += consoleListener.requestReady;
#if USE_DISPLAY
                // register another more complex listener to demonstrate that there can be
                // multiple listeners at the same time
                using (MyDisplayRequestListener displayListener = new MyDisplayRequestListener(String.Format("MyRequestListener for device {0}", pDev.serial.read())))
                {
                    rp.onRequestReady += displayListener.requestReady;
#endif // #if USE_DISPLAY
                    // When starting the acquisition for the first time the device will
                    // be intialised automatically
                    Console.WriteLine("Initialising the device. This might take some time...");
                    rp.acquisitionStart();
                    Console.WriteLine("Press [ENTER] to end the application");
                    Console.ReadLine();
                    rp.acquisitionStop();
#if USE_DISPLAY
                }
#endif // #if USE_DISPLAY
            }
            catch (ImpactAcquireException e)
            {
                Console.WriteLine("An error occurred while opening the device " + pDev.serial +
                    "(error code: " + e.Message + "). Press any key to end the application...");
                Console.ReadLine();
            }
        }
    }
}
