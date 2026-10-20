namespace mv.impact.acquire.examples
{
    partial class DeviceSelectionForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(DeviceSelectionForm));
            this.deviceSelectionCombo = new System.Windows.Forms.ComboBox();
            this.deviceSelectionText = new System.Windows.Forms.Label();
            this.OKBtn = new System.Windows.Forms.Button();
            this.CancelBtn = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // deviceSelectionCombo
            // 
            this.deviceSelectionCombo.FormattingEnabled = true;
            this.deviceSelectionCombo.Location = new System.Drawing.Point(15, 34);
            this.deviceSelectionCombo.Name = "deviceSelectionCombo";
            this.deviceSelectionCombo.Size = new System.Drawing.Size(269, 21);
            this.deviceSelectionCombo.TabIndex = 1;
            // 
            // deviceSelectionText
            // 
            this.deviceSelectionText.AutoSize = true;
            this.deviceSelectionText.Location = new System.Drawing.Point(12, 9);
            this.deviceSelectionText.Name = "deviceSelectionText";
            this.deviceSelectionText.Size = new System.Drawing.Size(272, 13);
            this.deviceSelectionText.TabIndex = 0;
            this.deviceSelectionText.Text = "Please select the device you want to use for the session";
            // 
            // OKBtn
            // 
            this.OKBtn.Location = new System.Drawing.Point(134, 69);
            this.OKBtn.Name = "OKBtn";
            this.OKBtn.Size = new System.Drawing.Size(75, 23);
            this.OKBtn.TabIndex = 2;
            this.OKBtn.Text = "OK";
            this.OKBtn.UseVisualStyleBackColor = true;
            this.OKBtn.Click += new System.EventHandler(this.OKButton_Click);
            // 
            // CancelBtn
            // 
            this.CancelBtn.Location = new System.Drawing.Point(215, 69);
            this.CancelBtn.Name = "CancelBtn";
            this.CancelBtn.Size = new System.Drawing.Size(75, 23);
            this.CancelBtn.TabIndex = 3;
            this.CancelBtn.Text = "Cancel";
            this.CancelBtn.UseVisualStyleBackColor = true;
            this.CancelBtn.Click += new System.EventHandler(this.button1_Click);
            // 
            // DeviceSelectionForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(297, 104);
            this.Controls.Add(this.CancelBtn);
            this.Controls.Add(this.OKBtn);
            this.Controls.Add(this.deviceSelectionText);
            this.Controls.Add(this.deviceSelectionCombo);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "DeviceSelectionForm";
            this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
            this.Text = "Device Selection";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ComboBox deviceSelectionCombo;
        private System.Windows.Forms.Label deviceSelectionText;
        private System.Windows.Forms.Button OKBtn;
        private System.Windows.Forms.Button CancelBtn;
    }
}