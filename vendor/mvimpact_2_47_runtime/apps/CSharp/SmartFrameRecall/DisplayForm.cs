using System;
using System.Drawing;
using System.Windows.Forms;

namespace mv.impact.acquire.examples
{
    public partial class DisplayForm : Form
    {
        private int iShift = 0;
        private Color cDisplayed = Color.Blue;
        private Color cNotDisplayed = Color.Gray;
        private double exposureStepWidth_ = 50;
        private double gainStepWidth_ = 0.5;

        private AcquisitonClass acqClass = new AcquisitonClass();

        public DisplayForm()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            IntPtr[] smallWindowHandeles = { pictureBoxSmallLive.Handle,
                                         pictureBoxSmallImage1.Handle, pictureBoxSmallImage2.Handle, pictureBoxSmallImage3.Handle, pictureBoxSmallImage4.Handle,
                                         pictureBoxSmallImage5.Handle, pictureBoxSmallImage6.Handle, pictureBoxSmallImage7.Handle, pictureBoxSmallImage8.Handle};
            PictureBox[] pictureBoxes = { pictureBoxSmallLive,
                                         pictureBoxSmallImage1, pictureBoxSmallImage2, pictureBoxSmallImage3, pictureBoxSmallImage4,
                                         pictureBoxSmallImage5, pictureBoxSmallImage6, pictureBoxSmallImage7, pictureBoxSmallImage8};
            DeviceSelectionForm dlg = new DeviceSelectionForm();
            if (dlg.ShowDialog(this) != DialogResult.OK)
            {
                Environment.Exit(1);
            }
            acqClass.setUp(dlg.selectedDeviceSerial);
            acqClass.loadSettingsFromXML(false, out exposureStepWidth_, out gainStepWidth_);
            /// acquire images and provide them to pictureBox1
            acqClass.acquireImages(pictureBoxLiveImage.Handle,
                pictureBoxLiveImage,
                smallWindowHandeles,
                pictureBoxes,
                statisticsLabel);
        }

        private void Form1_KeyDown(object sender, KeyEventArgs e)
        {
            switch (e.KeyCode)
            {
                case Keys.F1:
                    HelpForm hlp = new HelpForm(acqClass);
                    hlp.Show();
                    break;
                case Keys.F2:
                    Boolean fullRes = acqClass.toggleFullResolutionSmallView();
                    groupBoxSmallLive.Text = fullRes ? "Requested AOI (Full resolution)" : "Detected Blob Area (Preview reduced resolution)";
                    break;
                case Keys.F11:
                    acqClass.saveSettingsAsXML(exposureStepWidth_, gainStepWidth_);
                    break;
                case Keys.F10:
                    acqClass.loadSettingsFromXML(true, out exposureStepWidth_, out gainStepWidth_);
                    break;
                case Keys.Pause:
                    acqClass.toggleFlash();
                    break;
                case Keys.Scroll:
                    acqClass.toggleTriggerMode();
                    break;
                case Keys.Insert:
                    // increase exposure time
                    acqClass.changeExposure(+exposureStepWidth_);
                    break;
                case Keys.Delete:
                    // reduce exposure time
                    acqClass.changeExposure(-exposureStepWidth_);
                    break;
                case Keys.Home:
                    // increase gain
                    acqClass.changeGain(gainStepWidth_);
                    break;
                case Keys.End:
                    // reduce gain
                    acqClass.changeGain(-gainStepWidth_);
                    break;
                case Keys.PageUp:
                    // increase FPS
                    iShift = acqClass.changeFPS(10);
                    break;
                case Keys.PageDown:
                    // reduce FPS
                    iShift = acqClass.changeFPS(-10);
                    break;
                case Keys.Left:
                    acqClass.moveSmallAOI(-1, 0, 8);
                    break;
                case Keys.Right:
                    acqClass.moveSmallAOI(1, 0, 8);
                    break;
                case Keys.Up:
                    acqClass.moveSmallAOI(0, -1, 8);
                    break;
                case Keys.Down:
                    acqClass.moveSmallAOI(0, 1, 8);
                    break;
                case Keys.F12:
                    statisticsLabel.Visible = !statisticsLabel.Visible;
                    smartFrameRecallInfoPanel.Visible = !smartFrameRecallInfoPanel.Visible;
                    break;
            }
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            acqClass.shutDown();
            Environment.Exit(0);
        }
    }
}
