using System;
using System.Drawing;
using System.Windows.Forms;

namespace mv.impact.acquire.examples
{
    public partial class HelpForm : Form
    {
        public HelpForm(AcquisitonClass acqClass)
        {
            InitializeComponent();
        }

        private void populateHotkeys()
        {
            dataGridViewHotkeys.ColumnCount = 2;

            dataGridViewHotkeys.ColumnHeadersDefaultCellStyle.BackColor = Color.Navy;
            dataGridViewHotkeys.ColumnHeadersDefaultCellStyle.ForeColor = Color.White;
            dataGridViewHotkeys.ColumnHeadersDefaultCellStyle.Font = new Font(dataGridViewHotkeys.Font, FontStyle.Bold);

            dataGridViewHotkeys.Name = "dataGridViewHotkeys";
            dataGridViewHotkeys.AutoSizeRowsMode = DataGridViewAutoSizeRowsMode.DisplayedCellsExceptHeaders;
            dataGridViewHotkeys.ColumnHeadersBorderStyle = DataGridViewHeaderBorderStyle.Single;
            dataGridViewHotkeys.CellBorderStyle = DataGridViewCellBorderStyle.Single;
            dataGridViewHotkeys.GridColor = Color.Black;
            dataGridViewHotkeys.RowHeadersVisible = false;

            dataGridViewHotkeys.Columns[0].Name = "Key";
            dataGridViewHotkeys.Columns[1].Name = "Function";

            dataGridViewHotkeys.Columns[0].Width = dataGridViewHotkeys.Width / 3;
            dataGridViewHotkeys.Columns[1].Width = dataGridViewHotkeys.Width - 3 - (dataGridViewHotkeys.Width / 3);

            dataGridViewHotkeys.Rows.Add(new string[] { Keys.F1.ToString(), "Show Help Window" });
            dataGridViewHotkeys.Rows.Add(new string[] { Keys.F2.ToString(), "Toggle LiveView Blob Resolution" });
            dataGridViewHotkeys.Rows.Add(new string[] { Keys.Scroll.ToString(), "Toggle Trigger Mode" });
            dataGridViewHotkeys.Rows.Add(new string[] { Keys.Insert.ToString(), "Increase Exposure" });
            dataGridViewHotkeys.Rows.Add(new string[] { Keys.Delete.ToString(), "Decrease Exposure" });
            dataGridViewHotkeys.Rows.Add(new string[] { Keys.Home.ToString(), "Increase Gain" });
            dataGridViewHotkeys.Rows.Add(new string[] { Keys.End.ToString(), "Decrease Gain" });
            dataGridViewHotkeys.Rows.Add(new string[] { Keys.PageUp.ToString(), "Increase FPS Value" });
            dataGridViewHotkeys.Rows.Add(new string[] { Keys.PageDown.ToString(), "Decrease FPS Value" });
            dataGridViewHotkeys.Rows.Add(new string[] { Keys.F10.ToString(), "Load stored settings" });
            dataGridViewHotkeys.Rows.Add(new string[] { Keys.F11.ToString(), "Save current settings" });
            dataGridViewHotkeys.Rows.Add(new string[] { Keys.F12.ToString(), "Toggle Statistics Window" });
        }

        private void HelpForm_Load(object sender, EventArgs e)
        {
            populateHotkeys();
        }
    }
}
