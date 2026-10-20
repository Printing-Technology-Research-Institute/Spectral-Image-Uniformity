using AForge.Imaging;
using mv.impact.acquire.display;
using mv.impact.acquire.GenICam;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Drawing;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using System.Xml;

namespace mv.impact.acquire.examples
{
    //-----------------------------------------------------------------------------
    /// \brief Class for image and camera handling to be more convenient in combination with WinForms
    /**
     * Class for convenient handling of mvIMPACT Acquire in combination with WinForms
     * 
     * Available features are
     * - modifying Gain
     * - modifying Bit Shift
     * - modifying Exposure Time
     * - switch illumination on/off
     * - switch triggered mode on/off
     * - return statistical data
     */
    public class AcquisitonClass
    {
        #region private members -----------------------------------------------

        private Device pDev_ = null;
        private DeviceControl dc_ = null;
        private FunctionInterface fi_ = null;
        private ImageFormatControl ifc_ = null;
        private AnalogControl ac_ = null;
        private AcquisitionControl acqc_ = null;
        private DigitalIOControl dioc_ = null;
        private Statistics statistics_ = null;
        private mv.impact.acquire.GenICam.CustomCommandGenerator ccg_ = null;

        private const int timeout_ms_ = 200; ///< Defines the image acquisition timeout
        private int fps_ = 100; ///< Framerate of camera.
        private int aoiOffsetX_ = 0; ///< Defines the offset of the small image to the display border in X-orientation
        private int aoiOffsetY_ = 0; ///< Defines the offset of the small image to the display border in Y-orientation
        private int scaleFactor_ = 4;
        private int minBlobSize_ = 160;
        private String settingsFileName_ = "SmartFrameRecall.settings.xml";
        int imageRequestCountOK_ = 0;
        int imageRequestCountErr_ = 0;
        private BlockingQueue<List<SmartFrameRecallData>> smartFrameRecallDataQueue_ = new BlockingQueue<List<SmartFrameRecallData>>();

        private Boolean boAcquisitionThreadRunning_ = false;
        private Thread acquisitionThread_ = null;
        private Boolean boSmartFrameRecallThreadRunning_ = false;
        private Thread smartFrameRecallThread_ = null;
        private Boolean boFlashMode_ = false;
        private Boolean boTriggerMode_ = false;

        private Boolean boShowFullresSmallView_ = true;

        #endregion
        #region private methods -----------------------------------------------

        private void closeMvGenTLDevice()
        {
            if (pDev_ != null)
            {
                pDev_.close();
                pDev_ = null;
            }
        }

        private void openMvGenTLDevice(String serial)
        {
            try
            {
                pDev_ = DeviceManager.getDeviceBySerial(serial);
                if (pDev_ != null)
                {
                    pDev_.interfaceLayout.write(TDeviceInterfaceLayout.dilGenICam);
                    pDev_.acquisitionStartStopBehaviour.write(TAcquisitionStartStopBehaviour.assbUser);
                    pDev_.open();
                }
                else
                {
                    throw new EDeviceManager("Device with serial number " + serial + " could not be found!", TDMR_ERROR.DEV_ACCESS_DENIED);
                }
            }
            catch (ImpactAcquireException e)
            {
                if (pDev_ == null)
                {
                    MessageBox.Show("Invalid device selection!");
                }
                else
                {
                    MessageBox.Show(String.Format("Error while opening device {0}({1}): {2}! ", pDev_.serial.readS(), pDev_.product.readS(), e.errorString));
                }
                Environment.Exit(1);
            }
        }

        private void initMvGenTLDevice()
        {
            try
            {
                acqc_ = new AcquisitionControl(pDev_);
                // Important: Switch off 'mvSmartFrameRecallEnable' because when enabled certain features like binning/decimation etc. will become read-only
                if (!acqc_.mvSmartFrameRecallEnable.isValid)
                {
                    throw new EDeviceManager("The device does not support the Smart Frame Recall feature this application is written for", TDMR_ERROR.DEV_UNSUPPORTED_PARAMETER);
                }
                acqc_.mvSmartFrameRecallEnable.write(TBoolean.bFalse);

                dc_ = new DeviceControl(pDev_);
                fi_ = new FunctionInterface(pDev_);
                ac_ = new AnalogControl(pDev_);
                ifc_ = new ImageFormatControl(pDev_);
                dioc_ = new DigitalIOControl(pDev_);

                statistics_ = new Statistics(pDev_);
                ccg_ = new mv.impact.acquire.GenICam.CustomCommandGenerator(pDev_);

                BasicDeviceSettings bds = new BasicDeviceSettings(pDev_);
                bds.imageRequestTimeout_ms.write(0);

                SystemSettings ss = new SystemSettings(pDev_);
                ss.requestCount.write(25);

                ChunkDataControl cdc = new ChunkDataControl(pDev_);
                cdc.chunkModeActive.write(TBoolean.bTrue);
                cdc.chunkSelector.writeS("mvCustomIdentifier");
                cdc.chunkEnable.write(TBoolean.bTrue);

                ifc_.decimationHorizontal.write(scaleFactor_);
                ifc_.decimationVertical.write(scaleFactor_);

                //make sure the image width is a multiple of 8 - this is necessary in order to enable SmartFrameRecall
                long modulo = ifc_.width.read() % 8;
                if (modulo != 0)
                {
                    ifc_.width.write(ifc_.width.read() - modulo);
                }

                acqc_.mvAcquisitionFrameRateEnable.writeS("On");
                acqc_.acquisitionFrameRate.write(100);
                acqc_.exposureTime.write(100);

                acqc_.mvSmartFrameRecallEnable.write(TBoolean.bTrue);

                acqc_.mvAcquisitionFrameRateLimitMode.writeS("mvDeviceMaxSensorThroughput");
                acqc_.acquisitionFrameRate.write(fps_);

                acqc_.acquisitionMode.writeS("Continuous");

                ReadOnlyCollection<String> reportedPixelFormats = ifc.pixelFormat.listOfValidStrings;
                if (reportedPixelFormats.Contains("Mono8"))
                {
                    ifc_.pixelFormat.writeS("Mono8");
                }
                else if (reportedPixelFormats.Contains("BayerRG8"))
                {
                    ifc_.pixelFormat.writeS("BayerRG8");
                }
                else if (reportedPixelFormats.Contains("BayerGR8"))
                {
                    ifc_.pixelFormat.writeS("BayerGR8");
                }
                else if (reportedPixelFormats.Contains("BayerBG8"))
                {
                    ifc_.pixelFormat.writeS("BayerBG8");
                }
                else if (reportedPixelFormats.Contains("BayerGB8"))
                {
                    ifc_.pixelFormat.writeS("BayerGB8");
                }
                else
                {
                    throw new EDeviceManager("The device does not support any of the pixel formats needed by this application", TDMR_ERROR.DEV_UNSUPPORTED_PARAMETER);
                }

                ImageDestination imageDestination = new ImageDestination(pDev_);
                // This will convert Bayer to Mono for color cameras or will do nothing for grey cameras
                imageDestination.pixelFormat.write(TImageDestinationPixelFormat.idpfMono8);

                if (dioc_.userOutputValue.read() == TBoolean.bTrue)
                {
                    boFlashMode_ = true;
                }
                if (acqc_.triggerMode.read() == 1)
                {
                    boTriggerMode_ = true;
                }
            }
            catch (ImpactAcquireException e)
            {
                StringBuilder sb = new StringBuilder();
                sb.AppendFormat("Failed to configure device {0}({1}). Additional error information: {2}({3})", pDev_.serial.readS(), pDev_.product.readS(), e.errorString, e.errorCodeAsString);
                MessageBox.Show(sb.ToString());
                Environment.Exit(1);
            }
        }

        /// <summary>
        /// \brief Defines the output which should be used to in combination with UserOutput0. The accepted parameter defines the Output line.
        /// </summary>
        /// <param name="line">Used to define the line number which should be toggled</param>
        /// 
        private void defineFlashLine(int line)
        {
            switch (line)
            {
                case 0:
                    dioc_.lineSelector.writeS("Line0");
                    dioc_.lineSource.writeS("UserOutput0");
                    break;
                case 1:
                    dioc_.lineSelector.writeS("Line1");
                    dioc_.lineSource.writeS("UserOutput0");
                    break;
                case 2:
                    dioc_.lineSelector.writeS("Line2");
                    dioc_.lineSource.writeS("UserOutput0");
                    break;
                case 3:
                    dioc_.lineSelector.writeS("Line2");
                    dioc_.lineSource.writeS("UserOutput0");
                    break;
            }
        }

        private static void storeXMLElements(List<XMLPropertyDataSet> xmlSettings, XmlWriter writer, String className)
        {
            foreach (XMLPropertyDataSet element in xmlSettings)
            {
                if (element.ClassName == className)
                {
                    writer.WriteStartElement("Property");
                    writer.WriteAttributeString("Name", element.Name);
                    writer.WriteElementString("Value", element.Value.ToString());
                    writer.WriteElementString("Type", element.DataType.ToString());
                    writer.WriteEndElement();
                }
            }
        }

        delegate void UpdateControlImage(PictureBox pb);

        private void updateControl(PictureBox pb)
        {
            if (pb.InvokeRequired)
            {
                UpdateControlImage d = new UpdateControlImage(updateControl);
                pb.Invoke(d, new object[] { pb });
            }
            else
            {
                pb.Update();
            }
        }

        #endregion
        #region public properties ---------------------------------------------

        public Device pDev
        {
            get { return pDev_; }
        }

        public ImageFormatControl ifc
        {
            get { return ifc_; }
        }

        public DeviceControl dc
        {
            get { return dc_; }
        }

        public Boolean toggleFullResolutionSmallView()
        {
            boShowFullresSmallView_ = !boShowFullresSmallView_;
            return boShowFullresSmallView_;
        }

        #endregion
        #region public methods ------------------------------------------------

        public void setUp(String selectedDeviceSerial)
        {
            openMvGenTLDevice(selectedDeviceSerial);
            initMvGenTLDevice();
            defineFlashLine(0);
            boSmartFrameRecallThreadRunning_ = true;
            smartFrameRecallThread_ = new Thread(delegate ()
            {
                // This thread processes the transmission request that come out of the blob analysis
                // in the acquisition thread. processing these requests in a separate thread allows
                // the capture thread to continue to receive frames without being blocked by additional
                // communication with the device.
                while (boSmartFrameRecallThreadRunning_)
                {
                    List<SmartFrameRecallData> requestsToQueue = null;
                    smartFrameRecallDataQueue_.TryDequeue(out requestsToQueue, 100);
                    if (requestsToQueue != null)
                    {
                        int combinedErrorCode = 0;
                        foreach (SmartFrameRecallData requestToQueue in requestsToQueue)
                        {
                            combinedErrorCode |= ccg_.queueTransmissionRequest(requestToQueue.timestamp_us_, requestToQueue.x_, requestToQueue.y_, requestToQueue.w_, requestToQueue.h_, TRequestTransmissionMode.rtmFullResolution, requestToQueue.identifier_);
                        }
                        combinedErrorCode |= ccg_.sendCommandBuffer();
                        if (combinedErrorCode == 0)
                        {
                            imageRequestCountOK_++;
                        }
                        else
                        {
                            imageRequestCountErr_++;
                        }
                    }
                }
            });
            smartFrameRecallThread_.Start();
        }

        /// <summary>
        /// /brief Toggles the DigOut of the camera.
        /// </summary>
        public void toggleFlash()
        {
            dioc_.userOutputSelector.writeS("UserOutput0");
            if (!boFlashMode_)
            {
                dioc_.userOutputValue.write(TBoolean.bTrue);
                boFlashMode_ = true;
            }
            else
            {
                dioc_.userOutputValue.write(TBoolean.bFalse);
                boFlashMode_ = false;
            }
        }

        /// <summary>
        /// Activates or deactivates the trigger input on Line4.
        /// </summary>
        public void toggleTriggerMode()
        {
            if (acqc_.triggerMode.readS() == "On")
            {
                acqc_.triggerMode.writeS("Off");
                boTriggerMode_ = false;
            }
            else
            {
                acqc_.triggerSelector.writeS("FrameStart");
                acqc_.triggerSource.writeS("Line4");
                acqc_.triggerActivation.writeS("RisingEdge");
                acqc_.triggerMode.writeS("On");
                boTriggerMode_ = true;
            }
        }

        /// <summary>
        /// Changes the exposure time according to the given value. 
        /// </summary>
        /// <param name="exposureStep">Represents the step width which should be used for the modification. Positive values increase the value, negative values decrease the value. </param>
        public void changeExposure(double exposureStep)
        {
            if (exposureStep > 0 && acqc_.exposureTime.read() + exposureStep <= acqc_.exposureTime.maxValue)
            {
                acqc_.exposureTime.write(acqc_.exposureTime.read() + exposureStep);
            }
            else if (exposureStep < 0 && acqc_.exposureTime.read() + exposureStep >= acqc_.exposureTime.minValue)
            {
                acqc_.exposureTime.write(acqc_.exposureTime.read() + exposureStep);
            }
        }

        public int changeFPS(int fpsStep)
        {
            if (fps_ < 200 && fpsStep > 0)
            {
                fps_ += fpsStep;
            }

            if (fps_ > 10 && fpsStep < 0)
            {
                fps_ += fpsStep;
            }
            if (fps_ < 0) fps_ = 10;
            acqc_.acquisitionFrameRate.write(fps_);
            return fps_;
        }

        /// <summary>
        /// changes the gain value according to the given gainStep value
        /// </summary>
        /// <param name="gainStep">Positive gain steps increase the gain, negative gain steps decrease the gain value</param>
        public void changeGain(double gainStep)
        {
            if (gainStep > 0 && (ac_.gain.read() + gainStep) <= ac_.gain.maxValue)
            {
                ac_.gain.write(ac_.gain.read() + gainStep);
            }
            if (gainStep < 0 && (ac_.gain.read() + gainStep) >= ac_.gain.minValue)
            {
                ac_.gain.write(ac_.gain.read() + gainStep);
            }
        }

        /// <summary>
        /// moves the AOI which is displayed in the smaller image into the given direction.
        /// </summary>
        /// <param name="stepX">Is used for the X-direction</param>
        /// <param name="stepY">is used for the Y-direction</param>
        /// <param name="stepWidth">Defines the size of the step size</param>
        public void moveSmallAOI(int stepX, int stepY, int stepWidth)
        {
            if (stepX == 1 && stepY == 0 && aoiOffsetX_ >= 0)
            {
                aoiOffsetX_ = aoiOffsetX_ + stepWidth;
            }
            if (stepX == -1 && stepY == 0 && aoiOffsetX_ > 0)
            {
                aoiOffsetX_ = aoiOffsetX_ - stepWidth;
            }
            if (stepX == 0 && stepY == 1 && aoiOffsetY_ >= 0)
            {
                aoiOffsetY_ = aoiOffsetY_ + stepWidth;
            }
            if (stepX == 0 && stepY == -1 && aoiOffsetY_ > 0)
            {
                aoiOffsetY_ = aoiOffsetY_ - stepWidth;
            }
        }

        /// <summary>
        /// acquires images and provides them to the given PictureBox
        /// </summary>
        /// <param name="WindowHandle">Handle to the PictureBox that should be used for displaying</param>
        /// <param name="SmallWindowsHandles">Handle to the PictureBoxes that should be used for displaying of the small window</param>
        /// <param name="pBoxSmall">small picture boxes</param>
        /// <param name="lblStatistics">The label that should be used for updating the statistics</param>
        /// <param name="lblTrigger">The label that should be used to display the trigger mode state</param>
        /// <param name="lblFlash">The label that should be used to display the flash mode state</param>
        public void acquireImages(IntPtr WindowHandle, PictureBox pBoxWindow, IntPtr[] SmallWindowsHandles, PictureBox[] pBoxSmall, Label lblStatistics)
        {
            ImageDisplay window = new ImageDisplay(WindowHandle);
            window.displayMode = TDisplayMode.DM_Default;

            int SmallWindowCount = SmallWindowsHandles.GetLength(0);
            ImageDisplay[] windowSmallArray = new ImageDisplay[SmallWindowCount];

            int win = 0;
            foreach (IntPtr handle in SmallWindowsHandles)
            {
                ImageDisplay windowSmall = new ImageDisplay(handle);
                windowSmall.displayMode = TDisplayMode.DM_Default;
                windowSmallArray[win] = windowSmall;
                win++;
            }

            TDMR_ERROR result = TDMR_ERROR.DMR_NO_ERROR;
            boAcquisitionThreadRunning_ = true;
            acquisitionThread_ = new Thread(delegate ()
            {
                while ((result = (TDMR_ERROR)fi_.imageRequestSingle()) == TDMR_ERROR.DMR_NO_ERROR) { };
                if (result != TDMR_ERROR.DEV_NO_FREE_REQUEST_AVAILABLE)
                {
                    MessageBox.Show(String.Concat("'FunctionInterface.imageRequestSingle' returned with an unexpected result: {0}({1})", result, ImpactAcquireException.getErrorCodeAsString(result)));
                }

                if ((result = (TDMR_ERROR)fi_.acquisitionStart()) != TDMR_ERROR.DMR_NO_ERROR)
                {
                    MessageBox.Show(String.Concat("'FunctionInterface.acquisitionStart' returned with an unexpected result: {0}({1})", result, ImpactAcquireException.getErrorCodeAsString(result)));
                }

                Request pRequest = null;
                Request pPreviousRequest = null;
                BlobCounter blobCounter = new BlobCounter();
                int cnt = 0;
                const int statisticsUpdateInterval = 50;
                int requestNr = Device.INVALID_ID;
                int objectsFound = 0;
                Rectangle rect = new Rectangle(0, 0, 0, 0);
                int activeSmallWindow = 0;
                uint identifier = 1;
                int decimationHorizontal = (int)ifc_.decimationHorizontal.read();
                int imageRequestCount = 0;
                int requestsArrived = 0;
                int errorCount = 0;
                long totalBytesTransferred = 0;
                DateTime startTime = DateTime.UtcNow;

                while (boAcquisitionThreadRunning_)
                {
                    requestNr = fi_.imageRequestWaitFor(timeout_ms_);
                    pRequest = fi_.isRequestNrValid(requestNr) ? fi_.getRequest(requestNr) : null;
                    if (pRequest != null)
                    {
                        if (pRequest.isOK)
                        {
                            totalBytesTransferred += pRequest.imageWidth.read() * pRequest.imageHeight.read() * pRequest.imageBytesPerPixel.read(); // This is not exact, but with smart frame recall active 'imageSize' will always report the max. size as the buffer must use this size in order to be able to receive every possible image
                            if (pRequest.chunkmvCustomIdentifier.read() == identifier)
                            {
                                // previously requested image
                                requestsArrived++;
                                ImageDisplay windowSmall = windowSmallArray[activeSmallWindow + 1];
                                PictureBox pb = pBoxSmall[activeSmallWindow + 1];
                                windowSmall.SetImage(pRequest);
                                windowSmall.SetBitmapRectangle(0, 0, pRequest.imageWidth.read(), pRequest.imageHeight.read());
                                windowSmall.SetDisplayRectangle(0, 0, pb.Width, pb.Height);
                                windowSmall.Update();
                                activeSmallWindow++;
                                if (activeSmallWindow >= SmallWindowCount - 1)
                                {
                                    activeSmallWindow = 0;
                                }

                                if (boShowFullresSmallView_)
                                {
                                    ImageDisplay windowSmallLive = windowSmallArray[0];
                                    PictureBox pbLive = pBoxSmall[0];
                                    windowSmallLive.SetImage(pRequest);
                                    windowSmallLive.SetBitmapRectangle(0, 0, pRequest.imageWidth.read(), pRequest.imageHeight.read());
                                    windowSmallLive.SetDisplayRectangle(0, 0, pbLive.Width, pbLive.Height);
                                    windowSmallLive.Update();
                                }
                            }
                            else
                            {
                                // normally streamed image
                                #region displaying
                                // windowSmall.shift = 4;
                                switch (pRequest.imagePixelFormat.read())
                                {
                                    case TImageBufferPixelFormat.ibpfRGB101010Packed:
                                    case TImageBufferPixelFormat.ibpfRGB121212Packed:
                                    case TImageBufferPixelFormat.ibpfRGB141414Packed:
                                    case TImageBufferPixelFormat.ibpfRGB161616Packed:
                                        window.SetImage(pRequest);
                                        objectsFound = 0;
                                        break;
                                    case TImageBufferPixelFormat.ibpfMono8:
                                    case TImageBufferPixelFormat.ibpfMono10:
                                    case TImageBufferPixelFormat.ibpfMono12:
                                    case TImageBufferPixelFormat.ibpfMono12Packed_V1:
                                    case TImageBufferPixelFormat.ibpfMono12Packed_V2:
                                    case TImageBufferPixelFormat.ibpfMono16:
                                        int wr = (int)pRequest.chunkWidth.read();
                                        int hr = (int)pRequest.chunkHeight.read();
                                        blobCounter.BackgroundThreshold = Color.FromArgb(100, 100, 100);
                                        blobCounter.FilterBlobs = true;
                                        blobCounter.MinWidth = minBlobSize_ / decimationHorizontal;
                                        blobCounter.MinHeight = minBlobSize_ / decimationHorizontal;
                                        // set ordering options
                                        blobCounter.ObjectsOrder = ObjectsOrder.Size;
                                        using (mv.impact.acquire.RequestBitmapData data = pRequest.bitmapData)
                                        {
                                            blobCounter.ProcessImage(data.bitmap);
                                        }
                                        objectsFound = blobCounter.ObjectsCount;
                                        Blob[] blobs = blobCounter.GetObjectsInformation();
                                        // extract the biggest blobs
                                        if (blobs.Length > 0)
                                        {
                                            // do not process more than 2 blobs!
                                            int blobcount = blobs.Length;
                                            if (blobcount > 2)
                                            {
                                                blobcount = 2;
                                            }

                                            List<SmartFrameRecallData> ROIsToRequest = new List<SmartFrameRecallData>();
                                            for (int blob = 0; blob < blobcount; blob++)
                                            {
                                                rect = blobs[blob].Rectangle;
                                                int border = 3;
                                                if ((rect.Top > border) && (rect.Bottom < (hr - border)))
                                                {
                                                    // get center
                                                    int xcenter = rect.Left + rect.Width / 2;
                                                    int ycenter = rect.Top + rect.Height / 2;
                                                    // make square
                                                    int size = rect.Width;
                                                    if (rect.Width < rect.Height)
                                                    {
                                                        size = rect.Height;
                                                    }
                                                    //observe alignment restrictions of requestTransmission()
                                                    switch (decimationHorizontal)
                                                    {
                                                        case 1:
                                                            xcenter = xcenter & 0xfff8;
                                                            size = size & 0xfff8;
                                                            break;
                                                        case 2:
                                                            xcenter = xcenter & 0xfffc;
                                                            size = size & 0xfffc;
                                                            break;
                                                        case 4:
                                                            xcenter = xcenter & 0xfffe;
                                                            size = size & 0xfffe;
                                                            break;
                                                    }

                                                    int half_size = size / 2;
                                                    int OffsX = xcenter - half_size;
                                                    int OffsY = ycenter - half_size;

                                                    // make sure AOI fits within frame
                                                    if (OffsX >= 0 && OffsY >= 0 && (OffsX + size) < wr && (OffsY + size) < hr)
                                                    {
                                                        imageRequestCount++;
                                                        ROIsToRequest.Add(new SmartFrameRecallData(pRequest.infoTimeStamp_us.read(), OffsX, OffsY, size, size, identifier));
                                                        if (!boShowFullresSmallView_)
                                                        {
                                                            ImageDisplay windowSmallLive = windowSmallArray[0];
                                                            PictureBox pbLive = pBoxSmall[0];
                                                            windowSmallLive.SetImage(pRequest);
                                                            windowSmallLive.SetBitmapRectangle(OffsX, OffsY, size, size);
                                                            windowSmallLive.SetDisplayRectangle(0, 0, pbLive.Width, pbLive.Height);
                                                            windowSmallLive.Update();
                                                        }
                                                    }
                                                }
                                            }
                                            if (ROIsToRequest.Count > 0) // There might be blobs, but those might not fulfill all the requirements from above thus this list might actually be empty!
                                            {
                                                smartFrameRecallDataQueue_.Enqueue(ROIsToRequest);
                                            }
                                        }
                                        window.SetImage(pRequest);
                                        window.SetBitmapRectangle(0, 0, wr, hr);
                                        window.SetDisplayRectangle(0, 0, pBoxWindow.Width, pBoxWindow.Height);
                                        break;
                                }
                                window.Update();
                                #endregion

                                DateTime now = DateTime.UtcNow;
                                double TotalRuntimeSeconds = (now - startTime).TotalSeconds;
                                if ((cnt % statisticsUpdateInterval) == 0)
                                {
                                    updateStatistics(cnt, lblStatistics, pRequest, objectsFound, rect, errorCount, imageRequestCount, imageRequestCountOK_, imageRequestCountErr_, requestsArrived, totalBytesTransferred, TotalRuntimeSeconds);
                                }
                                cnt++;
                            }
                        }
                        else
                        {
                            errorCount++;
                        }

                        if (pPreviousRequest != null)
                        {
                            pPreviousRequest.unlock();
                        }
                        pPreviousRequest = pRequest;
                        fi_.imageRequestSingle();
                    }
                    else
                    {
                        errorCount++;
                    }
                }

                if ((result = (TDMR_ERROR)fi_.acquisitionStop()) != TDMR_ERROR.DMR_NO_ERROR)
                {
                    MessageBox.Show(String.Concat("'FunctionInterface.acquisitionStop' returned with an unexpected result: {0}({1})", result, ImpactAcquireException.getErrorCodeAsString(result)));
                }
                if (pRequest != null)
                {
                    pRequest.unlock();
                }

                fi_.imageRequestReset(0, 0);
            });
            acquisitionThread_.Start();
        }

        /// <summary>
        /// Updates the statistics in the objects which are passed to the function.
        /// </summary>
        /// <param name="cnt">Represents the current frame number</param>
        /// <param name="lblStatistics">Represents the label which should be used to display the statistic data</param>
        /// <param name="lblTrigger">Represents the label which should be used to display the trigger status</param>
        /// <param name="lblFlash">Represents the label which should be used to display the flash status</param>
        public void updateStatistics(int cnt, Label lblStatistics, Request pRequest, int objectsFound, Rectangle rect, int errorCount, int imageRequestCount, int imageRequestCountOK, int imageRequestCountErr, int requestsArrived, long totalBytesTransferred, double totalRuntimeSeconds)
        {
            if (!lblStatistics.IsDisposed)
            {
                if (totalRuntimeSeconds > 0)
                {
                    String msg = String.Concat(
                                "Gain [dB]: ", pRequest.getChunkGain(ac_.gainSelector.readS()).readS(), "\n",
                                "Exposure [µs]: ", pRequest.chunkExposureTime.readS(), "\n",
                                "FrameRate [Hz]: ", Math.Round(Convert.ToDecimal(statistics_.framesPerSecond.read()), 2), " Set=", fps_, "\n",
                                "BlobObjectsFound: ", objectsFound, "\n",
                                "BlobRect: B=", rect.Bottom, " L=", rect.Left, " T=", rect.Top, " R=", rect.Right, "\n",
                                "FrameCount=", cnt, " Runtime=", (long)totalRuntimeSeconds++, "s BandwidthUsed=", (long)((double)totalBytesTransferred / (totalRuntimeSeconds * (1024.0 * 1024.0))), "Mbytes/s\n",
                                "Errors=", errorCount, " Requests=", imageRequestCount, " OK:", imageRequestCountOK, " Err:", imageRequestCountErr, " Got =", requestsArrived, " Diff=", imageRequestCountOK - requestsArrived);

                    lblStatistics.BeginInvoke(new Action(() =>
                    {
                        lblStatistics.Text = msg;
                    }));
                }
            }
        }

        public void resetQueue()
        {
            // clear all queues
            fi_.imageRequestReset(0, 0);
            while (((TDMR_ERROR)fi_.imageRequestSingle()) == TDMR_ERROR.DMR_NO_ERROR) { };
        }

        public void shutDown()
        {
            boSmartFrameRecallThreadRunning_ = false;
            smartFrameRecallThread_.Join();
            boAcquisitionThreadRunning_ = false;
            acquisitionThread_.Join();
            closeMvGenTLDevice();
        }

        public void saveSettingsAsXML(double exposureStepWidth, double gainStepWidth)
        {
            string sAppName = Application.ProductName.ToString();
            string sAppVersion = Application.ProductVersion.ToString();

            List<XMLPropertyDataSet> xmlSettings = new List<XMLPropertyDataSet>();
            xmlSettings.Add(new XMLPropertyDataSet("Camera", "Exposure", acqc_.exposureTime.readS(), XMLPropertyDataSet.PropertyDataType.pdtDouble));
            xmlSettings.Add(new XMLPropertyDataSet("Camera", "Gain", ac_.gain.readS(), XMLPropertyDataSet.PropertyDataType.pdtDouble));
            xmlSettings.Add(new XMLPropertyDataSet("Camera", "TriggerMode", boTriggerMode_.ToString(), XMLPropertyDataSet.PropertyDataType.pdtBool));
            xmlSettings.Add(new XMLPropertyDataSet("Camera", "FlashMode", boFlashMode_.ToString(), XMLPropertyDataSet.PropertyDataType.pdtBool));
            xmlSettings.Add(new XMLPropertyDataSet("Application", "AOIOffsetX", aoiOffsetX_.ToString(), XMLPropertyDataSet.PropertyDataType.pdtInteger));
            xmlSettings.Add(new XMLPropertyDataSet("Application", "AOIOffsetY", aoiOffsetY_.ToString(), XMLPropertyDataSet.PropertyDataType.pdtInteger));
            xmlSettings.Add(new XMLPropertyDataSet("Application", "ExposureStepWidth", exposureStepWidth.ToString(), XMLPropertyDataSet.PropertyDataType.pdtDouble));
            xmlSettings.Add(new XMLPropertyDataSet("Application", "GainStepWidth", gainStepWidth.ToString(), XMLPropertyDataSet.PropertyDataType.pdtDouble));
            xmlSettings.Add(new XMLPropertyDataSet("Application", "FPS", fps_.ToString(), XMLPropertyDataSet.PropertyDataType.pdtInteger));
            xmlSettings.Add(new XMLPropertyDataSet("Application", "MinBlobSize", minBlobSize_.ToString(), XMLPropertyDataSet.PropertyDataType.pdtInteger));
            xmlSettings.Add(new XMLPropertyDataSet("Application", "ScaleFactor", scaleFactor_.ToString(), XMLPropertyDataSet.PropertyDataType.pdtInteger));

            try
            {
                using (XmlWriter writer = XmlWriter.Create(settingsFileName_))
                {
                    writer.WriteStartDocument();
                    writer.WriteStartElement("Settings");
                    writer.WriteAttributeString("Application", sAppName);
                    writer.WriteAttributeString("Version", sAppVersion);

                    // camera specific settings
                    writer.WriteStartElement("Camera");
                    writer.WriteAttributeString("Model", pDev_.product.readS());
                    writer.WriteAttributeString("Serial", pDev_.serial.readS());
                    storeXMLElements(xmlSettings, writer, "Camera");
                    writer.WriteEndElement();

                    // application specific settings
                    writer.WriteStartElement("Application");
                    storeXMLElements(xmlSettings, writer, "Application");
                    writer.WriteEndElement();

                    writer.WriteEndDocument();
                }
            }
            catch (Exception e)
            {
                MessageBox.Show(String.Format("Failed to store application settings into XML file. {0}", e.Message));
            }
        }

        public void loadSettingsFromXML(bool boShowMessageOnError, out double exposureStepWidth, out double gainStepWidth)
        {
            exposureStepWidth = 50;
            gainStepWidth = 0.5;
            StringBuilder sb = new StringBuilder(1024);
            try
            {
                using (XmlReader reader = XmlReader.Create(settingsFileName_))
                {
                    while (reader.Read())
                    {
                        // Only detect start elements.
                        if (reader.IsStartElement())
                        {
                            switch (reader.Name)
                            {
                                case "Property":
                                    String propertyName = reader["Name"];
                                    reader.ReadToDescendant("Value");
                                    String value = reader.ReadElementString();
                                    String type = reader.ReadElementString();
                                    switch (propertyName)
                                    {
                                        case "Exposure":
                                            acqc_.exposureTime.writeS(value);
                                            break;
                                        case "Gain":
                                            ac_.gain.writeS(value);
                                            break;
                                        case "TriggerMode":
                                            if (bool.Parse(value))
                                            {
                                                acqc_.triggerSelector.writeS("FrameStart");
                                                acqc_.triggerSource.writeS("Line4");
                                                acqc_.triggerActivation.writeS("RisingEdge");
                                                acqc_.triggerMode.writeS("On");
                                                boTriggerMode_ = true;
                                            }
                                            else
                                            {
                                                acqc_.triggerMode.writeS("Off");
                                                boTriggerMode_ = false;
                                            }
                                            break;
                                        case "FlashMode":
                                            dioc_.userOutputSelector.writeS("UserOutput0");
                                            boFlashMode_ = !boFlashMode_;
                                            dioc_.userOutputValue.write(boFlashMode_ ? TBoolean.bTrue : TBoolean.bFalse);
                                            break;
                                        case "AOIOffsetX":
                                            aoiOffsetX_ = Int32.Parse(value);
                                            break;
                                        case "AOIOffsetY":
                                            aoiOffsetY_ = Int32.Parse(value);
                                            break;
                                        case "ExposureStepWidth":
                                            exposureStepWidth = Double.Parse(value);
                                            break;
                                        case "GainStepWidth":
                                            gainStepWidth = Double.Parse(value);
                                            break;
                                        case "FPS":
                                            fps_ = Int32.Parse(value);
                                            acqc_.acquisitionFrameRate.write(fps_);
                                            break;
                                        case "MinBlobSize":
                                            minBlobSize_ = Int32.Parse(value);
                                            break;
                                        case "ScaleFactor":
                                            scaleFactor_ = Int32.Parse(value);
                                            if ((scaleFactor_ != ifc_.decimationHorizontal.read()) &&
                                                acqc_.mvSmartFrameRecallEnable.isWriteable &&
                                                ifc_.decimationHorizontal.isWriteable &&
                                                ifc_.decimationVertical.isWriteable)
                                            {
                                                acqc_.mvSmartFrameRecallEnable.write(TBoolean.bFalse);
                                                ifc_.decimationHorizontal.write(scaleFactor_);
                                                ifc_.decimationVertical.write(scaleFactor_);
                                                acqc_.mvSmartFrameRecallEnable.write(TBoolean.bTrue);
                                            }
                                            break;
                                    }
                                    break;
                            }
                        }
                    }
                }
            }
            catch (Exception e)
            {
                if (boShowMessageOnError)
                {
                    MessageBox.Show(String.Format("Failed to apply stored application settings. {0}", e.Message));
                }
            }
        }

        #endregion
    }
}