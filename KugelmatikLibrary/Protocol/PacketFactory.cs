﻿using System;
using System.Collections.Generic;

namespace KugelmatikLibrary.Protocol
{
    /// <summary>
    /// Stellt Methoden bereit um Pakete zu erstellen.
    /// </summary>
    public static class PacketFactory
    {
        public static IPacket CreatePacket(PacketType type)
        {
            Type packetType;
            if (!packetTypes.TryGetValue(type, out packetType))
                throw new NotImplementedException("PacketType is not implemented.");

            return (IPacket)Activator.CreateInstance(packetType);
        }

        private static Dictionary<PacketType, Type> packetTypes = new Dictionary<PacketType, Type>()
        {
            { PacketType.Ping, typeof(PacketPing) },
            { PacketType.Stepper, typeof(PacketStepper) },
            { PacketType.Steppers, typeof(PacketSteppers) },
            { PacketType.SteppersArray, typeof(PacketSteppersArray) },
            { PacketType.SteppersRectangle, typeof(PacketSteppersRectangle) },
            { PacketType.SteppersRectangleArray, typeof(PacketSteppersRectangleArray) },
            { PacketType.AllSteppers, typeof(PacketAllSteppers) },
            { PacketType.AllSteppersArray, typeof(PacketAllSteppersArray) },
            { PacketType.Home, typeof(PacketHome) },
            { PacketType.ResetRevision, typeof(PacketResetRevision) },
            { PacketType.Fix, typeof(PacketFix) },
            { PacketType.HomeStepper, typeof(PacketHomeStepper) },
            { PacketType.GetData, typeof(PacketGetData) },
            { PacketType.Info, typeof(PacketInfo) },
            { PacketType.BlinkGreen, typeof(PacketBlinkGreen) },
            { PacketType.BlinkRed, typeof(PacketBlinkRed) },
            { PacketType.Stop, typeof(PacketStop) },
            { PacketType.SetData, typeof(PacketSetData) },
            { PacketType.Config2, typeof(PacketConfig2) },
            { PacketType.ClearError, typeof(PacketClearError) },
            { PacketType.Restart, typeof(PacketRestart) },
            { PacketType.StartOTA, typeof(PacketStartOTA) }
        };
    }
}
