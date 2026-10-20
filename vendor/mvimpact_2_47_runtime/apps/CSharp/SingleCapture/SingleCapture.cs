using System;

#if USE_DISPLAY
using mv.impact.acquire.display;
#endif // #if USE_DISPLAY
using mv.impact.acquire.examples.helper;

namespace mv.impact.acquire.examples
{
    class SingleCapture
    {
        static void Main(string[] args)
        {
            mv.impact.acquire.LibraryPath.init(); // this will add the folders containing unmanaged libraries to the PATH variable.
            Device pDev = DeviceAccess.getDeviceFromUserInput();
            if (pDev == null)
            {
                Console.WriteLine("Unable to continue!");
                Console.WriteLine("Press any key to end the program.");
                Console.Read();
                Environment.Exit(1);
            }

            Console.WriteLine("Initialising the device. This might take some time...");
            try
            {
                pDev.open();
            }
            catch (ImpactAcquireException e)
            {
                // this e.g. might happen if the same device is already opened in another process...
                Console.WriteLine("An error occurred while opening the device " + pDev.serial + "(error code: " + e.Message + "). Press any key to end the application...");
                Console.ReadLine();
                Environment.Exit(1);
            }

            // create an interface to the selected device
            FunctionInterface fi = new FunctionInterface(pDev);
            // send a request to the default request queue of the device and wait for the result.
            TDMR_ERROR result = (TDMR_ERROR)fi.imageRequestSingle();
            if (result != TDMR_ERROR.DMR_NO_ERROR)
            {
                Console.WriteLine("'FunctionInterface.imageRequestSingle' returned with an unexpected result: {0}({1})", result, ImpactAcquireException.getErrorCodeAsString(result));
            }

            DeviceAccess.manuallyStartAcquisitionIfNeeded(pDev, fi);
            // Wait for results from the default capture queue by passing a timeout (The maximum time allowed
            // for the application to wait for a Result). Infinity value: -1, positive value: The time to wait in milliseconds.
            // Please note that slow systems or interface technologies in combination with high resolution sensors
            // might need more time to transmit an image than the timeout value.
            // Once the device is configured for triggered image acquisition and the timeout elapsed before
            // the device has been triggered this might happen as well.
            // If waiting with an infinite timeout(-1) it will be necessary to call 'imageRequestReset' from another thread
            // to force 'imageRequestWaitFor' to return when no data is coming from the device/can be captured.
            int timeout_ms = 10000;
            // wait for results from the default capture queue
            int requestNr = fi.imageRequestWaitFor(timeout_ms);
            Request pRequest = fi.isRequestNrValid(requestNr) ? fi.getRequest(requestNr) : null;
            if (pRequest != null)
            {
                if (pRequest.isOK)
                {
                    // everything went well. Display the result
#if USE_DISPLAY
                    Console.WriteLine("Please note that there will be just one refresh for the display window, so if it is hidden under another window the result will not be visible.");
                    // initialise display window
                    ImageDisplayWindow window = new ImageDisplayWindow(String.Format("mvIMPACT_acquire sample, Device {0}", pDev.serial.read()));
#   if CLR_AT_LEAST_3_DOT_5
                    // Extension methods are not supported by CLR versions smaller than 3.5 and this next function
                    // is therefore not available then.
                    window.imageDisplay.SetImage(pRequest);
#   else
                    // If extension methods are not available, the following function can be used instead. It is
                    // not as convenient and will only work for some pixel formats. For more complex pixel formats
                    // use other overloads of this method
                    window.imageDisplay.SetImage(pRequest.imageData.read(), pRequest.imageWidth.read(), pRequest.imageHeight.read(), pRequest.imageBytesPerPixel.read() * 8, pRequest.imageLinePitch.read());
#   endif
                    window.imageDisplay.Update();
#endif // #if USE_DISPLAY
                    Console.WriteLine();
                    Console.WriteLine("Image captured: {0}({1}x{2})", pRequest.imagePixelFormat.readS(), pRequest.imageWidth.read(), pRequest.imageHeight.read());
                }
                else
                {
                    Console.WriteLine("Error: {0}", pRequest.requestResult.readS());
                    // if the application wouldn't terminate at this point this buffer HAS TO be unlocked before
                    // it can be used again as currently it is under control of the user. However terminating the application
                    // will free the resources anyway thus the call
                    // pRequest.unlock();
                    // could be omitted here.
                }

                // unlock the buffer to let the driver know that you no longer need this buffer.
                pRequest.unlock();
                Console.WriteLine();
                Console.WriteLine("Press [ENTER] to end the application");
                Console.ReadKey();
            }
            else
            {
                // If the error code is -2119(DEV_WAIT_FOR_REQUEST_FAILED), the documentation will provide
                // additional information under TDMR_ERROR in the interface reference
                Console.WriteLine("imageRequestWaitFor failed maybe the timeout value has been too small?");
            }
            DeviceAccess.manuallyStopAcquisitionIfNeeded(pDev, fi);
        }
    }
}
