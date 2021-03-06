﻿using System;
using System.Collections.Generic;

namespace KugelmatikLibrary
{
    public class Kugelmatik
    {
        /// <summary>
        /// Gibt die Config-Instanz zurück mit dem die Kugelmatik initalisiert wurde.
        /// </summary>
        public Config Config { get; set; }

        /// <summary>
        /// Gibt die ClusterConfig-Instanz zurück mit dem die Cluster eingestellt werden.
        /// </summary>
        public ClusterConfig ClusterConfig { get; set; }

        private Cluster[] clusters;

        /// <summary>
        /// Gibt die Anzahl der Schrittmotoren in X-Richtung zurück.
        /// </summary>
        public int StepperCountX
        {
            get
            {
                return Config.KugelmatikWidth * Cluster.Width;
            }
        }

        /// <summary>
        /// Gibt die Anzahl der Schrittmotoren in Y-Richtung zurück.
        /// </summary>
        public int StepperCountY
        {
            get
            {
                return Config.KugelmatikHeight * Cluster.Height;
            }
        }


        /// <summary>
        /// Gibt die Anzahl der Cluster in dieser Kugelmatik zurück.
        /// </summary>
        public int ClusterCount
        {
            get { return clusters.Length; }
        }

        /// <summary>
        /// Gibt das Cluster mit dem Index index zurück.
        /// </summary>
        /// <param name="index"></param>
        /// <returns></returns>
        public Cluster this[int index]
        {
            get { return GetClusterByIndex(index); }
        }

        /// <summary>
        /// Gibt zurück ob Pakete noch warten von einem Cluster bestätigt zu werden.
        /// </summary>
        public bool AnyPacketsAcknowledgePending
        {
            get
            {
                bool anyPacketsAcknowledgePending = false;
                foreach (Cluster cluster in clusters)
                    anyPacketsAcknowledgePending |= cluster.AnyPacketsAcknowledgePending;
                return anyPacketsAcknowledgePending;
            }
        }

        /// <summary>
        /// Gibt zurück ob ein Cluster verfügbar ist.
        /// </summary>
        public bool AnyClusterOnline
        {
            get
            {
                foreach (Cluster cluster in clusters)
                    if (cluster.IsOnline)
                        return true;
                return false;
            }
        }

        public Kugelmatik(Config config, ClusterConfig clusterConfig)
        {
            this.Config = config;
            this.ClusterConfig = clusterConfig;

            IAddressProvider addressProvider;
            if (!string.IsNullOrWhiteSpace(config.AddressFile))
                addressProvider = new FileAddressProvider(config.AddressFile);
            else if (!string.IsNullOrWhiteSpace(config.FixedIP))
                addressProvider = new FixedAddressProvider(config.FixedIP);
            else
                addressProvider = new KugelmatikAddressProvider();

            InitClusters(addressProvider);

            Log.Debug("ClusterConfig.Size: {0} bytes:", ClusterConfig.Size);
        }

        public Kugelmatik(Config config, ClusterConfig clusterConfig, IAddressProvider addressProvider)
        {
            this.Config = config;
            this.ClusterConfig = clusterConfig;
            InitClusters(addressProvider);
        }

        private void InitClusters(IAddressProvider addressProvider)
        {
            if (addressProvider == null)
                throw new ArgumentNullException("addressProvider");

            Log.Debug("Using adress provider: {0}", addressProvider.GetType().Name);

            clusters = new Cluster[Config.KugelmatikWidth * Config.KugelmatikHeight];
            for (int x = 0; x < Config.KugelmatikWidth; x++)
                for (int y = 0; y < Config.KugelmatikHeight; y++)
                    clusters[y * Config.KugelmatikWidth + x] = new Cluster(this, x, y, addressProvider.GetAddress(Config, x, y));
        }

        public void Dispose()
        {
            if (clusters != null)
                foreach (Cluster cluster in clusters)
                    cluster.Dispose();
        }

        public IEnumerable<Cluster> EnumerateClusters()
        {
            for (int i = 0; i < clusters.Length; i++)
                yield return clusters[i];
        }

        public IEnumerable<Stepper> EnumerateSteppers()
        {
            for (int i = 0; i < clusters.Length; i++)
            {
                Cluster cluster = clusters[i];
                for (int j = 0; j < cluster.StepperCount; j++)
                    yield return cluster[j];
            }
        }

        /// <summary>
        /// Gibt das Cluster mit dem Index index zurück.
        /// </summary>
        /// <param name="index"></param>
        /// <returns></returns>
        public Cluster GetClusterByIndex(int index)
        {
            if (index < 0 || index >= clusters.Length)
                throw new ArgumentOutOfRangeException("index");
            return clusters[index];
        }

        /// <summary>
        /// Gibt das Cluster für die Position x und y zurück.
        /// </summary>
        /// <param name="x">Die X-Koordiante der Position.</param>
        /// <param name="y">Die Y-Koordinate der Position.</param>
        /// <returns></returns>
        public Cluster GetClusterByPosition(int x, int y)
        {
            if (x < 0 || x >= Config.KugelmatikWidth)
                throw new ArgumentOutOfRangeException("x");
            if (y < 0 || y >= Config.KugelmatikHeight)
                throw new ArgumentOutOfRangeException("y");

            return clusters[y * Config.KugelmatikWidth + x];
        }

        /// <summary>
        /// Lässt alle Kugeln aller Cluster in der ganzen Kugelmatik auf eine Höhe fahren.
        /// </summary>
        /// <param name="height"></param>
        public void SetAllClusters(ushort height)
        {
            foreach (Cluster cluster in clusters)
                cluster.SetAllStepper(height);
        }

        /// <summary>
        /// Gibt einen Stepper für die Position x und y zurück.
        /// </summary>
        /// <param name="x">Die X-Koordiante der Position.</param>
        /// <param name="y">Die Y-Koordinate der Position.</param>
        /// <returns></returns>
        public Stepper GetStepperByPosition(int x, int y)
        {
            if (x < 0 || x >= StepperCountX)
                throw new ArgumentOutOfRangeException("x");
            if (y < 0 || y >= StepperCountY)
                throw new ArgumentOutOfRangeException("y");

            int cx = (int)(x / Cluster.Width);
            int cy = (int)(y / Cluster.Height);
            int sx = x % Cluster.Width;
            int sy = y % Cluster.Height;
            return GetClusterByPosition(cx, cy).GetStepperByPosition(sx, sy);
        }

        /// <summary>
        /// Sendet alle Daten die zur Aktualisierung der Kugelmatik notwendig sind.
        /// </summary>
        /// <returns>Gibt true zurück, wenn Pakete an ein Cluster geschickt wurden.</returns>
        public bool SendData()
        {
            return SendData(false);
        }

        /// <summary>
        /// Sendet alle Daten die zur Aktualisierung der Kugelmatik notwendig sind.
        /// </summary>
        /// <param name="guaranteed">Wert der angibt ob das Paket von den Clustern bestätigt werden muss.</param>
        /// <returns>Gibt true zurück, wenn Pakete an ein Cluster geschickt wurden.</returns>
        public bool SendData(bool guaranteed)
        {
            return SendData(guaranteed, false);
        }

        /// <summary>
        /// Sendet alle Daten die zur Aktualisierung der Kugelmatik notwendig sind.
        /// </summary>
        /// <param name="guaranteed">Wert der angibt ob das Paket von den Clustern bestätigt werden muss.</param>
        /// <param name="ignoreInvalid">Wert der angibt ob alle Stepper (auch valide) von den Clustern gesendet werden sollen.</param>
        /// <returns>Gibt true zurück, wenn Pakete an ein Cluster geschickt wurden.</returns>
        public bool SendData(bool guaranteed, bool allSteppers)
        {
            bool anyDataSent = false;
            foreach (Cluster cluster in clusters)
                anyDataSent |= cluster.SendData(guaranteed, allSteppers);
            return anyDataSent;
        }

        /// <summary>
        /// Sendet einen Ping-Befehl an alle Cluster.
        /// </summary>
        public void SendPing()
        {
            foreach (Cluster cluster in clusters)
                cluster.SendPing();
        }

        /// <summary>
        /// Sendet einen Home-Befehl an alle Cluster und setzt die Kugeln auf die Höhe 0.
        /// </summary>
        public void SendHome()
        {
            foreach (Cluster cluster in clusters)
                cluster.SendHome();
        }

        /// <summary>
        /// Sendet einen GetData-Befehl an alle Cluster.
        /// </summary>
        public void SendGetData()
        {
            foreach (Cluster cluster in clusters)
                cluster.SendGetData();
        }

        /// <summary>
        /// Sendet einen Info-Befehl an alle Cluster.
        /// </summary>
        public void SendInfo()
        {
            foreach (Cluster cluster in clusters)
                cluster.SendInfo();
        }

        /// <summary>
        /// Sendet einen Stop-Befehl an alle Cluster.
        /// </summary>
        public void SendStop()
        {
            foreach (Cluster cluster in clusters)
                cluster.SendStop();
        }

        /// <summary>
        /// Sendet einen Config-Befehl an alle Cluster.
        /// </summary>
        public void SendConfig(ClusterConfig config)
        {
            foreach (Cluster cluster in clusters)
                cluster.SendConfig(config);
        }


        /// <summary>
        /// Verschickt alle Pakete nochmal die noch von allen Cluster bestätigt werden müssen.
        /// </summary>
        /// <returns>Gibt true zurück, wenn Pakete gesendet wurden.</returns>
        public bool ResendPendingPackets()
        {
            bool anyDataSent = false;
            foreach (Cluster cluster in clusters)
                anyDataSent |= cluster.ResendPendingPackets();
            return anyDataSent;
        }

        public void SetStepper(int x, int y, ushort height)
        {
            GetStepperByPosition(x, y).Set(height);
        }

        public void SetRectangle(int x, int y, int rectWidth, int rectHeight, ushort stepperHeight)
        {
            if (x < 0 || x >= StepperCountX)
                throw new ArgumentOutOfRangeException("x");
            if (y < 0 || y >= StepperCountY)
                throw new ArgumentOutOfRangeException("x");
            if (rectWidth <= 0)
                throw new ArgumentOutOfRangeException("rectWidth");
            if (x + rectWidth > StepperCountX)
                throw new ArgumentOutOfRangeException("rectWidth");
            if (rectHeight <= 0)
                throw new ArgumentOutOfRangeException("rectHeight");
            if (y + rectHeight > StepperCountY)
                throw new ArgumentOutOfRangeException("rectHeight");

            for (int i = 0; i < rectWidth; i++)
                for (int j = 0; j < rectHeight; j++)
                    GetStepperByPosition(x + i , y + j).Set(stepperHeight);
        }

        public void SetCircle(int x, int y, float radius, ushort stepperHeight)
        {
            if (x < 0 || x >= StepperCountX)
                throw new ArgumentOutOfRangeException("x");
            if (y < 0 || y >= StepperCountY)
                throw new ArgumentOutOfRangeException("x");
            if (radius <= 0)
                throw new ArgumentOutOfRangeException("radius");
            if (x - radius < 0 || x + radius >= StepperCountX)
                throw new ArgumentOutOfRangeException("radius");
            if (y - radius < 0 || y + radius >= StepperCountY)
                throw new ArgumentOutOfRangeException("radius");

            int radiusInt = (int)Math.Ceiling(radius);
            for (int i = -radiusInt; i <= radiusInt; i++)
                for (int j = -radiusInt; j <= radiusInt; j++)
                    if (MathHelper.Distance(0, 0, i, j) <= radius)
                        GetStepperByPosition(x + i, y + j).Set(stepperHeight);
        }
    }
}
