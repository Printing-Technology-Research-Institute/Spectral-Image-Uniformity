using System;
using System.Collections.Generic;
using System.Threading;

using mv.impact.acquire;
#if USE_DISPLAY
using mv.impact.acquire.display;
#endif // #if USE_DISPLAY
using mv.impact.acquire.examples.helper;

namespace CameraDescriptions
{
    class CameraDescriptions
    {
        static bool terminated_ = false;

        static void selectCameraDescriptionFromUserInput(CameraSettingsFrameGrabber cs)
        {
            // display the name of every camera description available for this device.
            // this might be less than the number of camera descriptions available on the system as e.g.
            // an analog frame grabber can't use descriptions for digital cameras
            List<KeyValuePair<string, int>> vAvailableDescriptions = new List<KeyValuePair<string, int>>();
            cs.type.getTranslationDict(vAvailableDescriptions);

            Console.WriteLine();
            Console.WriteLine("Available camera descriptions: {0}", vAvailableDescriptions.Count);
            Console.WriteLine("----------------------------------");
            foreach (KeyValuePair<string, int> kvp in vAvailableDescriptions)
            {
                Console.WriteLine(" {0} - {1}", kvp.Value.ToString(), kvp.Key.ToString());
            }
            Console.WriteLine("----------------------------------");
            Console.WriteLine();
            Console.WriteLine("Please select a camera description to use for this sample (numerical value): ");

            int cameraDescriptionToUse = Convert.ToInt32(Console.ReadLine());

            // wrong input will raise an exception
            // The camera description contains all the parameters needed to capture the associated video
            // signal. Camera descriptions for a certain camera can be obtained from MATRIX VISION. Experienced
            // users can also create their own camera descriptions using wxPropView. The documentation will
            // explain how to create and configure camera description files
            cs.type.write(vAvailableDescriptions[cameraDescriptionToUse].Value);
        }

        static void modifyConnectorFromUserInput(Connector connector)
        {
            bool boRun = true;
            while (boRun)
            {
                Console.WriteLine();
                Console.WriteLine("Current connector settings:");
                Console.WriteLine("  Camera output: {0}", connector.cameraOutputUsed.readS());
                Console.WriteLine("  Grabber input: {0}({1})", connector.videoChannel.read(), connector.pinDescription.read());
                Console.WriteLine("Press");
                Console.WriteLine("  'o' to select a different camera output");
                Console.WriteLine("  'i' to select a different grabber input");
                Console.WriteLine("  any other key when done");

                string cmd = Console.ReadLine();

                if (cmd == "o")
                {
                    Console.WriteLine("Available camera outputs as defined by the camera description:");
                    List<KeyValuePair<string, TCameraOutput>> lst = new List<KeyValuePair<string, TCameraOutput>>();
                    // query the properties translation dictionary, which will contain every camera description
                    // recognized by this device
                    connector.cameraOutputUsed.getTranslationDict(lst);
                    foreach (KeyValuePair<string, TCameraOutput> kvp in lst)
                    {
                        Console.WriteLine(" {0} - {1}", lst.IndexOf(kvp), kvp.Key.ToString());
                    }
                    Console.WriteLine("Enter the new (numerical) value: ");
                    int newOutput = Convert.ToInt32(Console.ReadLine());
                    try
                    {
                        connector.cameraOutputUsed.write(lst[newOutput].Value);
                    }
                    catch (ImpactAcquireException e)
                    {
                        Console.WriteLine(e.errorString);
                    }
                }
                else if (cmd == "i")
                {
                    // The allowed range depends on the currently selected camera output as e.g. for
                    // a RGB camera signal 3 video input are required on the device side, while a
                    // composite signal just requires 1.
                    Console.WriteLine("Allowed grabber video inputs in the current camera output mode: {0} - {1}", connector.videoChannel.minValue, connector.videoChannel.maxValue);
                    Console.WriteLine("Enter the new (numerical) value: ");
                    int newVideoInput = Convert.ToInt32(Console.ReadLine());
                    try
                    {
                        connector.videoChannel.write(newVideoInput);
                    }
                    catch (ImpactAcquireException e)
                    {
                        Console.WriteLine(e.errorString);
                    }
                }
                else
                {
                    boRun = false;
                    continue;
                }
            }
        }

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

            if (!pDev.hasCapability(TDeviceCapability.dcCameraDescriptionSupport))
            {
                Console.WriteLine("This sample is meant for devices that support camera descriptions only.");
                Console.WriteLine("Other devices might be installed but won't be recognized by the application.");
                Console.Read();
                Environment.Exit(1);
            }

            Console.WriteLine();
            Console.WriteLine("          Initialising the device. This might take some time...");
            Console.WriteLine();

            try
            {
                pDev.open();
            }
            catch (ImpactAcquireException e)
            {
                // this e.g. might happen if the same device is already opened in another process...
                Console.WriteLine("An error occurred while opening device " + pDev.serial +
                    "(error code: " + e.Message + "). Press any key to end the application...");
                Console.ReadLine();
                Environment.Exit(1);
            }

            // Assume that this is a frame grabber as currently only these devices support camera descriptions
            CameraSettingsFrameGrabber cs = new CameraSettingsFrameGrabber(pDev);
            if (!cs.type.isValid)
            {
                Console.WriteLine("Device {0} doesn't seem to support camera descriptions.", pDev.serial.readS());
                Console.WriteLine("Press [ENTER] to end the application");
                Console.ReadLine();
                Environment.Exit(1);
            }

            Connector connector = new Connector(pDev);
            CameraDescriptionManager cdm = new CameraDescriptionManager(pDev);
            bool boRun = true;
            while (boRun)
            {
                try
                {
                    terminated_ = false;
                    selectCameraDescriptionFromUserInput(cs);
                    modifyConnectorFromUserInput(connector);

                    // start the execution of the 'live' thread.
                    Console.WriteLine();
                    Console.WriteLine("Press [ENTER] to end the acquisition");
                    Console.WriteLine();
                    Console.WriteLine();

                    // IMPORTANT: It's NOT safe to create multiple display windows in multiple threads!!!
#if USE_DISPLAY
                    ImageDisplayWindow window = new ImageDisplayWindow(String.Format("Continuous Capture Window, Device {0}", pDev.serial.read()));
#endif // #if USE_DISPLAY
                    #region AcquisitionThread
                    Thread thread = new Thread(delegate ()
                    {

                        // establish access to the statistic properties
                        Statistics statistics = new Statistics(pDev);
                        // create an interface to the device found
                        FunctionInterface fi = new FunctionInterface(pDev);

                        // Send all requests to the capture queue. There can be more than 1 queue for some devices, but for this sample
                        // we will work with the default capture queue. If a device supports more than one capture or result
                        // queue, this will be stated in the manual. If nothing is mentioned about it, the device supports one
                        // queue only. This loop will send all requests currently available to the driver. To modify the number of requests
                        // use the property mv.impact.acquire.SystemSettings.requestCount at runtime or the property
                        // mv.impact.acquire.Device.defaultRequestCount BEFORE opening the device.
                        TDMR_ERROR result = TDMR_ERROR.DMR_NO_ERROR;
                        while ((result = (TDMR_ERROR)fi.imageRequestSingle()) == TDMR_ERROR.DMR_NO_ERROR) { };
                        if (result != TDMR_ERROR.DEV_NO_FREE_REQUEST_AVAILABLE)
                        {
                            Console.WriteLine("'FunctionInterface.imageRequestSingle' returned with an unexpected result: {0}({1})", result, ImpactAcquireException.getErrorCodeAsString(result));
                        }
                        DeviceAccess.manuallyStartAcquisitionIfNeeded(pDev, fi);

                        // run thread loop
                        Request pRequest = null;
                        // we always have to keep at least 2 images as the display module might want to repaint the image, thus we
                        // cannot free it unless we have a assigned the display to a new buffer.
                        Request pPreviousRequest = null;
                        int timeout_ms = 500;
                        int cnt = 0;
                        int requestNr = Device.INVALID_ID;
                        while (!terminated_)
                        {
                            // wait for results from the default capture queue
                            requestNr = fi.imageRequestWaitFor(timeout_ms);
                            pRequest = fi.isRequestNrValid(requestNr) ? fi.getRequest(requestNr) : null;
                            if (pRequest != null)
                            {
                                if (pRequest.isOK)
                                {
                                    ++cnt;
                                    // here we can display some statistical information every 10th image
                                    if (cnt % 10 == 0)
                                    {
                                        Console.Write("\r Statistics for {0}:\t{1} fps, {2} frames captured, {3} errors", pDev.serial.read(),
                                        statistics.framesPerSecond.readS(), statistics.frameCount.readS(), statistics.errorCount.readS());
                                    }
#if USE_DISPLAY
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
                                }
                                else
                                {
                                    Console.WriteLine("Error: {0}", pRequest.requestResult.readS());
                                }
                                if (pPreviousRequest != null)
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
                                // Console.WriteLine("imageRequestWaitFor failed ({0}, {1}), timeout value too small?", requestNr, ImpactAcquireException.getErrorCodeAsString(requestNr));
                            //}
                        }
                        DeviceAccess.manuallyStopAcquisitionIfNeeded(pDev, fi);

#if USE_DISPLAY
                        // stop the display from showing freed memory
                        window.imageDisplay.RemoveImage();
#endif // #if USE_DISPLAY
                        // In this sample all the next lines are redundant as the device driver will be
                        // closed now, but in a real world application a thread like this might be started
                        // several times an then it becomes crucial to clean up correctly.

                        // free the last potentially locked request
                        if (pRequest != null)
                        {
                            pRequest.unlock();
                        }
                        // clear all queues
                        fi.imageRequestReset(0, 0);
                    });
                    #endregion // AcquisitionThread
                    thread.Start();
                    Console.ReadKey();
                    terminated_ = true;
                    thread.Join();
#if USE_DISPLAY
                    window.Dispose();
#endif // #if USE_DISPLAY
                    Console.WriteLine();
                    Console.WriteLine("Press 'q' followed by [ENTER] to end to application or any other key followed by [ENTER] to select a different camera description: ");
                    string cmd;
                    cmd = Console.ReadLine();
                    if (cmd == "q")
                    {
                        boRun = false;
                        continue;
                    }
                }
                catch (ImpactAcquireException e)
                {
                    Console.WriteLine();
                    Console.WriteLine("Invalid selection(Error message: {0})", e.errorString);
                    Console.WriteLine("Please make a valid selection");
                }
            }

        }
    }
}
