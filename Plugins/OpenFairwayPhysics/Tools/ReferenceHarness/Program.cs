// SPDX-License-Identifier: MIT
// Reference harness: mirrors ogolf-physics PhysicsAdapter.SimulateShotInternal (flat ground)
// on top of the real Godot-free core sources, to produce golden values for the C++ port.
// Usage: dotnet run --project <this folder> [-- <ogolf-physics dir> [shot.json[@Surface] ...]]
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Numerics;
using System.Text.Json;
using Core = OpenFairway.Physics;

static class Program
{
    const float MpsPerMph = 0.44704f;
    const float RadPerRpm = 0.10472f;
    const float YardsPerMeter = 1.09361f;
    const float FeetPerMeter = 3.28084f;
    const float Dt = 1.0f / 120.0f;
    const float MaxTime = 30.0f;
    const float DegToRadF = MathF.PI / 180f;

    static Vector3 Rotated(Vector3 v, Vector3 axis, float angle)
    {
        // Godot Basis(axis, angle) * v.
        float c = MathF.Cos(angle), s = MathF.Sin(angle), t = 1f - c;
        float r00 = axis.X * axis.X + c * (1f - axis.X * axis.X);
        float r11 = axis.Y * axis.Y + c * (1f - axis.Y * axis.Y);
        float r22 = axis.Z * axis.Z + c * (1f - axis.Z * axis.Z);
        float r01 = axis.X * axis.Y * t - axis.Z * s, r10 = axis.X * axis.Y * t + axis.Z * s;
        float r02 = axis.X * axis.Z * t + axis.Y * s, r20 = axis.X * axis.Z * t - axis.Y * s;
        float r12 = axis.Y * axis.Z * t - axis.X * s, r21 = axis.Y * axis.Z * t + axis.X * s;
        return new Vector3(r00 * v.X + r01 * v.Y + r02 * v.Z, r10 * v.X + r11 * v.Y + r12 * v.Z, r20 * v.X + r21 * v.Y + r22 * v.Z);
    }

    // The cases in Private/Tests/PhysicsFixtureTest.cpp.
    static readonly string[] DefaultShots =
    {
        "wood_low_test_shot.json", "drive_test_shot.json", "approach_mid_iron_test_shot.json", "wedge_test_shot.json",
        "chip_test_shot.json", "bump_test_shot.json", "checked_test_shot.json", "flop_test_shot.json", "topped_test_shot.json",
        "5iron.json", "driver1.json", "p_wedge_shot_1.json", "bump_and_run.json", "wedge_test_shot.json@Green",
        "approach_mid_iron_test_shot.json@Rough", "approach_mid_iron_test_shot.json@Sand", "wood_low_test_shot.json@Firm",
        "wood_low_test_shot.json@FairwaySoft", "bump_and_run.json@Fringe", "putt_test_shot.json@Green", "checked_test_shot.json@Green",
    };

    static void Main(string[] args)
    {
        string harnessDir = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "../../../"));
        string ogolfDir = args.Length > 0 ? args[0] : Path.Combine(harnessDir, "../../../../../ogolf-physics");
        string[] dataDirs = { Path.Combine(ogolfDir, "assets/data"), Path.Combine(harnessDir, "data") };
        var results = new List<string>();
        foreach (string file in args.Length > 1 ? args[1..] : DefaultShots)
        {
            string surfaceName = "Fairway";
            string name = file;
            int at = file.IndexOf('@');
            if (at >= 0) { surfaceName = file[(at + 1)..]; name = file[..at]; }
            var surface = Enum.Parse<PhysicsEnums.SurfaceType>(surfaceName);
            string path = Array.Find(dataDirs, dir => File.Exists(Path.Combine(dir, name))) is { } found
                ? Path.Combine(found, name)
                : throw new FileNotFoundException($"{name} not found in {string.Join(", ", dataDirs)}");
            using var doc = JsonDocument.Parse(File.ReadAllText(path));
            JsonElement ball = doc.RootElement.TryGetProperty("BallData", out var b) ? b : doc.RootElement;
            results.Add(Simulate(name, ball, surface));
        }
        Console.WriteLine(string.Join("\n", results));
    }

    static bool Has(JsonElement e, string k) => e.TryGetProperty(k, out _);
    static float Get(JsonElement e, string k) => e.TryGetProperty(k, out var v) ? (float)v.GetDouble() : 0f;

    static string Simulate(string name, JsonElement ball, PhysicsEnums.SurfaceType surfaceType)
    {
        float speedMph = Get(ball, "Speed"), vla = Get(ball, "VLA"), hla = Get(ball, "HLA");
        // ShotSetup.ParseSpin
        bool hasB = Has(ball, "BackSpin"), hasS = Has(ball, "SideSpin"), hasT = Has(ball, "TotalSpin"), hasA = Has(ball, "SpinAxis");
        float backspin = Get(ball, "BackSpin"), sidespin = Get(ball, "SideSpin"), total = Get(ball, "TotalSpin"), axis = Get(ball, "SpinAxis");
        if (total == 0f && (hasB || hasS)) total = MathF.Sqrt(backspin * backspin + sidespin * sidespin);
        if (!hasA && (hasB || hasS)) axis = MathF.Atan2(sidespin, backspin) * (180f / MathF.PI);
        if (hasT && hasA)
        {
            if (!hasB) backspin = total * MathF.Cos(axis * DegToRadF);
            if (!hasS) sidespin = total * MathF.Sin(axis * DegToRadF);
        }
        if (hasB && hasS && hasT)
        {
            float computed = MathF.Sqrt(backspin * backspin + sidespin * sidespin);
            if (MathF.Abs(computed - total) > 1f) { total = computed; axis = MathF.Atan2(sidespin, backspin) * (180f / MathF.PI); }
        }

        var profile = new BallPhysicsProfile();
        // ShotSetup.BuildLaunchVectorsFromComponents
        float speedMps = speedMph * MpsPerMph;
        Vector3 up = Vector3.UnitY, forward = new(0, 0, -1);
        Vector3 velocity = Rotated(Rotated(new Vector3(speedMps, 0, 0), forward, -vla * DegToRadF), up, -hla * DegToRadF);
        Vector3 omega = Rotated(new Vector3(0, -sidespin * RadPerRpm, backspin * RadPerRpm), up, -hla * DegToRadF);
        omega = Core.GolfPhysics.LaunchSpin(velocity, omega, up, BounceProfile.Default);
        Vector3 flat = new(velocity.X, 0, velocity.Z);
        Vector3 shotDir = flat.Length() > 0.001f ? Vector3.Normalize(flat) : Vector3.UnitX;

        // PhysicsParamsFactory.Create (drag/lift 1, launch inputs from the shot)
        float tempC = (75f - 32f) * 5f / 9f;
        float airDensity = Core.Atmosphere.Density(0f, tempC);
        float airViscosity = Core.Atmosphere.DynamicViscosity(tempC);
        var s = SurfacePhysicsCatalog.Get(surfaceType);
        var regime = profile.ResolveScaleOverride(speedMph, vla, total, out string regimeKey, out string matched);
        float rolling = surfaceType == PhysicsEnums.SurfaceType.Green ? GreenSpeed.RollingFriction(profile.GreenStimpFeet) : s.RollingFriction;
        var landing = profile.ResolveLandingResponse(vla, total, out LandingBucketKind bucket);
        float dragScale = profile.DragScaleMultiplier * regime.DragScaleMultiplier;
        float liftScale = profile.LiftScaleMultiplier * regime.LiftScaleMultiplier;
        var surface = new Core.SurfaceSettings(
            s.KineticFriction * profile.KineticFrictionMultiplier * regime.KineticFrictionMultiplier,
            rolling * profile.RollingFrictionMultiplier * regime.RollingFrictionMultiplier,
            s.GrassViscosity * profile.GrassViscosityMultiplier * regime.GrassViscosityMultiplier,
            s.SpinbackResponseScale,
            s.CriticalAngle + profile.CriticalAngleOffsetRadians + regime.CriticalAngleOffsetRadians,
            s.SpinbackThetaBoostMax * profile.SpinbackThetaBoostMultiplier * regime.SpinbackThetaBoostMultiplier,
            s.SpinbackSpinStartRpm, s.SpinbackSpinEndRpm, s.SpinbackSpeedStartMps, s.SpinbackSpeedEndMps,
            s.RollingSpinScale, s.RestitutionScale, s.ImpactRetentionScale, s.SurfaceBounce);
        var env = new Core.FlightEnvironment(airDensity, airViscosity, dragScale, liftScale, vla);
        FlightProfile fp = profile.ResolvedFlight;
        BounceProfile bp = profile.ResolvedBounce;
        RolloutProfile rp = profile.ResolvedRollout;
        float rolloutImpactSpin = 0f;

        Vector3 n = up;
        Vector3 pos = n * Core.GolfPhysics.Radius;
        bool flight = true, onGround = false, carryRecorded = false, rest = false;
        float carry = 0, hang = 0, apex = pos.Y, landingSpeed = 0, landingAngle = 0;
        int steps = (int)(MaxTime / Dt);
        for (int i = 0; i < steps; i++)
        {
            if (!onGround)
            {
                var sample = FlightAerodynamicsModel.Sample(velocity, omega, airDensity, airViscosity, dragScale, liftScale, vla, fp);
                Vector3 prev = pos;
                var next = Core.GolfPhysics.StepFlight(new Core.BallState(Vector3.Zero, velocity, omega), env, sample, Dt);
                velocity = next.Velocity; omega = next.Spin;
                pos += velocity * Dt;
                apex = MathF.Max(apex, pos.Y);
                float d = Vector3.Dot(pos, n) - Core.GolfPhysics.Radius;
                if (d <= 0 && Vector3.Dot(velocity, n) < -0.01f)
                {
                    float pd = Vector3.Dot(prev, n) - Core.GolfPhysics.Radius;
                    float f = pd > 0 ? Math.Clamp(pd / (pd - d), 0, 1) : 0;
                    pos = Vector3.Lerp(prev, pos, f);
                    pos += n * (Core.GolfPhysics.Radius - Vector3.Dot(pos, n));
                    float preSpeed = velocity.Length();
                    float preNormal = MathF.Abs(Vector3.Dot(velocity, n));
                    Vector3 preTan = velocity - n * Vector3.Dot(velocity, n);
                    if (!carryRecorded) rolloutImpactSpin = omega.Length() / RadPerRpm;
                    var resolved = Core.GolfPhysics.ResolveImpact(new Core.BallState(Vector3.Zero, velocity, omega), n, surface, bp, flight, landing);
                    bool wasFlight = flight;
                    if (Vector3.Dot(velocity, n) < 0 && flight) flight = false;
                    velocity = resolved.Velocity; omega = resolved.Spin;
                    onGround = !flight && !Core.SurfaceBounceSettings.HasRebound(velocity, n, surface.SurfaceBounce);
                    if (onGround) velocity -= n * Vector3.Dot(velocity, n);
                    if (!carryRecorded)
                    {
                        landingSpeed = preSpeed;
                        landingAngle = MathF.Atan2(preNormal, MathF.Max(preTan.Length(), 0.0001f)) * (180f / MathF.PI);
                        carry = MathF.Max(Vector3.Dot(pos, shotDir), 0);
                        carryRecorded = true;
                        hang = (i + f) * Dt;
                    }
                    pos += velocity * (1f - f) * Dt;
                }
            }
            else
            {
                var next = Core.GolfPhysics.StepGround(new Core.BallState(Vector3.Zero, velocity, omega), n, surface, rp, rolloutImpactSpin, Dt);
                velocity = next.Velocity; omega = next.Spin;
                pos += velocity * Dt;
                float d = Vector3.Dot(pos, n) - Core.GolfPhysics.Radius;
                if (d < 0) pos -= n * d;
            }
            if (Core.GolfPhysics.CanRest(new Core.BallState(pos, velocity, omega), onGround, n, surface, rp))
            {
                rest = true;
                velocity = Vector3.Zero; omega = Vector3.Zero;
                break;
            }
        }
        float totalM = MathF.Max(Vector3.Dot(pos, shotDir), 0);
        if (!carryRecorded) carry = totalM;
        var ci = CultureInfo.InvariantCulture;
        return string.Format(ci,
            "{0}@{1}: carry_yd={2:F4} total_yd={3:F4} apex_ft={4:F4} hang_s={5:F4} landing_angle_deg={6:F3} landing_speed_mps={7:F3} lateral_yd={8:F4} rest={9} bucket={10} regime={11} matched={12}",
            name, surfaceType, carry * YardsPerMeter, totalM * YardsPerMeter, apex * FeetPerMeter, hang, landingAngle, landingSpeed,
            pos.Z * YardsPerMeter, rest, bucket, regimeKey, matched);
    }
}
