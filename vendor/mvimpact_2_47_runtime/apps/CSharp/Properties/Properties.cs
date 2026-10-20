using System;
using System.Collections.Generic;

#if USE_DISPLAY
using mv.impact.acquire.display;
#endif // #if USE_DISPLAY
using mv.impact.acquire.examples.helper;

namespace mv.impact.acquire.examples
{
    class Properties
    {
        #region private members -----------------------------------------------

#if USE_DISPLAY
        static ImageDisplayWindow window_ = null;
#endif // #if USE_DISPLAY

        #endregion

        static void populatePropertyMap(Dictionary<string, Property> m, Component it, string currentPath)
        {
            while (it.isValid)
            {
                string fullName = currentPath;
                if (fullName != "")
                {
                    fullName += "/";
                }
                fullName += it.name;
                if (it.isList)
                {
                    populatePropertyMap(m, it.firstChild, fullName);
                }
                else if (it.isProp)
                {
                    m.Add(fullName, new Property(it.hObj));
                }
                ++it;
                // method object will be ignored...
            }
        }

        static void singleCapture(Device pDev, FunctionInterface fi, int maxWaitTime_ms)
        {
            // send a request to the default request queue of the device and wait for the result.
            TDMR_ERROR result = (TDMR_ERROR)fi.imageRequestSingle();
            if (result != TDMR_ERROR.DMR_NO_ERROR)
            {
                Console.WriteLine("'FunctionInterface.imageRequestSingle' returned with an unexpected result: {0}({1})", result, ImpactAcquireException.getErrorCodeAsString(result));
            }

            DeviceAccess.manuallyStartAcquisitionIfNeeded(pDev, fi);
            // wait for results from the default capture queue
            int requestNr = fi.imageRequestWaitFor(maxWaitTime_ms);
            Request pRequest = fi.isRequestNrValid(requestNr) ? fi.getRequest(requestNr) : null;
            if (pRequest != null)
            {
                if (pRequest.isOK)
                {
                    // everything went well. Display the result
#if USE_DISPLAY
                    // IMPORTANT: Please note that there will be just one refresh for the display window, so if it is hidden under another window the result will not be visible.

                    // initialise display window
                    // here we check whether this is the first snapshot or not. We do not want a new window created with each snapshot.
                    if (window_ == null)
                    {
                        window_ = new ImageDisplayWindow(String.Format("mvIMPACT_acquire sample, Device {0}", pDev.serial.read()));
                    }
#   if CLR_AT_LEAST_3_DOT_5
                    // Extension methods are not supported by CLR versions smaller than 3.5 and this next function
                    // is therefore not available then.
                    window_.imageDisplay.SetImage(pRequest);
#   else
                    // If extension methods are not available, the following function can be used instead. It is
                    // not as convenient and will only work for some pixel formats. For more complex pixel formats
                    // use other overloads of this method
                    window_.imageDisplay.SetImage(pRequest.imageData.read(), pRequest.imageWidth.read(), pRequest.imageHeight.read(), pRequest.imageBytesPerPixel.read() * 8, pRequest.imageLinePitch.read());
#   endif
                    window_.imageDisplay.Update();
#endif // #if USE_DISPLAY
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

                Console.WriteLine();
                Console.WriteLine("\t\t Image captured: {0}({1}x{2})", pRequest.imagePixelFormat.readS(), pRequest.imageWidth.read(), pRequest.imageHeight.read());

                // unlock the buffer to let the driver know that you no longer need this buffer.
                pRequest.unlock();
                Console.WriteLine();
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
            DeviceAccess.manuallyStopAcquisitionIfNeeded(pDev, fi);
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

            Console.WriteLine("Initialising the device. This might take some time...");
            try
            {
                pDev.open();
            }
            catch (ImpactAcquireException e)
            {
                // this e.g. might happen if the same device is already opened in another process...
                Console.WriteLine("An error occurred while opening the device " + pDev.serial +
                    "(error code: " + e.Message + "). Press any key to end the application...");
                Console.ReadLine();
                Environment.Exit(1);
            }

            // create an interface to the selected device
            FunctionInterface fi = new FunctionInterface(pDev);

            // obtain all the settings related properties available for this device
            // Only work with the 'Base' setting. For more information please refer to the manual (working with settings)
            Dictionary<string, Property> propertyMap = new Dictionary<string, Property>();
            DeviceComponentLocator locator = new DeviceComponentLocator(pDev, TDeviceListType.dltSetting, "Base");
            Component comp = new Component(locator.searchbase_id);
            populatePropertyMap(propertyMap, comp.firstChild, "");
            try
            {
                // this category is not supported by every device, thus we can expect an exception if this feature is missing
                locator = new DeviceComponentLocator(pDev, TDeviceListType.dltIOSubSystem);
                comp = new Component(locator.searchbase_id);
                populatePropertyMap(propertyMap, comp.firstChild, "");
            }
            catch (ImpactAcquireException) { }

            locator = new DeviceComponentLocator(pDev, TDeviceListType.dltRequest);
            comp = new Component(locator.searchbase_id);
            populatePropertyMap(propertyMap, comp.firstChild, "");

            locator = new DeviceComponentLocator(pDev, TDeviceListType.dltSystemSettings);
            comp = new Component(locator.searchbase_id);
            populatePropertyMap(propertyMap, comp.firstChild, "SystemSettings");

            locator = new DeviceComponentLocator(pDev, TDeviceListType.dltInfo);
            comp = new Component(locator.searchbase_id);
            populatePropertyMap(propertyMap, comp.firstChild, "Info");
            comp = new Component(pDev.hDev);
            populatePropertyMap(propertyMap, comp.firstChild, "Device");

            int timeout_ms = 500;
            bool boRun = true;
            while (boRun)
            {
                Console.WriteLine();
                Console.WriteLine("-------------------------------------------------------------------------------");
                Console.WriteLine("            enter 'quit', 'snap', 'list', 'help' or the name of                ");
                Console.WriteLine("            the property you want to modify followed by [ENTER]:               ");
                Console.WriteLine("-------------------------------------------------------------------------------");
                Console.Write("Command:>");
                String command = Console.ReadLine();
                Console.WriteLine();

                if (command == "snap")
                {
                    singleCapture(pDev, fi, timeout_ms);
                }
                else if (command == "list")
                {
                    foreach (KeyValuePair<string, Property> KVP in propertyMap)
                    {
                        Console.WriteLine(KVP.Key);
                    }
                }
                else if (command == "help")
                {
                    Console.WriteLine("quit: terminates this sample application");
#if USE_DISPLAY
                    Console.WriteLine("snap: takes and displays one image with the current settings");
#else
                    Console.WriteLine("snap: takes one image with the current settings");
#endif // USE_DISPLAY
                    Console.WriteLine("list: displays all properties available for this device");
                    Console.WriteLine("help: displays this help text");
                    Console.WriteLine("timeout: set a new timeout(in ms) used as a max. timeout to wait for an image");
                    Console.WriteLine("the full name of a property must be specified");
                }
                else if (command == "quit")
                {
                    boRun = false;
                    continue;
                }
                else if (command == "timeout")
                {
                    Console.WriteLine("Enter the new timeout to be passed to the imageRequestWaitFor function: ");
                    timeout_ms = Convert.ToInt32(Console.ReadLine());
                }
                else
                {
                    if (!propertyMap.ContainsKey(command))
                    {
                        Console.WriteLine("Unknown command or property");
                    }
                    else
                    {
                        Property prop = propertyMap[command];
                        FeatureAccess.displayPropertyData(prop);
                        if (prop.hasDict)
                        {
                            Console.WriteLine("This function expects the string representation as input!");
                        }
                        if (prop.isWriteable)
                        {
                            Console.Write("Enter new Value:> ");
                            String NewVal = Console.ReadLine();
                            int index = 0;
                            if (prop.valCount > 1)
                            {
                                Console.WriteLine("'{0} defines {1} values. Enter the (zero-based) index of the value to modify: ", prop.name, prop.valCount);
                                index = System.Convert.ToInt32(Console.ReadLine());
                            }
                            try
                            {
                                prop.writeS(NewVal);
                            }
                            catch (ImpactAcquireException e)
                            {
                                Console.WriteLine("");
                                Console.WriteLine("An exception occurred: {0}(error code: {1})", e.errorString, e.errorCodeAsString);
                            }
                        }
                        else
                        {
                            Console.WriteLine("'{0}' is read-only, thus can't be modified.", prop.name);
                        }
                    }
                }
            }
        }
    }
}
