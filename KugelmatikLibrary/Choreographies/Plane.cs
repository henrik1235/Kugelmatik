﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace KugelmatikLibrary.Choreographies
{
    public class Plane : IChoreographyFunction
    {
        public TimeSpan CycleTime { get; private set; }
        public float Inclination { get; private set; }

        public Plane(TimeSpan cycleTime, float inclination)
        {
            this.CycleTime = cycleTime;
            this.Inclination = inclination;
        }

        public ushort GetHeight(Cluster cluster, TimeSpan time, int x, int y)
        {
            float cy = y / (float)cluster.Kugelmatik.StepperCountY;

            float maxSteps = cluster.Kugelmatik.ClusterConfig.MaxSteps;

            float dcy = cy - 0.5f;
            float range = Math.Abs(dcy) * maxSteps;
            float t = MathHelper.ConvertTime(time, CycleTime);
            t = (float)MathHelper.ConvertToOneToOne(t);

            double steps = 0.5f * maxSteps + range * t * Math.Sign(dcy) * Inclination;
            return (ushort)Math.Round(steps);
        }
    }
}
