using System;
using System.Collections.Generic;
using System.Threading;
using mv.impact.acquire;
#if USE_DISPLAY
using mv.impact.acquire.display;
#endif // #if USE_DISPLAY
using mv.impact.acquire.examples.helper;


// Unlike the respective C++ example, no special measures have been taken to ensure
// "synchronised" console output by multiple threads. This is not necessary, since the
// Console.WriteLine() method of the .NET Framework guarantees thread-safety by itself.

namespace ContinuousCaptureAllDevices
{
    class ContinuousCaptureAllDevices
    {
        public class ThreadParameters
        {
            public Device pDev;
            public Thread thread;
#if USE_DISPLAY
            public ImageDisplayWindow window;
#endif // #if USE_DISPLAY
            public bool terminated = false;

            public ThreadParameters(Device Dev, string windowtitle)
            {
                pDev = Dev;
#if USE_DISPLAY
                window = new ImageDisplayWindow(windowtitle);
#endif // #if USE_DISPLAY
            }

            public void terminateThread()
            {
                terminated = true;
            }
        }

        public static void liveThread(Object pData)
        {
            ThreadParameters pThreadParameter = (ThreadParameters)pData;
            Device pDev = pThreadParameter.pDev;
            uint cnt = 0;
            Console.WriteLine("Trying to open {0}", pDev.serial.read());
            try
            {
                // if this device offers the 'GenICam' interface switch it on, as this will
                // allow are better control over GenICam compliant devices
                FeatureAccess.conditionalSetProperty(pDev.interfaceLayout, TDeviceInterfaceLayout.dilGenICam);
                // if this device offers a user defined acquisition start/stop behaviour
                // enable it as this allows finer control about the streaming behaviour
                FeatureAccess.conditionalSetProperty(pDev.acquisitionStartStopBehaviour, TAcquisitionStartStopBehaviour.assbUser);
                pDev.open();
            }
            catch (ImpactAcquireException e)
            {
                // this e.g. might happen if the same device is already opened in another process...
                Console.WriteLine("An error occurred while opening the device {0}", pDev.serial.read());
                Console.WriteLine("(error code: {0} ({1})). Terminating thread.", e.errorCode, e.errorCodeAsString);
                Console.WriteLine("Press [ENTER] to end the application...");
                return;
            }

            Console.WriteLine("Opened {0}", pDev.serial.read());
#if USE_DISPLAY
            ImageDisplay display = pThreadParameter.window.imageDisplay;
#endif // #if USE_DISPLAY
            // establish access to the statistic properties
            Statistics statistics = new Statistics(pDev);
            // create an interface to the device found
            FunctionInterface fi = new FunctionInterface(pDev);

            // Send all requests to the capture queue. There can be more than 1 queue for some devices, but for this sample
            // we will work with the default capture queue. If a device supports more than one capture or result
            // queue, this will be stated in the manual. If nothing is mentioned about it, the device supports one
            // queue only. This loop will send all requests currently available to the driver. To modify the number of requests
            // use the property mvIMPACT::acquire::SystemSettings::requestCount at runtime or the property
            // mvIMPACT::acquire::Device::defaultRequestCount BEFORE opening the device.
            TDMR_ERROR result = TDMR_ERROR.DMR_NO_ERROR;
            while ((result = (TDMR_ERROR)fi.imageRequestSingle()) == TDMR_ERROR.DMR_NO_ERROR) { };
            if (result != TDMR_ERROR.DEV_NO_FREE_REQUEST_AVAILABLE)
            {
                Console.WriteLine("'FunctionInterface.imageRequestSingle' returned with an unexpected result: {0}", result);
                Console.WriteLine("({0})", ImpactAcquireException.getErrorCodeAsString(result));
            }

            DeviceAccess.manuallyStartAcquisitionIfNeeded(pDev, fi);
            // run thread loop
            Request pRequest = null;
            int timeout_ms = 500;
            int requestNr = -1;
            // we always have to keep at least 2 images as the display module might want to repaint the image, thus we
            // can't free it unless we have a assigned the display to a new buffer.
            int lastRequestNr = -1;
            while (!pThreadParameter.terminated)
            {
                // wait for results from the default capture queue
                requestNr = fi.imageRequestWaitFor(timeout_ms);
                if (fi.isRequestNrValid(requestNr))
                {
                    pRequest = fi.getRequest(requestNr);
                    if (pRequest.isOK)
                    {
                        ++cnt;
                        // here we can display some statistical information every 100th image
                        if (cnt % 100 == 0)
                        {
                            Console.WriteLine("Info from {0}: {1}: {2}, {3}: {4}, {5}: {6}", pDev.serial.read(),
                            statistics.framesPerSecond.name, statistics.framesPerSecond.readS(),
                            statistics.errorCount.name, statistics.errorCount.readS(),
                            statistics.captureTime_s.name, statistics.captureTime_s.readS());
                        }
#if USE_DISPLAY
#   if CLR_AT_LEAST_3_DOT_5
                        // Extension methods are not supported by CLR versions smaller than 3.5 and this next function
                        // is therefore not available then.
                        display.SetImage(pRequest);
#   else
                        // If extension methods are not available, the following function can be used instead. It is
                        // not as convenient and will only work for some pixel formats. For more complex pixel formats
                        // use other overloads of this method
                        display.SetImage(pRequest.imageData.read(), pRequest.imageWidth.read(), pRequest.imageHeight.read(), pRequest.imageBytesPerPixel.read() * 8, pRequest.imageLinePitch.read());
#   endif
                        display.Update();
#endif // #if USE_DISPLAY
                    }
                    else
                    {
                        Console.WriteLine("Error: {0}", pRequest.requestResult.readS());
                    }
                    if (fi.isRequestNrValid(lastRequestNr))
                    {
                        // this image has been displayed thus the buffer is no longer needed...
                        fi.imageRequestUnlock(lastRequestNr);
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
                // Console.WriteLine("imageRequestWaitFor failed ({0}, {1}), timeout value too small?", requestNr, ImpactAcquireException.getErrorCodeAsString(requestNr));
                //}
            }
            DeviceAccess.manuallyStopAcquisitionIfNeeded(pDev, fi);

#if USE_DISPLAY
            // stop the display from showing freed memory
            display.RemoveImage();
#endif // #if USE_DISPLAY
            // In this sample all the next lines are redundant as the device driver will be
            // closed now, but in a real world application a thread like this might be started
            // several times an then it becomes crucial to clean up correctly.

            // free the last potentially locked request
            if (fi.isRequestNrValid(requestNr))
            {
                fi.imageRequestUnlock(requestNr);
            }
            // clear all queues
            fi.imageRequestReset(0, 0);
        }

        static void Main(string[] args)
        {
            mv.impact.acquire.LibraryPath.init(); // this will add the folders containing unmanaged libraries to the PATH variable.
            if (DeviceManager.deviceCount < 1)
            {
                Console.WriteLine("Unable to continue! Press any key to end the program.");
                Console.Read();
                Environment.Exit(1);
            }

            string productFilter = "";

            // scan command line
            if (args.Length == 1)
            {
                for (int i = 0; i < args.Length; i++)
                {
                    string param = args[i];
                    int keyEnd = param.IndexOf('=');
                    if ((!param.Contains("=")) || (keyEnd == param.Length))
                    {
                        Console.WriteLine("Invalid command line parameter: '{0}' (ignored).", param);
                    }
                    else
                    {
                        string key = param.Substring(0, keyEnd);
                        string value = param.Substring(keyEnd + 1);
                        if ((key == "product") || (key == "p"))
                        {
                            productFilter = value;
                        }
                        else
                        {
                            Console.WriteLine("Invalid command line parameter: '{0}' (ignored).", param);
                        }
                    }
                }
            }
            else
            {
                Console.WriteLine("No command line parameters specified. Available parameters:");
                Console.WriteLine("  'product' or 'p' to specify a certain product type. All other products will be ignored then");
                Console.WriteLine("      a '*' serves as a wildcard.");
                Console.WriteLine();
                Console.WriteLine("USAGE EXAMPLE:");
                Console.WriteLine("  ContinuousCaptureAllDevices p=mvBlue* ");
                Console.WriteLine();
            }

            // store all device infos in a vector
            // and start the execution of a 'live' thread for each device.
            List<ThreadParameters> threadParams = new List<ThreadParameters>();
            for (int i = 0; i < DeviceManager.deviceCount; i++)
            {
                if (DeviceManager.deviceList[i].product.read().Contains(productFilter))
                {
                    // initialise display window
                    // IMPORTANT: It's NOT safe to create multiple display windows in multiple threads!!!
                    // Therefore this must be done before starting the threads
                    string windowtitle = String.Concat("mvIMPACT_acquire sample, Device " + DeviceManager.deviceList[i].serial.read());
                    // store thread parameter for this device in the vector
                    threadParams.Add(new ThreadParameters(DeviceManager.deviceList[i], windowtitle));
                    Console.WriteLine("{0} ({1})", DeviceManager.deviceList[i].family.read(), DeviceManager.deviceList[i].serial.read());
                }
            }

            if (threadParams.Count == 0)
            {
                Console.WriteLine("No MATRIX VISION device found that matches the product filter '{0}'! Unable to continue!", productFilter);
                Environment.Exit(1);
            }

            // start live threads
            foreach (ThreadParameters p in threadParams)
            {
                p.thread = new Thread(ContinuousCaptureAllDevices.liveThread);
                p.thread.Start(p);
            }

            // now all threads will start running...
            Console.WriteLine("Press [ENTER] to start the acquisition( the initialisation of the devices might take some time )");
            Console.ReadKey();

            // stop all threads again
            Console.WriteLine("Terminating live threads...");

            // wait until each live thread has terminated.
            foreach (ThreadParameters p in threadParams)
            {
                p.terminated = true;
                p.thread.Join();
            }
        }
    }
}