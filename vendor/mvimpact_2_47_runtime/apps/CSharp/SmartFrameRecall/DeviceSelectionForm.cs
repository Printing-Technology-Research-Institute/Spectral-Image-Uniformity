using System;
using System.Windows.Forms;

namespace mv.impact.acquire.examples
{
    public partial class DeviceSelectionForm : Form
    {
        public DeviceSelectionForm()
        {
            InitializeComponent();
            int devCount = DeviceManager.deviceCount;
            for (int i = 0; i < devCount; i++)
            {
                Device pDev = DeviceManager.getDevice(i);
                if (pDev.interfaceLayout.isValid && pDev.interfaceLayout.listOfValidValues.Contains(TDeviceInterfaceLayout.dilGenICam))
                {
                    deviceSelectionCombo.Items.Add(String.Format("{0}({1})", pDev.serial.readS(), pDev.product.readS()));
                }
            }
        }

        public String selectedDeviceSerial
        {
            get
            {
                object obj = deviceSelectionCombo.SelectedItem;
                if (obj == null)
                {
                    return null;
                }
                String selection = obj.ToString();
                String[] tokens = selection.Split('(');
                return tokens[0];
            }
        }

        private void OKButton_Click(object sender, EventArgs e)
        {
            this.DialogResult = DialogResult.OK;
            Close();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            this.DialogResult = DialogResult.Cancel;
            Close();
        }
    }
}
