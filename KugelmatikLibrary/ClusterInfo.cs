﻿using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace KugelmatikLibrary
{
    /// <summary>
    /// Stellt verschiedene Informationen über das Cluster bereit.
    /// </summary>
    public class ClusterInfo : IEquatable<ClusterInfo>
    {
        /// <summary>
        /// Gibt die Build-Version der Firmware des Clusters zurück.
        /// </summary>
        public byte BuildVersion { get; private set; }

        /// <summary>
        /// Gibt true zurück, wenn das Cluster einen Busy-Befehl ausführt.
        /// </summary>
        public BusyCommand CurrentBusyCommand { get; private set; }

        /// <summary>
        /// Gibt die größte Revision des Clusters zurück.
        /// </summary>
        public int HighestRevision { get; private set; }

        /// <summary>
        /// Gibt die Einstellungen des Clusters zurück.
        /// </summary>
        [TypeConverter(typeof(ExpandableObjectConverter))]
        public ClusterConfig Config { get; private set; }

        public ClusterInfo(byte buildVersion, BusyCommand currentBusyCommand, int highestRevision, ClusterConfig config)
        {
            this.BuildVersion = buildVersion;
            this.CurrentBusyCommand = currentBusyCommand;
            this.HighestRevision = highestRevision;
            this.Config = config;
        }

        public static bool operator ==(ClusterInfo a, ClusterInfo b)
        {
            if (object.ReferenceEquals(a, b))
                return true;
            if (object.ReferenceEquals(a, null))
                return false;
            return a.Equals(b);
        }

        public static bool operator !=(ClusterInfo a, ClusterInfo b)
        {
            return !(a == b);
        }

        public override bool Equals(object obj)
        {
            if (obj is ClusterInfo)
                return Equals(obj as ClusterInfo);
            return false;
        }

        public bool Equals(ClusterInfo other)
        {
            if (other == null)
                return false;
            return BuildVersion == other.BuildVersion 
                && CurrentBusyCommand == other.CurrentBusyCommand
                && HighestRevision == other.HighestRevision
                && Config == other.Config;
        }

        public override int GetHashCode()
        {
            unchecked
            {
                int hash = 13;
                hash = hash * 7 + BuildVersion.GetHashCode();
                hash = hash * 7 + CurrentBusyCommand.GetHashCode();
                hash = hash * 7 + HighestRevision.GetHashCode();
                hash = hash * 7 + Config.GetHashCode();
                return hash;
            }
        }
    }
}
