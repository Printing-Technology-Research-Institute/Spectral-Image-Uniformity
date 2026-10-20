using System;
using System.Collections.Generic;
using System.Threading;
#if USE_DISPLAY
using mv.impact.acquire.display;
#endif // #if USE_DISPLAY
using mv.impact.acquire.examples.helper;
using mv.impact.acquire.helper;

namespace mv.impact.acquire.examples
{
    class CaptureToUserMemory
    {
        public class ThreadParameters
        {
            public Device pDev;
            public FunctionInterface fi;
            public ImageRequestControl irc;
#if USE_DISPLAY
            public ImageDisplayWindow window;
#endif // #if USE_DISPLAY
            public bool terminated = false;
            public bool userSuppliedMemoryUsed = false;
            public List<UserSuppliedHeapBuffer> buffers = new List<UserSuppliedHeapBuffer>();
            public ThreadParameters(Device d)
            {
                pDev = d;
                fi = new FunctionInterface(pDev);
                irc = new ImageRequestControl(pDev);
            }
        }

        static void CaptureThread(object data)
        {
            ThreadParameters parameters = (ThreadParameters)data;
            // establish access to the statistic properties
            Statistics statistics = new Statistics(parameters.pDev);

            // Send all requests to the capture queue. There can be more than 1 queue for some devices, but for this sample
            // we will work with the default capture queue. If a device supports more than one capture or result
            // queue, this will be stated in the manual. If nothing is mentioned about it, the device supports one
            // queue only. This loop will send all requests currently available to the driver. To modify the number of requests
            // use the property mv.impact.acquire.SystemSettings.requestCount at runtime or the property
            // mv.impact.acquire.Device.defaultRequestCount BEFORE opening the device.
            TDMR_ERROR result = TDMR_ERROR.DMR_NO_ERROR;
            while ((result = (TDMR_ERROR)parameters.fi.imageRequestSingle()) == TDMR_ERROR.DMR_NO_ERROR) { };
            if (result != TDMR_ERROR.DEV_NO_FREE_REQUEST_AVAILABLE)
            {
                Console.WriteLine("'FunctionInterface.imageRequestSingle' returned with an unexpected result: {0}({1})", result, ImpactAcquireException.getErrorCodeAsString(result));
            }
            DeviceAccess.manuallyStartAcquisitionIfNeeded(parameters.pDev, parameters.fi);

            // run thread loop
            Request pRequest = null;
            // we always have to keep at least 2 images as the display module might want to repaint the image, thus we
            // cannot free it unless we have a assigned the display to a new buffer.
            Request pPreviousRequest = null;
            int timeout_ms = 500;
            int cnt = 0;
            int requestNr = Device.INVALID_ID;
            while (!parameters.terminated)
            {
                // wait for results from the default capture queue
                requestNr = parameters.fi.imageRequestWaitFor(timeout_ms);
                pRequest = parameters.fi.isRequestNrValid(requestNr) ? parameters.fi.getRequest(requestNr) : null;
                if (pRequest != null)
                {
                    if (pRequest.isOK)
                    {
                        ++cnt;
                        // here we can display some statistical information every 100th image
                        if (cnt % 100 == 0)
                        {
                            Console.WriteLine("Info from {0}: {1}: {2}, {3}: {4}, {5}: {6}", parameters.pDev.serial.read(),
                                statistics.framesPerSecond.name, statistics.framesPerSecond.readS(),
                                statistics.errorCount.name, statistics.errorCount.readS(),
                                statistics.captureTime_s.name, statistics.captureTime_s.readS());
                        }
#if USE_DISPLAY
#   if CLR_AT_LEAST_3_DOT_5
                        // Extension methods are not supported by CLR versions smaller than 3.5 and this next function
                        // is therefore not available then.
                        parameters.window.imageDisplay.SetImage(pRequest);
#   else
                        // If extension methods are not available, the following function can be used instead. It is
                        // not as convenient and will only work for some pixel formats. For more complex pixel formats
                        // use other overloads of this method
                        parameters.window.imageDisplay.SetImage(pRequest.imageData.read(), pRequest.imageWidth.read(), pRequest.imageHeight.read(), pRequest.imageBytesPerPixel.read() * 8, pRequest.imageLinePitch.read());
#   endif
                        parameters.window.imageDisplay.Update();
#endif // #if USE_DISPLAY
                        UserSuppliedMemory.checkCaptureBufferAddress(pRequest, parameters.userSuppliedMemoryUsed, parameters.buffers);
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
                    parameters.fi.imageRequestSingle();
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
            DeviceAccess.manuallyStopAcquisitionIfNeeded(parameters.pDev, parameters.fi);

#if USE_DISPLAY
            // stop the display from showing freed memory
            parameters.window.imageDisplay.RemoveImage();
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
            parameters.fi.imageRequestReset(0, 0);
        }

        public static void runThread(ThreadParameters captureParams)
        {
            captureParams.terminated = false;
            Thread thread = new Thread(CaptureToUserMemory.CaptureThread);
            thread.Start(captureParams);
            Console.ReadLine();
            captureParams.terminated = true;
            thread.Join();
        }

        public static void exitOnError(String message)
        {
            Console.WriteLine(message);
            Console.WriteLine("Press [ENTER] to exit the application.");
            Console.ReadLine();
            System.Environment.Exit(1);
        }

        static void Main(string[] args)
        {
            mv.impact.acquire.LibraryPath.init(); // this will add the folders containing unmanaged libraries to the PATH variable.
            Device pDev = DeviceAccess.getDeviceFromUserInput();
            if (pDev == null)
            {
                exitOnError("Unable to continue!");
            }

            Console.WriteLine("Initialising the device. This might take some time...");
            try
            {
                pDev.open();
            }
            catch (ImpactAcquireException e)
            {
                // this e.g. might happen if the same device is already opened in another process...
                exitOnError(String.Format("An error occurred while opening device " + pDev.serial +
                    "(error code: " + e.Message + ")."));
            }

            ThreadParameters captureParams = new ThreadParameters(pDev);
#if USE_DISPLAY
            captureParams.window = new ImageDisplayWindow(String.Format("mvIMPACT_acquire sample, Device {0}", pDev.serial.read()));
#endif // #if USE_DISPLAY

            //=============================================================================
            //========= Capture loop into memory managed by the driver (default) ==========
            //=============================================================================
            Console.WriteLine("The device will try to capture continuously into memory automatically allocated be the device driver.");
            Console.WriteLine("This is the default behaviour.");
            Console.WriteLine("Press [ENTER] to end the continuous acquisition.");
            runThread(captureParams);

            //=============================================================================
            //========= Capture loop into memory managed by the user (advanced) ===========
            //=============================================================================
            Console.WriteLine("The device will now try to capture continuously into user supplied memory.");
            int bufferSize = 0;
            int bufferAlignment = 0;
            TDMR_ERROR result = (TDMR_ERROR)captureParams.fi.getCurrentCaptureBufferLayout(captureParams.irc, out bufferSize, out bufferAlignment);
            if (result != TDMR_ERROR.DMR_NO_ERROR)
            {
                exitOnError(String.Format("An error occurred while querying the current capture buffer layout for device {0}(error code: {1}). Press [ENTER] to end the application...", captureParams.pDev.serial.read(), ImpactAcquireException.getErrorCodeAsString(result)));
            }
            result = UserSuppliedMemory.createCaptureBuffers(captureParams.fi, captureParams.buffers, bufferSize, bufferAlignment);
            if (result != TDMR_ERROR.DMR_NO_ERROR)
            {
                exitOnError(String.Format("An error occurred while setting up the user supplied buffers for device {0}(error code: {1}). Press [ENTER] to end the application...", captureParams.pDev.serial.read(), ImpactAcquireException.getErrorCodeAsString(result)));
            }
            captureParams.userSuppliedMemoryUsed = true;
            runThread(captureParams);

            //=============================================================================
            //========= unregister user supplied buffers again ============================
            //=============================================================================
            UserSuppliedMemory.freeCaptureBuffers(captureParams.fi, captureParams.buffers);

            //=============================================================================
            //========= Capture loop into memory managed by the driver again (default) ====
            //=============================================================================
            captureParams.userSuppliedMemoryUsed = false;
            Console.WriteLine("The device will try to capture continuously into memory automatically allocated be the device driver again.");
            Console.WriteLine("This is the default behaviour.");
            runThread(captureParams);

            //=============================================================================
            //========= Capture into a specific buffer managed by the user (advanced) =====
            //=============================================================================
            // by default the driver will decide which request will be used for an acquisition
            // requested by the user. However sometimes it can be necessary to make sure that a
            // certain request object will be used...
            Console.WriteLine("Now the device will try to capture one frame into a specific user supplied buffer");
            UserSuppliedHeapBuffer buffer = new UserSuppliedHeapBuffer(bufferSize, bufferAlignment);
            const int REQUEST_TO_USE = 2;
            // we want to use request number 'REQUEST_TO_USE' (zero based) for this acquisition thus we have to make sure
            // that there are at least 'REQUEST_TO_USE + 1' requests
            SystemSettings ss = new SystemSettings(pDev);
            ss.requestCount.write(REQUEST_TO_USE + 1);
            // associate a user supplied buffer with this request
            Request pRequest = captureParams.fi.getRequest(REQUEST_TO_USE);
            try
            {
                result = (TDMR_ERROR)pRequest.attachUserBuffer(buffer.pointer, buffer.size);
                if (result != TDMR_ERROR.DMR_NO_ERROR)
                {
                    exitOnError(String.Format("An error occurred while attaching a user buffer to request number {0}: {1}.", REQUEST_TO_USE, ImpactAcquireException.getErrorCodeAsString(result)));
                }
            }
            catch (ImpactAcquireException e)
            {
                exitOnError(String.Format("An error occurred while attaching a user buffer to request number {0}: {1}.", REQUEST_TO_USE, e.errorCodeAsString));
            }
            // define that 'REQUEST_TO_USE' is used for the next acquisition
            captureParams.irc.requestToUse.write(REQUEST_TO_USE);
            // and capture the image
            int requestUsed = Device.INVALID_ID;
            result = (TDMR_ERROR)captureParams.fi.imageRequestSingle(captureParams.irc, out requestUsed);
            if (result != TDMR_ERROR.DMR_NO_ERROR)
            {
                exitOnError(String.Format("An error occurred while requesting an image for request number {0}: {1}.", REQUEST_TO_USE, ImpactAcquireException.getErrorCodeAsString(result)));
            }
            if (requestUsed != REQUEST_TO_USE)
            {
                Console.WriteLine("ERROR! An acquisition into buffer {0} was requested, but the driver did use {1} for this acquisition.", REQUEST_TO_USE, requestUsed);
            }

            // Start the acquisition manually if this was requested(this is to prepare the driver for data capture and tell the device to start streaming data)
            if (pDev.acquisitionStartStopBehaviour.read() == TAcquisitionStartStopBehaviour.assbUser)
            {
                if ((result = (TDMR_ERROR)captureParams.fi.acquisitionStart()) != TDMR_ERROR.DMR_NO_ERROR)
                {
                    Console.WriteLine("'FunctionInterface.acquisitionStart' returned with an unexpected result: {0}({1})", result, ImpactAcquireException.getErrorCodeAsString(result));
                }
            }

            // Wait for the buffer to get filled
            int requestNr = captureParams.fi.imageRequestWaitFor(-1);

            // Stop the acquisition manually if this was requested
            if (pDev.acquisitionStartStopBehaviour.read() == TAcquisitionStartStopBehaviour.assbUser)
            {
                if ((result = (TDMR_ERROR)captureParams.fi.acquisitionStop()) != TDMR_ERROR.DMR_NO_ERROR)
                {
                    Console.WriteLine("'FunctionInterface.acquisitionStop' returned with an unexpected result: {0}({1})", result, ImpactAcquireException.getErrorCodeAsString(result));
                }
            }
            pRequest = captureParams.fi.getRequest(requestNr);
            if (!pRequest.isOK)
            {
                exitOnError(String.Format("Error: {0}", pRequest.requestResult.readS()));
            }
            Console.WriteLine("Capture into specific user supplied buffer done.");
#if USE_DISPLAY
#   if CLR_AT_LEAST_3_DOT_5
            // Extension methods are not supported by CLR versions smaller than 3.5 and this next function
            // is therefore not available then.
            captureParams.window.imageDisplay.SetImage(pRequest);
#   else
            // If extension methods are not available, the following function can be used instead. It is
            // not as convenient and will only work for some pixel formats. For more complex pixel formats
            // use other overloads of this method
            captureParams.window.imageDisplay.SetImage(pRequest.imageData.read(), pRequest.imageWidth.read(), pRequest.imageHeight.read(), pRequest.imageBytesPerPixel.read() * 8, pRequest.imageLinePitch.read());
#   endif
            captureParams.window.imageDisplay.Update();
#endif // #if USE_DISPLAY
            // and end the application
            Console.WriteLine("Press [ENTER] to end the application.");
            Console.ReadLine();
#if USE_DISPLAY
            captureParams.window.Dispose();
#endif // #if USE_DISPLAY
            captureParams.fi.imageRequestUnlock(requestNr);
            captureParams.fi.imageRequestReset(0, 0);
        }
    }
}
