// SPDX-License-Identifier: MIT
// Reference harness: mirrors ogolf-physics PhysicsAdapter.SimulateShotInternal (planar ground)
// on top of the real Godot-free core sources, to produce golden values for the C++ port.
// Usage: dotnet run --project <this folder> [-- <ogolf-physics dir> [greens | shot[@Surface[:option...]] ...]]
// A shot is a JSON file from assets/data or data/, or putt=<mph>[,<vla>] for a no-spin putt.
// Options: stimp=<ft> (profile GreenStimpFeet), slope=<deg> (ground tilted downhill along
// the target line; negative is uphill), neutral (no landing-bucket bite, as the reference's
// LandingBucketRegressionTests use). "greens" runs the GreenReferenceTest.cpp cases.
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

    // The cases in Private/Tests/GreenReferenceTest.cpp.
    static readonly string[] GreenShots =
    {
        "putt=4.1@Green", "putt=5.5@Green", "putt=7.8@Green", "putt=11@Green", "putt=5.5,3@Green",
        "putt=3.9@Green:stimp=11", "putt=6@Green:stimp=11", "putt=9.5@Green:stimp=11", "putt=12.5@Green:stimp=11",
        "putt=6@Green:stimp=8", "putt=6@Green:stimp=13",
        "flop_test_shot.json@Green", "wedge_test_shot.json@Green:neutral", "approach_mid_iron_test_shot.json@Green",
        "approach_mid_iron_test_shot.json@Green:neutral", "wedge_test_shot.json@Green:slope=2", "wedge_test_shot.json@Green:slope=-3",
        "putt=6@Green:slope=2", "putt=6@Green:slope=-2", "wood_low_test_shot.json@Fairway:slope=2", "wood_low_test_shot.json@Rough:slope=-2",
    };

    static void Main(string[] args)
    {
        string harnessDir = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "../../../"));
        string ogolfDir = args.Length > 0 ? args[0] : Path.Combine(harnessDir, "../../../../../ogolf-physics");
        string[] dataDirs = { Path.Combine(ogolfDir, "assets/data"), Path.Combine(harnessDir, "data") };
        var results = new List<string>();
        string[] shots = args.Length > 1 ? (args[1] == "greens" ? GreenShots : args[1..]) : DefaultShots;
        foreach (string file in shots)
        {
            string surfaceName = "Fairway";
            string name = file;
            int at = file.IndexOf('@');
            if (at >= 0) { surfaceName = file[(at + 1)..]; name = file[..at]; }
            string[] options = surfaceName.Split(':');
            var surface = Enum.Parse<PhysicsEnums.SurfaceType>(options[0]);
            var profile = new BallPhysicsProfile();
            float slopeDeg = 0f;
            foreach (string option in options[1..])
            {
                string[] kv = option.Split('=');
                if (kv[0] == "stimp") profile.GreenStimpFeet = float.Parse(kv[1], CultureInfo.InvariantCulture);
                else if (kv[0] == "slope") slopeDeg = float.Parse(kv[1], CultureInfo.InvariantCulture);
                else if (kv[0] == "neutral")
                    foreach (var key in new List<LandingBucketKind>(profile.LandingResponses.Keys))
                        profile.LandingResponses[key] = profile.LandingResponses[key] with { SpinBiteScale = 0f };
                else throw new ArgumentException($"Unknown option {option}");
            }

            string json;
            if (name.StartsWith("putt="))
            {
                string[] p = name[5..].Split(',');
                json = "{\"Speed\": " + p[0] + ", \"VLA\": " + (p.Length > 1 ? p[1] : "0") + ", \"HLA\": 0, \"TotalSpin\": 0, \"SpinAxis\": 0}";
            }
            else
            {
                string path = Array.Find(dataDirs, dir => File.Exists(Path.Combine(dir, name))) is { } found
                    ? Path.Combine(found, name)
                    : throw new FileNotFoundException($"{name} not found in {string.Join(", ", dataDirs)}");
                json = File.ReadAllText(path);
            }
            using var doc = JsonDocument.Parse(json);
            JsonElement ball = doc.RootElement.TryGetProperty("BallData", out var b) ? b : doc.RootElement;
            float slope = slopeDeg * DegToRadF;
            results.Add(Simulate(ball, surface, profile, new Vector3(MathF.Sin(slope), MathF.Cos(slope), 0f), at >= 0 ? file : $"{file}@{surface}"));
        }
        Console.WriteLine(string.Join("\n", results));
    }

    static bool Has(JsonElement e, string k) => e.TryGetProperty(k, out _);
    static float Get(JsonElement e, string k) => e.TryGetProperty(k, out var v) ? (float)v.GetDouble() : 0f;

    static string Simulate(JsonElement ball, PhysicsEnums.SurfaceType surfaceType, BallPhysicsProfile profile, Vector3 groundNormal, string label)
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

        // PhysicsAdapter: contactNormal = floorNormal.Normalized(), else up.
        Vector3 n = groundNormal.LengthSquared() > 0.000001f ? Vector3.Normalize(groundNormal) : up;
        Vector3 pos = n * Core.GolfPhysics.Radius;
        bool flight = true, onGround = false, carryRecorded = false, rest = false;
        float carry = 0, hang = 0, apex = pos.Y, landingSpeed = 0, landingAngle = 0, tangentIn = 0, tangentOut = 0;
        bool spinback = false;
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
                        Vector3 postTan = velocity - n * Vector3.Dot(velocity, n);
                        tangentIn = preTan.Length();
                        tangentOut = postTan.Length();
                        if (tangentIn > 0.01f && tangentOut > 0.01f)
                        {
                            spinback = Vector3.Dot(Vector3.Normalize(preTan), Vector3.Normalize(postTan)) < -0.001f;
                            if (spinback) tangentOut = -tangentOut;
                        }
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
            "{0}: carry_yd={1:F4} total_yd={2:F4} apex_ft={3:F4} hang_s={4:F4} landing_angle_deg={5:F3} landing_speed_mps={6:F3} lateral_yd={7:F4} rest={8} bucket={9} regime={10} matched={11} tangent_out_mps={12:F4} spinback={13}",
            label, carry * YardsPerMeter, totalM * YardsPerMeter, apex * FeetPerMeter, hang, landingAngle, landingSpeed,
            pos.Z * YardsPerMeter, rest, bucket, regimeKey, matched, tangentOut, spinback);
    }
}
