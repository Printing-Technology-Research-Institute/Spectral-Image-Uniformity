using mv.impact.acquire;
using mv.impact.acquire.examples.helper;
using System;
using System.Drawing;
using System.IO;
using System.Threading;
using System.Windows.Forms;

namespace ContinuousSaveImage
{
    public partial class ContinuousSaveImage : Form
    {
        string filePath;
        Device device;
        Thread thread;
        int imagesToSaveCount = 10;
        int requestCount = 10;
        string[] arrayImageFormat = { "bmp", "jpg", "png", "tif" };
        string imageFormat;
        bool terminated;

        public ContinuousSaveImage()
        {
            InitializeComponent();
        }

        private void Frm_Main_Load(object sender, EventArgs e)
        {
            // Initialize GUI parameters and device list.
            comboBoxImgFormat.Items.AddRange(arrayImageFormat);
            comboBoxImgFormat.SelectedItem = arrayImageFormat[0];
            btnAcquire.Enabled = false;
            btnAcquire.BackColor = Color.DarkGray;
            btnUse.BackColor = Color.Gainsboro;
            textBoxRequestCnt.Text = requestCount.ToString();
            textBoxNrImg.Text = imagesToSaveCount.ToString();
            InitDevice();
        }

        private bool TerminateCurrentCaptureThread()
        {
            if ((thread != null) && (thread.IsAlive))
            {
                terminated = true;
                thread.Join();
                return true;
            }
            return false;
        }

        private void ContinuousSaveImage_FormClosing(object sender, FormClosingEventArgs e)
        {
            TerminateCurrentCaptureThread();
        }

        private void btnBrowse_Click(object sender, EventArgs e)
        {
            // Choose a path for saving images via the folder browser dialog.
            FolderBrowserDialog fbd = new FolderBrowserDialog();
            if ((fbd.ShowDialog() == DialogResult.OK) && !string.IsNullOrWhiteSpace(fbd.SelectedPath))
            {
                filePath = fbd.SelectedPath;
                textBoxPath.Text = filePath;
            }
        }

        private void comboBoxCamera_SelectedIndexChanged(object sender, EventArgs e)
        {
            // If the previous device is still open, terminate the capture thread if needed, close the previous device and change the button color to gray.
            if (device.isOpen)
            {
                TerminateCurrentCaptureThread();
                device.close();
                btnUse.BackColor = Color.Gainsboro;
                btnAcquire.Enabled = false;
                btnAcquire.BackColor = Color.DarkGray;
                textBoxFPS.Clear();
                textBoxStatistics.Clear();
                progressBarSaving.Value = 0;
            }
        }

        private void btnUse_Click(object sender, EventArgs e)
        {
            // Warning when clicking the 'Use'-button without any camera chosen.
            if (comboBoxCamera.SelectedIndex == -1)
            {
                MessageBox.Show("Please select a camera!");
                return;
            }

            // If the 'Use'-button is gray, open the device and change the button color to blue.
            if (btnUse.BackColor == Color.Gainsboro)
            {
                try
                {
                    device = DeviceManager.getDevice(comboBoxCamera.SelectedIndex);
                    device.open();
                }
                catch (ImpactAcquireException ex)
                {
                    MessageBox.Show("An error occurred while opening the device: " + ex.errorCodeAsString);
                    return;
                }
                btnUse.BackColor = Color.PaleTurquoise;
                btnAcquire.Enabled = true;
                btnAcquire.BackColor = Color.LimeGreen;
            }
            // If the 'Use'-button is blue, terminate the capture thread if needed, close the current device and change the button color to gray.
            else
            {
                TerminateCurrentCaptureThread();
                device.close();
                btnUse.BackColor = Color.Gainsboro;
                btnAcquire.Enabled = false;
                btnAcquire.BackColor = Color.DarkGray;
                textBoxFPS.Clear();
                textBoxStatistics.Clear();
                progressBarSaving.Value = 0;
            }
        }

        private void btnAcquire_Click(object sender, EventArgs e)
        {
            // Warning when no saving path is given.
            if (string.IsNullOrEmpty(filePath))
            {
                MessageBox.Show("Please choose chose a valid path for saving image to!");
                return;
            }

            // Terminate the capture thread if the 'Acquire'-button is clicked during acquisition.
            if (TerminateCurrentCaptureThread())
            {
                btnAcquire.BackColor = Color.DarkGray;
                return;
            }

            // Set the 'Acquire'-button to green to indicate that the acquisition is now active.
            btnAcquire.BackColor = Color.LimeGreen;

            // Read the given request count.
            int requestCount = 0;
            if (!Int32.TryParse(textBoxRequestCnt.Text, out requestCount) || (requestCount < 2))
            {
                MessageBox.Show("Invalid request count. Please re-enter the request count (an integer ranged from 2 to 32768)!");
                return;
            }

            // Read the given number of images to save.
            int imageCount = 0;
            if (!Int32.TryParse(textBoxNrImg.Text, out imageCount) || (imageCount < 1))
            {
                MessageBox.Show("Invalid number of images to be saved. At least one image has to be captured. Please re-enter the value!");
                return;
            }

            // Initialize the parameters for the GUI output.
            progressBarSaving.Minimum = 0;
            progressBarSaving.Maximum = imageCount;
            imageFormat = comboBoxImgFormat.SelectedItem.ToString();
            textBoxFPS.Clear();
            textBoxStatistics.Clear();
            terminated = false;

            // Start capture thread.
            thread = new Thread(LiveThread);
            thread.Start();
        }

        private void InitDevice()
        {
            // This will add folders containing unmanaged libraries to the PATH variable.
            mv.impact.acquire.LibraryPath.init();

            // Close the program if no device is found.
            if (DeviceManager.deviceCount < 1)
            {
                MessageBox.Show("No camera found！");
                Environment.Exit(1);
            }

            // Create the device list.
            for (int deviceIndex = 0; deviceIndex < DeviceManager.deviceCount; deviceIndex++)
            {
                device = DeviceManager.getDevice(deviceIndex);
                comboBoxCamera.Items.Add(device.product.readS() + " " + device.serial.readS());
            }
        }

        private void LiveThread()
        {
            // Create instances to several camera setting modules.
            Statistics statistics = new Statistics(device);
            FunctionInterface fi = new FunctionInterface(device);
            SystemSettings ss = new SystemSettings(device);

            // Initialize parameters.
            Request request = null;
            Request previousRequest = null;
            int requestNr = Device.INVALID_ID;
            int savedImageCnt = 0;
            int imageCnt = 0;

            // Write the given request count to the driver.
            ss.requestCount.write(requestCount);

            // Reset the statistics
            statistics.reset();

            // Queue up all the request objects.
            TDMR_ERROR result = TDMR_ERROR.DMR_NO_ERROR;
            while ((result = (TDMR_ERROR)fi.imageRequestSingle()) == TDMR_ERROR.DMR_NO_ERROR) { };
            if (result != TDMR_ERROR.DEV_NO_FREE_REQUEST_AVAILABLE)
            {
                MessageBox.Show("'FunctionInterface.imageRequestSingle' returned with an unexpected result: " + ImpactAcquireException.getErrorCodeAsString(result));
                return;
            }

            // Start the acquisition loop.
            DeviceAccess.manuallyStartAcquisitionIfNeeded(device, fi);
            while ((imageCnt < imagesToSaveCount) && !terminated)
            {
                requestNr = fi.imageRequestWaitFor(500);
                request = fi.isRequestNrValid(requestNr) ? fi.getRequest(requestNr) : null;
                if (request != null)
                {
                    // Read out the current frame ID in the FPS text box.
                    string frameID = (device.interfaceLayout.read() == TDeviceInterfaceLayout.dilGenICam) ? request.infoFrameID.readS() : request.infoFrameNr.readS();
                    // Save the image if the current request is OK. Report frame error otherwise.
                    if (request.isOK)
                    {
                        // Output the FPS.
                        textBoxFPS.BeginInvoke((Action)delegate
                        {
                            textBoxFPS.Text = statistics.framesPerSecond.readS();
                        });
                        // Output the saving progress in the progress bar.
                        progressBarSaving.BeginInvoke((Action)delegate
                        {
                            progressBarSaving.Value = savedImageCnt;
                        });
                        // Save the current image with the given format.
                        request.save(Path.Combine(filePath, device.serial.readS() + "_" + frameID + "." + imageFormat));
                        savedImageCnt++;
                    }
                    else
                    {
                        textBoxStatistics.BeginInvoke((Action)delegate
                        {
                            textBoxStatistics.Text += "Frame " + frameID + " has error: " + request.requestResult.readS() + Environment.NewLine;
                        });
                    }
                    // Update the image count.
                    imageCnt = savedImageCnt + statistics.errorCount.read() + statistics.lostImagesCount.read();
                    // Unlock the previous request.
                    if (previousRequest != null)
                    {
                        previousRequest.unlock();
                    }
                    previousRequest = request;
                    // Push one empty request object to the request queue.
                    fi.imageRequestSingle();
                }
            }
            // Stop the acquisition.
            DeviceAccess.manuallyStopAcquisitionIfNeeded(device, fi);
            // Unlock the current request.
            if (request != null)
            {
                request.unlock();
            }
            // Clear all queues.
            fi.imageRequestReset(0, 0);

            // Change the 'Acquire'-button color to gray
            btnAcquire.BackColor = Color.DarkGray;

            // Display the error frame count and the lost frame count in the statistics box.
            if (!terminated)
            {
                textBoxStatistics.BeginInvoke((Action)delegate
                {
                    textBoxStatistics.Text += Environment.NewLine + savedImageCnt.ToString() +
                    "/" + imagesToSaveCount.ToString() + " images have been saved successfully." + Environment.NewLine;
                    textBoxStatistics.Text += "Error Frame Count: " + statistics.errorCount.readS() + Environment.NewLine;
                    textBoxStatistics.Text += "Lost Frame Count: " + statistics.lostImagesCount.readS();
                });
            }
        }
    }
}