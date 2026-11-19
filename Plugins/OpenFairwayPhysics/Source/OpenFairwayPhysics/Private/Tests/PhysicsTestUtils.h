// SPDX-License-Identifier: MIT
// Helpers shared by the physics automation tests: launches, level and sloped ground, rolling distances and yards.
#pragma once

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "OpenFairway/ShotSimulator.h"

namespace OpenFairwayTest
{
    using namespace OpenFairway;

    inline const FVector3f LevelNormal(0.0f, 1.0f, 0.0f);
    /** A rolling ball that has not stopped after this long never will. */
    constexpr float MaxRollSeconds = 60.0f;

    /** A launch-monitor launch with measured back and side spin. */
    inline FLaunchData MakeLaunch(float SpeedMph, float VlaDeg, float HlaDeg, float BackSpinRpm, float SideSpinRpm)
    {
        FLaunchData Launch;
        Launch.SpeedMph = SpeedMph;
        Launch.VerticalLaunchDeg = VlaDeg;
        Launch.HorizontalLaunchDeg = HlaDeg;
        Launch.BackSpinRpm = BackSpinRpm;
        Launch.SideSpinRpm = SideSpinRpm;
        return Launch;
    }

    /** A straight launch with backspin only. */
    inline FLaunchData MakeLaunch(float SpeedMph, float VlaDeg, float BackSpinRpm)
    {
        return MakeLaunch(SpeedMph, VlaDeg, 0.0f, BackSpinRpm, 0.0f);
    }

    /** A launch given by total spin on a level axis, as published averages give it. */
    inline FLaunchData MakeTotalSpinLaunch(float SpeedMph, float VlaDeg, float TotalSpinRpm, float HlaDeg = 0.0f)
    {
        FLaunchData Launch;
        Launch.SpeedMph = SpeedMph;
        Launch.VerticalLaunchDeg = VlaDeg;
        Launch.HorizontalLaunchDeg = HlaDeg;
        Launch.TotalSpinRpm = TotalSpinRpm;
        Launch.SpinAxisDeg = 0.0f;
        return Launch;
    }

    /** Ground tilted downhill along the +X target line by SlopeDeg (negative is uphill). */
    inline FVector3f SlopeNormal(float SlopeDeg)
    {
        const float Slope = FMath::DegreesToRadians(SlopeDeg);
        return FVector3f(FMath::Sin(Slope), FMath::Cos(Slope), 0.0f);
    }

    inline float Yards(float Meters)
    {
        return Meters * YardsPerMeter;
    }

    /** Roll-out (total minus carry) in yards. */
    inline float RollYd(const FShotSimulationResult& Result)
    {
        return Yards(Result.TotalM - Result.CarryM);
    }

    /** Meters a ball already rolling at SpeedMps along +X travels on level ground of this surface before it rests. */
    inline float RollMeters(const FSurfaceSettings& Surface, float SpeedMps, float ImpactSpinRpm = 0.0f)
    {
        const FRolloutProfile Rollout;
        FBallState State;
        State.Velocity = FVector3f(SpeedMps, 0.0f, 0.0f);
        State.Spin = FVector3f::CrossProduct(LevelNormal, State.Velocity) / GolfPhysics::Radius;
        const int32 Steps = static_cast<int32>(MaxRollSeconds / GolfPhysics::SimulationDt);
        for (int32 Step = 0; Step < Steps && !GolfPhysics::CanRest(State, true, LevelNormal, Surface, Rollout); ++Step)
        {
            State = GolfPhysics::StepGround(State, LevelNormal, Surface, Rollout, ImpactSpinRpm, GolfPhysics::SimulationDt);
        }
        return State.Position.X;
    }
} // namespace OpenFairwayTest

#endif
