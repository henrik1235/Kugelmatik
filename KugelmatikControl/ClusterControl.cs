﻿using KugelmatikLibrary;
using System;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace KugelmatikControl
{
    public partial class ClusterControl : UserControl
    {
        public Cluster Cluster { get; private set; }

        public bool AutomaticUpdate { get; set; } = true;

        public ClusterControl(Cluster cluster)
        {
            if (cluster == null)
                throw new ArgumentNullException("cluster");

            this.Cluster = cluster;
            
            InitializeComponent();

            foreach (Stepper stepper in Cluster.EnumerateSteppers())
                stepper.OnHeightChange += Stepper_OnHeightChange;

            cluster.OnPingChange += UpdateClusterBox;
            cluster.OnInfoChange += UpdateClusterBox;

            UpdateClusterBox(cluster, EventArgs.Empty);
            UpdateGrid();

            SetupOnClick(this);
        }

        private void SetupOnClick(Control parent)
        {
            foreach (Control control in parent.Controls)
            {
                control.Click += Control_Click;
                SetupOnClick(control);
            }
        }

        private void Control_Click(object sender, EventArgs e)
        {
            InvokeOnClick(this, EventArgs.Empty);
        }

        public void UpdateSteppers()
        {
            UpdateGrid();
        }

        private void Stepper_OnHeightChange(object sender, EventArgs e)
        {
            if (!AutomaticUpdate)
                return;

            UpdateGrid();
        }

        private void UpdateClusterBox(object sender, EventArgs e)
        {
            if (InvokeRequired)
                BeginInvoke(new EventHandler(UpdateClusterBox), sender, e);
            else
            {
                SuspendLayout();
                clusterBox.Text = FormatClusterText(Cluster);

                if (Cluster.Info == null)
                    infoText.Visible = false;
                else
                {
                    string errorText = "";
                    if (Cluster.Info.LastError != ErrorCode.None)
                        errorText = string.Format("(error: {0})", Cluster.Info.LastError);

                    infoText.Visible = true;
                    infoText.Text = string.Format(Properties.Resources.ClusterInfoLong,
                        Cluster.Info.BuildVersion.ToString(), errorText);
                }

                SetClusterBoxColor(clusterBox, Cluster);
                ResumeLayout();
            }
        }

        public static string FormatClusterText(Cluster cluster)
        {
            return string.Format(Properties.Resources.ClusterInfo,
                    cluster.X + 1, cluster.Y + 1,
                    cluster.IsOnline ? (cluster.Ping + "ms") : "offline",
                    cluster.Address == null ? "none" : cluster.Address.ToString());
        }

        private void UpdateGrid()
        {
            if (gridText.InvokeRequired)
                gridText.BeginInvoke(new Action(UpdateGrid));
            else
            {
                StringBuilder builder = new StringBuilder(Cluster.Width * Cluster.Height * 5);
            
                for (int y = Cluster.Height - 1; y >= 0; y--)
                {
                    for (int x = 0; x < Cluster.Width; x++)
                        builder.Append(Cluster.GetStepperByPosition(x, y).Height.ToString().PadLeft(5));
                    builder.AppendLine();
                }
                gridText.Text = builder.ToString();
            }
        }

        /// <summary>
        /// Setzt die Farbe der GroupBox passend zum Status des Clusters.
        /// </summary>
        /// <param name="groupBox"></param>
        /// <param name="cluster"></param>
        public static void SetClusterBoxColor(GroupBox groupBox, Cluster cluster)
        {
            SetClusterBoxColor(groupBox, cluster.Ping, cluster.Info);
        }

        /// <summary>
        /// Setzt die Farbe der GroupBox passend zum Ping.
        /// </summary>
        /// <param name="groupBox"></param>
        /// <param name="cluster"></param>
        public static void SetClusterBoxColor(GroupBox groupBox, int ping, ClusterInfo info)
        {
            if (ping < 0)
                groupBox.ForeColor = Color.DarkRed;
            else if (info != null && info.CurrentBusyCommand != BusyCommand.None)
                groupBox.ForeColor = Color.DarkBlue;
            else if (info != null && info.LastError != ErrorCode.None)
                groupBox.ForeColor = Color.DarkMagenta;
            else if (ping >= 150)
                groupBox.ForeColor = Color.DarkOrange;
            else
                groupBox.ForeColor = Color.DarkGreen;

            // Farbe wieder zurücksetzen
            foreach (Control c in groupBox.Controls)
                c.ForeColor = SystemColors.ControlText;
        }
    }
}
