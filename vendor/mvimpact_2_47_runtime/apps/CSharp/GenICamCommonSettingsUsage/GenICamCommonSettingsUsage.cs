using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Threading;

#if USE_DISPLAY
using mv.impact.acquire.display;
#endif // #if USE_DISPLAY
using mv.impact.acquire.examples.helper;
using mv.impact.acquire.GenICam;

namespace mv.impact.acquire.examples
{
    class GenICamInterfaceLayout
    {
        private static bool canRestoreFactoryDefault(Device pDev)
        {
            GenICam.UserSetControl usc = new GenICam.UserSetControl(pDev);
            if (!usc.userSetSelector.isValid || !usc.userSetLoad.isValid)
            {
                return false;
            }

            ReadOnlyCollection<String> validUserSetSelectorStrings = usc.userSetSelector.listOfValidStrings;
            if (!validUserSetSelectorStrings.Contains("Default"))
            {
                return false;
            }

            return true;
        }
        static void Main(string[] args)
        {
            mv.impact.acquire.LibraryPath.init(); // this will add the folders containing unmanaged libraries to the PATH variable.
            Console.WriteLine("--------------------------------------------!!! ATTENTION !!!--------------------------------------------");
            Console.WriteLine("Please be aware that the digital outputs of the camera might be enabled during the test. This might lead to unexpected behavior in case of devices which are connected to one of the digital outputs, so only proceed if you are sure that this will not cause any issue with connected hardware!");
            Console.WriteLine("---------------------------------------------------------------------------------------------------------");
            Console.WriteLine();
            Device pDev = DeviceAccess.getDeviceFromUserInput();
            if (pDev == null)
            {
                Console.WriteLine("Unable to continue! Press any key to end the program.");
                Console.Read();
                Environment.Exit(1);
            }

            try
            {
                Console.WriteLine("Initialising the device. This might take some time...");
                Console.WriteLine();
                pDev.open();
            }
            catch (ImpactAcquireException e)
            {
                // this e.g. might happen if the same device is already opened in another process...
                Console.WriteLine("An error occurred while opening device " + pDev.serial + "(error code: " + e.Message + ").");
                Console.WriteLine("Press any key to end the application...");
                Console.ReadLine();
                Environment.Exit(1);
            }

            // now display some SFNC(Standard Features Naming Convention) compliant features(see http://www.emva.org to find out more
            // about the standard and to download the latest SFNC document version)
            //
            // IMPORTANT:
            //
            // The SFNC unfortunately does NOT define numerical values for enumerations, thus a device independent piece of software
            // should use the enum-strings defined in the SFNC to ensure interoperability between devices. This is slightly slower
            // but should not cause problems in real world applications. When the device type AND GenICam XML file version is
            // guaranteed to be constant for a certain version of software, the driver internal code generator can be used to create
            // and interface header, that has numerical constants for enumerations as well. See device driver documentation under
            // 'Use Cases -> GenICam to mvIMPACT Acquire code generator' for details.

            // To make sure the camera will be configured based on a defined state, the default user set will be loaded
            UserSetControl usc = new UserSetControl(pDev);

            Console.WriteLine("The camera will be configured now!");
            Console.WriteLine();

            if (canRestoreFactoryDefault(pDev))
            {
                // Make sure the default user set and the load method exist
                // In general it is a good idea to verify if the GenICam class which should be used exists and is of the expected type
                if (usc.userSetSelector.isValid && usc.userSetSelector.isWriteable && usc.userSetLoad.isValid)
                {
                    Console.WriteLine("Loading the camera's default user set to avoid undefined settings!");
                    // selecting the default user set which includes the factory settings of the camera
                    usc.userSetSelector.writeS("Default");
                    // call the userSetLoad method to load the currently selected user set
                    usc.userSetLoad.call();
                }
                else
                {
                    Console.WriteLine("The camera seems not to support user sets!");
                }
                Console.WriteLine();
            }

            double dExposureTime = 10000;
            // initialise the AcquisitionControl class to get access to the exposure time property

            AcquisitionControl acq = new AcquisitionControl(pDev);
            // Make sure the exposure time property does exist and is currently not "read only"
            if (acq.exposureTime.isValid && acq.exposureTime.isWriteable)
            {
                Console.WriteLine("Currently the exposure time is set to {0} us. Changing to {1}us ", acq.exposureTime.read(), dExposureTime);
                Console.WriteLine();
                acq.exposureTime.write(dExposureTime);
            }

            // initialising the ImageFormatControl class to get access to the sensor's AOI settings
            ImageFormatControl ifc = new ImageFormatControl(pDev);

            // check if the sensors properties are available and modifying the AOI settings if width and height are valid properties
            if (ifc.width.isValid && ifc.height.isValid)
            {
                Console.WriteLine("The sensor has a max resolution of about {0}x{1}  pixels", ifc.width.maxValue, ifc.height.maxValue);
                Console.WriteLine("The resolution will now be adjusted to the half of width and height. The resulting AOI will be: {0}x{1} pixels", ifc.width.maxValue / 2, ifc.height.maxValue / 2);

                // the AOI settings are usually not writable once the senor is exposing images so make sure width and height are not read-only at the moment
                if (!ifc.width.isWriteable || !ifc.height.isWriteable)
                {
                    Console.WriteLine("Width or Height are not writable at the moment.");
                }
                else
                {
                    ifc.width.write(ifc.width.maxValue / 2);
                    ifc.width.write(ifc.height.maxValue / 2);
                }
                Console.WriteLine();
            }

            CounterAndTimerControl ctc = new CounterAndTimerControl(pDev);
            // Since the camera's settings have a huge impact on the frame rate of the sensor, we need the camera almost configured at this step. Otherwise the max frame rate would not be correct.

            Console.WriteLine("To avoid some cabling work, we will use an internal timer for triggering in this sample!");
            Console.WriteLine("The trigger frequency will be configured to half of the max frequency the sensor would be capable of in your setup.");
            Console.WriteLine();

            // Figuring out how many timers are available
            List<string> availableTimers = new List<string>();
            if (ctc.timerSelector.isValid)
            {
                ctc.timerSelector.getTranslationDictStrings(availableTimers);
            }
            double dPeriod = 0;
            if (ctc.timerSelector.isValid && ctc.timerSelector.isWriteable && (availableTimers.Count >= 2) && acq.triggerSelector.isValid)
            {
                // Making sure that Timer2End as TimerTriggerSource does exist
                ctc.timerSelector.writeS("Timer1");
                List<string> availableTriggerSources = new List<string>();
                ctc.timerTriggerSource.getTranslationDictStrings(availableTriggerSources);

                if (availableTriggerSources.Contains("Timer2End") && acq.mvResultingFrameRate.isValid)
                {
                    dPeriod = 1000000 / (acq.mvResultingFrameRate.read() / 2);
                    if (dPeriod >= 300)
                    {
                        // Defining the duration the trigger signal is "low"

                        ctc.timerDuration.write(1000);
                        ctc.timerTriggerSource.writeS("Timer2End");

                        // Defining the duration the trigger signal is "high"
                        ctc.timerTriggerSource.writeS("Timer1End");
                        ctc.timerSelector.writeS("Timer2");
                        ctc.timerDuration.write(dPeriod - 1000);

                        // Configuring the FrameStart trigger to use the start signal of Timer1 and enabling the trigger mode
                        acq.triggerSelector.writeS("FrameStart");
                        acq.triggerSource.writeS("Timer1Start");
                        acq.triggerMode.writeS("On");
                    }
                    else
                    {
                        Console.WriteLine("This camera does not support expected timer trigger sources! The camera will work in free run mode instead!");
                    }
                }
            }
            else
            {
                Console.WriteLine("This camera does not support timers! The camera will work in free run mode instead!");
            }

            AnalogControl anc = new AnalogControl(pDev);
            // Applying some gain to the signal provided by the camera's sensor
            if (anc.gain.isValid && anc.gain.isWriteable)
            {
                anc.gain.write(anc.gain.maxValue);
            }

            DigitalIOControl dio = new DigitalIOControl(pDev);
            // Figuring out how many digital IOs are available
            List<string> availableIOs = new List<string>();
            dio.lineSelector.getTranslationDictStrings(availableIOs);

            Console.WriteLine("Available Digital IOs:");
            bool boConfiguredFirstOutput = false;

            // iterating over the vector of digital IOs to read out the lineMode, lineStatus and lineSource properties 
            for (int i = 0; i < availableIOs.Count; i++)
            {
                dio.lineSelector.writeS("Line" + i.ToString());

                // using mvExposureAndAcquisitionActive as the lineSource for the first digital output found
                if (!boConfiguredFirstOutput && dio.lineMode.readS() == "Output")
                {
                    dio.lineSource.writeS("mvExposureAndAcquisitionActive");
                    boConfiguredFirstOutput = true;
                }
                Console.WriteLine("Line {0} - LineMode: {1} - LineStatus: {2} - LineSource: {3}", i.ToString(), dio.lineMode.readS(), dio.lineStatus.readS(), dio.lineSource.readS());
            }
            Console.WriteLine();

            // start the execution of the 'live' thread.
            Console.WriteLine("Press [ENTER] to end the application");

#if USE_DISPLAY
            ImageDisplayWindow window = new ImageDisplayWindow(String.Format("mvIMPACT_acquire sample, Device {0}", pDev.serial.read()));
#endif // #if USE_DISPLAY

            bool terminated = false;

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
                while (!terminated)
                {
                    // wait for results from the default capture queue
                    requestNr = fi.imageRequestWaitFor(timeout_ms);
                    pRequest = fi.isRequestNrValid(requestNr) ? fi.getRequest(requestNr) : null;
                    if (pRequest != null)
                    {
                        if (pRequest.isOK)
                        {
                            ++cnt;
                            // here we can display some statistical information every 100th image
                            if (cnt % 100 == 0)
                            {
                                Console.WriteLine("Info from {0}: {1}: {2}, {3}: {4}, {5}: {6}, LineStatusAll: {7}", pDev.serial.read(),
                                    statistics.framesPerSecond.name, statistics.framesPerSecond.readS(),
                                    statistics.errorCount.name, statistics.errorCount.readS(),
                                    statistics.captureTime_s.name, statistics.captureTime_s.readS(),
                                    dio.lineStatusAll.readS());

                                if (anc.gain.read() == anc.gain.maxValue)
                                {
                                    anc.gain.write(anc.gain.minValue);
                                }
                                else
                                {
                                    anc.gain.write(anc.gain.maxValue);
                                }
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

            thread.Start();
            Console.ReadLine();
            terminated = true;
            thread.Join();
#if USE_DISPLAY
            window.Dispose();
#endif // #if USE_DISPLAY
        }
    }
}
