using System.Windows.Forms;

namespace ContinuousSaveImage
{
    partial class ContinuousSaveImage
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ContinuousSaveImage));
            this.textBoxPath = new System.Windows.Forms.TextBox();
            this.btnBrowse = new System.Windows.Forms.Button();
            this.labelPath = new System.Windows.Forms.Label();
            this.labelCamera = new System.Windows.Forms.Label();
            this.btnUse = new System.Windows.Forms.Button();
            this.comboBoxCamera = new System.Windows.Forms.ComboBox();
            this.labelFPS = new System.Windows.Forms.Label();
            this.textBoxFPS = new System.Windows.Forms.TextBox();
            this.btnAcquire = new System.Windows.Forms.Button();
            this.progressBarSaving = new System.Windows.Forms.ProgressBar();
            this.labelSavingProgress = new System.Windows.Forms.Label();
            this.labelNrImg = new System.Windows.Forms.Label();
            this.textBoxNrImg = new System.Windows.Forms.TextBox();
            this.textBoxStatistics = new System.Windows.Forms.TextBox();
            this.labelRequestCnt = new System.Windows.Forms.Label();
            this.textBoxRequestCnt = new System.Windows.Forms.TextBox();
            this.labelImgFormat = new System.Windows.Forms.Label();
            this.comboBoxImgFormat = new System.Windows.Forms.ComboBox();
            this.groupBoxInputSection = new System.Windows.Forms.GroupBox();
            this.groupBoxOutputSection = new System.Windows.Forms.GroupBox();
            this.groupBoxInputSection.SuspendLayout();
            this.groupBoxOutputSection.SuspendLayout();
            this.SuspendLayout();
            // 
            // textBoxPath
            // 
            this.textBoxPath.Location = new System.Drawing.Point(202, 199);
            this.textBoxPath.Name = "textBoxPath";
            this.textBoxPath.Size = new System.Drawing.Size(682, 23);
            this.textBoxPath.TabIndex = 0;
            // 
            // btnBrowse
            // 
            this.btnBrowse.BackColor = System.Drawing.Color.Gainsboro;
            this.btnBrowse.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.btnBrowse.Location = new System.Drawing.Point(903, 190);
            this.btnBrowse.Name = "btnBrowse";
            this.btnBrowse.Size = new System.Drawing.Size(114, 36);
            this.btnBrowse.TabIndex = 1;
            this.btnBrowse.Text = "Browse";
            this.btnBrowse.UseVisualStyleBackColor = false;
            this.btnBrowse.Click += new System.EventHandler(this.btnBrowse_Click);
            // 
            // labelPath
            // 
            this.labelPath.AutoSize = true;
            this.labelPath.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.labelPath.Location = new System.Drawing.Point(15, 200);
            this.labelPath.Name = "labelPath";
            this.labelPath.Size = new System.Drawing.Size(156, 17);
            this.labelPath.TabIndex = 2;
            this.labelPath.Text = "Path for saving images:";
            // 
            // labelCamera
            // 
            this.labelCamera.AutoSize = true;
            this.labelCamera.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.labelCamera.Location = new System.Drawing.Point(16, 253);
            this.labelCamera.Name = "labelCamera";
            this.labelCamera.Size = new System.Drawing.Size(123, 17);
            this.labelCamera.TabIndex = 3;
            this.labelCamera.Text = "Choose a camera:";
            // 
            // btnUse
            // 
            this.btnUse.BackColor = System.Drawing.Color.Gainsboro;
            this.btnUse.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.btnUse.Location = new System.Drawing.Point(903, 244);
            this.btnUse.Name = "btnUse";
            this.btnUse.Size = new System.Drawing.Size(114, 35);
            this.btnUse.TabIndex = 5;
            this.btnUse.Text = "Use";
            this.btnUse.UseVisualStyleBackColor = false;
            this.btnUse.Click += new System.EventHandler(this.btnUse_Click);
            // 
            // comboBoxCamera
            // 
            this.comboBoxCamera.FormattingEnabled = true;
            this.comboBoxCamera.Location = new System.Drawing.Point(202, 252);
            this.comboBoxCamera.Name = "comboBoxCamera";
            this.comboBoxCamera.Size = new System.Drawing.Size(682, 24);
            this.comboBoxCamera.TabIndex = 6;
            this.comboBoxCamera.SelectedIndexChanged += new System.EventHandler(this.comboBoxCamera_SelectedIndexChanged);
            // 
            // labelFPS
            // 
            this.labelFPS.AutoSize = true;
            this.labelFPS.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.labelFPS.Location = new System.Drawing.Point(16, 42);
            this.labelFPS.Name = "labelFPS";
            this.labelFPS.Size = new System.Drawing.Size(38, 17);
            this.labelFPS.TabIndex = 7;
            this.labelFPS.Text = "FPS:";
            // 
            // textBoxFPS
            // 
            this.textBoxFPS.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.textBoxFPS.Location = new System.Drawing.Point(202, 44);
            this.textBoxFPS.Name = "textBoxFPS";
            this.textBoxFPS.Size = new System.Drawing.Size(682, 16);
            this.textBoxFPS.TabIndex = 8;
            // 
            // btnAcquire
            // 
            this.btnAcquire.BackColor = System.Drawing.Color.DarkGray;
            this.btnAcquire.Font = new System.Drawing.Font("Microsoft Sans Serif", 14F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.btnAcquire.ForeColor = System.Drawing.Color.White;
            this.btnAcquire.Location = new System.Drawing.Point(423, 359);
            this.btnAcquire.Name = "btnAcquire";
            this.btnAcquire.Size = new System.Drawing.Size(324, 63);
            this.btnAcquire.TabIndex = 9;
            this.btnAcquire.Text = "Acquire";
            this.btnAcquire.UseVisualStyleBackColor = false;
            this.btnAcquire.Click += new System.EventHandler(this.btnAcquire_Click);
            // 
            // progressBarSaving
            // 
            this.progressBarSaving.Location = new System.Drawing.Point(19, 93);
            this.progressBarSaving.Name = "progressBarSaving";
            this.progressBarSaving.Size = new System.Drawing.Size(998, 32);
            this.progressBarSaving.TabIndex = 10;
            // 
            // labelSavingProgress
            // 
            this.labelSavingProgress.AutoSize = true;
            this.labelSavingProgress.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.labelSavingProgress.Location = new System.Drawing.Point(16, 73);
            this.labelSavingProgress.Name = "labelSavingProgress";
            this.labelSavingProgress.Size = new System.Drawing.Size(111, 17);
            this.labelSavingProgress.TabIndex = 11;
            this.labelSavingProgress.Text = "Saving progress";
            // 
            // labelNrImg
            // 
            this.labelNrImg.AutoSize = true;
            this.labelNrImg.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.labelNrImg.Location = new System.Drawing.Point(15, 94);
            this.labelNrImg.Name = "labelNrImg";
            this.labelNrImg.Size = new System.Drawing.Size(181, 17);
            this.labelNrImg.TabIndex = 12;
            this.labelNrImg.Text = "Number of images to save: ";
            // 
            // textBoxNrImg
            // 
            this.textBoxNrImg.Location = new System.Drawing.Point(202, 93);
            this.textBoxNrImg.Name = "textBoxNrImg";
            this.textBoxNrImg.Size = new System.Drawing.Size(682, 23);
            this.textBoxNrImg.TabIndex = 13;
            // 
            // textBoxStatistics
            // 
            this.textBoxStatistics.BackColor = System.Drawing.SystemColors.MenuBar;
            this.textBoxStatistics.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.textBoxStatistics.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.textBoxStatistics.Location = new System.Drawing.Point(19, 149);
            this.textBoxStatistics.Multiline = true;
            this.textBoxStatistics.Name = "textBoxStatistics";
            this.textBoxStatistics.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.textBoxStatistics.Size = new System.Drawing.Size(995, 95);
            this.textBoxStatistics.TabIndex = 15;
            // 
            // labelRequestCnt
            // 
            this.labelRequestCnt.AutoSize = true;
            this.labelRequestCnt.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.labelRequestCnt.Location = new System.Drawing.Point(16, 40);
            this.labelRequestCnt.Name = "labelRequestCnt";
            this.labelRequestCnt.Size = new System.Drawing.Size(104, 17);
            this.labelRequestCnt.TabIndex = 16;
            this.labelRequestCnt.Text = "Request count:";
            // 
            // textBoxRequestCnt
            // 
            this.textBoxRequestCnt.Location = new System.Drawing.Point(202, 39);
            this.textBoxRequestCnt.Name = "textBoxRequestCnt";
            this.textBoxRequestCnt.Size = new System.Drawing.Size(682, 23);
            this.textBoxRequestCnt.TabIndex = 17;
            // 
            // labelImgFormat
            // 
            this.labelImgFormat.AutoSize = true;
            this.labelImgFormat.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.labelImgFormat.Location = new System.Drawing.Point(15, 146);
            this.labelImgFormat.Name = "labelImgFormat";
            this.labelImgFormat.Size = new System.Drawing.Size(94, 17);
            this.labelImgFormat.TabIndex = 19;
            this.labelImgFormat.Text = "Image format:";
            // 
            // comboBoxImgFormat
            // 
            this.comboBoxImgFormat.FormattingEnabled = true;
            this.comboBoxImgFormat.Location = new System.Drawing.Point(202, 145);
            this.comboBoxImgFormat.Name = "comboBoxImgFormat";
            this.comboBoxImgFormat.Size = new System.Drawing.Size(681, 24);
            this.comboBoxImgFormat.TabIndex = 20;
            // 
            // groupBoxInputSection
            // 
            this.groupBoxInputSection.BackColor = System.Drawing.Color.WhiteSmoke;
            this.groupBoxInputSection.Controls.Add(this.comboBoxImgFormat);
            this.groupBoxInputSection.Controls.Add(this.labelImgFormat);
            this.groupBoxInputSection.Controls.Add(this.textBoxRequestCnt);
            this.groupBoxInputSection.Controls.Add(this.labelRequestCnt);
            this.groupBoxInputSection.Controls.Add(this.textBoxNrImg);
            this.groupBoxInputSection.Controls.Add(this.labelNrImg);
            this.groupBoxInputSection.Controls.Add(this.comboBoxCamera);
            this.groupBoxInputSection.Controls.Add(this.btnUse);
            this.groupBoxInputSection.Controls.Add(this.labelCamera);
            this.groupBoxInputSection.Controls.Add(this.labelPath);
            this.groupBoxInputSection.Controls.Add(this.btnBrowse);
            this.groupBoxInputSection.Controls.Add(this.textBoxPath);
            this.groupBoxInputSection.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.groupBoxInputSection.Location = new System.Drawing.Point(56, 33);
            this.groupBoxInputSection.Name = "groupBoxInputSection";
            this.groupBoxInputSection.Size = new System.Drawing.Size(1034, 307);
            this.groupBoxInputSection.TabIndex = 21;
            this.groupBoxInputSection.TabStop = false;
            this.groupBoxInputSection.Text = "Input Section";
            // 
            // groupBoxOutputSection
            // 
            this.groupBoxOutputSection.BackColor = System.Drawing.Color.WhiteSmoke;
            this.groupBoxOutputSection.Controls.Add(this.textBoxStatistics);
            this.groupBoxOutputSection.Controls.Add(this.labelSavingProgress);
            this.groupBoxOutputSection.Controls.Add(this.progressBarSaving);
            this.groupBoxOutputSection.Controls.Add(this.textBoxFPS);
            this.groupBoxOutputSection.Controls.Add(this.labelFPS);
            this.groupBoxOutputSection.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.groupBoxOutputSection.Location = new System.Drawing.Point(57, 438);
            this.groupBoxOutputSection.Name = "groupBoxOutputSection";
            this.groupBoxOutputSection.Size = new System.Drawing.Size(1033, 269);
            this.groupBoxOutputSection.TabIndex = 22;
            this.groupBoxOutputSection.TabStop = false;
            this.groupBoxOutputSection.Text = "Output Section";
            // 
            // ContinuousSaveImage
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.WhiteSmoke;
            this.ClientSize = new System.Drawing.Size(1159, 732);
            this.Controls.Add(this.groupBoxOutputSection);
            this.Controls.Add(this.groupBoxInputSection);
            this.Controls.Add(this.btnAcquire);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "ContinuousSaveImage";
            this.Text = "MATRIX VISION Example Application - Continuously Save Images During Live Acquisit" +
    "ion";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.ContinuousSaveImage_FormClosing);
            this.Load += new System.EventHandler(this.Frm_Main_Load);
            this.groupBoxInputSection.ResumeLayout(false);
            this.groupBoxInputSection.PerformLayout();
            this.groupBoxOutputSection.ResumeLayout(false);
            this.groupBoxOutputSection.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.TextBox textBoxPath;
        private System.Windows.Forms.Button btnBrowse;
        private System.Windows.Forms.Label labelPath;
        private System.Windows.Forms.Label labelCamera;
        private System.Windows.Forms.Button btnUse;
        private System.Windows.Forms.ComboBox comboBoxCamera;
        private System.Windows.Forms.Label labelFPS;
        private System.Windows.Forms.TextBox textBoxFPS;
        private System.Windows.Forms.Button btnAcquire;
        private System.Windows.Forms.ProgressBar progressBarSaving;
        private System.Windows.Forms.Label labelSavingProgress;
        private System.Windows.Forms.Label labelNrImg;
        private System.Windows.Forms.TextBox textBoxNrImg;
        private System.Windows.Forms.TextBox textBoxStatistics;
        private System.Windows.Forms.Label labelRequestCnt;
        private System.Windows.Forms.TextBox textBoxRequestCnt;
        private System.Windows.Forms.Label labelImgFormat;
        private System.Windows.Forms.ComboBox comboBoxImgFormat;
        private GroupBox groupBoxInputSection;
        private GroupBox groupBoxOutputSection;
    }
}

