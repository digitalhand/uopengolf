// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenFairway/ShotSimulator.h"
#include "OpenFairway/WindProfile.h"
#include "OpenGolfBeaufort.h"
#include "OpenGolfFlagCloth.h"
#include "OpenGolfFlagstickComponent.h"

namespace FlagBeaufortTestDetail
{
    const FVector PoleTop(0.0, 0.0, UOpenGolfFlagstickComponent::PoleHeightCm);
    constexpr float Frame = 1.0f / 60.0f;
    constexpr float Seconds = 20.0f;
    /** Readings average over the last ten seconds, after the flag has turned and filled. */
    constexpr float MeasureSeconds = 10.0f;
    /** Flapping flags keep a near-constant Strouhal number f L / U, reported between 0.1 and 1. */
    constexpr float MinStrouhal = 0.1f;
    constexpr float MaxStrouhal = 1.0f;
    constexpr float MaxStrouhalSpread = 2.0f;
    /** Light air, as flapping starts, stretches the cloth most: about 6% (2 mm on a cell) at 3-5 mph. */
    constexpr float StretchLimit = 0.08f;
    /** The flap reading samples every cloth substep, so a slow frame rate must not change it by more than this. */
    constexpr float FrameRateFlapTolerance = 0.1f;

    struct FSweepPoint
    {
        float Mph10 = 0.0f;
        float FlagMps = 0.0f;
        float DroopDeg = 0.0f;
        float FlapHz = 0.0f;
        float MaxStretch = 0.0f;
        bool bFinite = true;
    };

    /** The flag in the range's path: the authored 10 m wind, reduced to the flag's height by the log profile. */
    FSweepPoint Fly(float Mph10, float FrameSeconds = Frame)
    {
        FSweepPoint Point;
        Point.Mph10 = Mph10;
        constexpr float FlagHeightM = UOpenGolfFlagstickComponent::FlagCentreHeightCm / 100.0f;
        Point.FlagMps = Mph10 * OpenFairway::MpsPerMph
            * OpenFairway::WindProfile::Factor(FlagHeightM, OpenFairway::WindProfile::StandardReferenceHeightM, OpenFairway::WindProfile::OpenGrassRoughnessM);

        FOpenGolfFlagCloth Cloth;
        FOpenGolfFlagCloth::FSettings Settings;
        Settings.LengthCm = UOpenGolfFlagstickComponent::FlagLengthCm;
        Settings.HeightCm = UOpenGolfFlagstickComponent::FlagHeightCm;
        Settings.PoleRadiusCm = UOpenGolfFlagstickComponent::PoleDiameterCm * 0.5f;
        Cloth.Reset(Settings, PoleTop, 90.0f);
        Cloth.Settle(UOpenGolfFlagstickComponent::SettleSeconds, PoleTop);
        // From +X, across the flag's starting heading, so it has to turn.
        Cloth.SetWind(FVector(-Point.FlagMps, 0.0, 0.0), 1.225f);

        FOpenGolfFlagMotion Motion;
        double DroopSum = 0.0, FlapSum = 0.0;
        int32 Samples = 0;
        for (float Time = 0.0f; Time < Seconds; Time += FrameSeconds)
        {
            Cloth.Advance(FrameSeconds, PoleTop, [&Motion](const FOpenGolfFlagCloth& Advanced, float Substep) { Motion.Sample(Advanced, Substep); });
            if (Time > 2.0f)
                Point.MaxStretch = FMath::Max(Point.MaxStretch, Cloth.GetMaxStretch());
            if (Time > Seconds - MeasureSeconds)
            {
                DroopSum += Cloth.GetDroopDeg();
                FlapSum += Motion.FlapHz;
                ++Samples;
            }
        }
        Point.DroopDeg = static_cast<float>(DroopSum / FMath::Max(Samples, 1));
        Point.FlapHz = static_cast<float>(FlapSum / FMath::Max(Samples, 1));
        for (const FVector& Position : Cloth.GetPositions())
            Point.bFinite &= !Position.ContainsNaN();
        return Point;
    }

    float Strouhal(const FSweepPoint& Point)
    {
        return Point.FlapHz * UOpenGolfFlagstickComponent::FlagLengthCm / 100.0f / FMath::Max(Point.FlagMps, UE_KINDA_SMALL_NUMBER);
    }
} // namespace FlagBeaufortTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfFlagBeaufortTest, "UOpenGolf.Course.FlagBeaufort",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenGolfFlagBeaufortTest::RunTest(const FString& Parameters)
{
    using namespace FlagBeaufortTestDetail;
    TArray<FSweepPoint> Sweep;
    // Every force the wind controls reach (up to 40 mph), with both sides of the stirring-to-extended edge.
    for (const float Mph10 : {0.0f, 2.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 10.0f, 12.0f, 15.0f, 20.0f, 25.0f, 30.0f, 35.0f, 40.0f})
        Sweep.Add(Fly(Mph10));

    FString Csv = TEXT("mph_10m,force,flag_mps,droop_deg,band_min_deg,band_max_deg,flap_hz,strouhal,max_stretch\n");
    float MinSt = TNumericLimits<float>::Max(), MaxSt = 0.0f;
    for (int32 Index = 0; Index < Sweep.Num(); ++Index)
    {
        const FSweepPoint& Point = Sweep[Index];
        const OpenGolfBeaufort::FForce& Force = OpenGolfBeaufort::ForceFor(Point.Mph10 * OpenFairway::MpsPerMph);
        const float St = Strouhal(Point);
        Csv += FString::Printf(TEXT("%.0f,%d,%.2f,%.1f,%.0f,%.0f,%.2f,%.2f,%.4f\n"), Point.Mph10, Force.Number, Point.FlagMps, Point.DroopDeg,
            Force.FlagDroopMinDeg, Force.FlagDroopMaxDeg, Point.FlapHz, St, Point.MaxStretch);

        TestTrue(FString::Printf(TEXT("%.0f mph stays finite"), Point.Mph10), Point.bFinite);
        TestTrue(FString::Printf(TEXT("%.0f mph (force %d, %s): flag %s, %.1f deg in %.0f-%.0f"), Point.Mph10, Force.Number, Force.Name,
                     Force.Flag, Point.DroopDeg, Force.FlagDroopMinDeg, Force.FlagDroopMaxDeg),
            Point.DroopDeg >= Force.FlagDroopMinDeg && Point.DroopDeg <= Force.FlagDroopMaxDeg);
        TestTrue(FString::Printf(TEXT("%.0f mph cloth does not stretch (%.1f%%)"), Point.Mph10, Point.MaxStretch * 100.0f), Point.MaxStretch < StretchLimit);
        if (Index > 0 && Sweep[Index - 1].Mph10 >= 5.0f)
        {
            // Small allowance for the gusts: stronger wind never lets the flag sag noticeably more.
            TestTrue(FString::Printf(TEXT("%.0f mph flies at least as high as %.0f mph (%.1f vs %.1f deg)"), Point.Mph10, Sweep[Index - 1].Mph10,
                         Point.DroopDeg, Sweep[Index - 1].DroopDeg),
                Point.DroopDeg <= Sweep[Index - 1].DroopDeg + 2.0f);
        }
        if (Force.Number >= 3)
        {
            TestTrue(FString::Printf(TEXT("%.0f mph flaps at Strouhal %.2f (%.2f Hz)"), Point.Mph10, St, Point.FlapHz), St >= MinStrouhal && St <= MaxStrouhal);
            MinSt = FMath::Min(MinSt, St);
            MaxSt = FMath::Max(MaxSt, St);
        }
    }
    // A near-constant Strouhal number means the flap rate rises in proportion to the wind.
    TestTrue(FString::Printf(TEXT("Flap rate follows the wind (Strouhal %.2f-%.2f)"), MinSt, MaxSt), MaxSt <= MinSt * MaxStrouhalSpread);

    // A gale beyond the controls, which uopengolf.WindMph can still set: the fly end flaps above the hoist,
    // which still reads as straight out. The cloth stretches past StretchLimit there (about 11%), so only the
    // band is held.
    const FSweepPoint Gale = Fly(60.0f);
    const OpenGolfBeaufort::FForce& GaleForce = OpenGolfBeaufort::ForceFor(Gale.Mph10 * OpenFairway::MpsPerMph);
    TestTrue(FString::Printf(TEXT("60 mph gale (force %d) stays finite, %.1f deg in %.0f-%.0f"), GaleForce.Number, Gale.DroopDeg,
                 GaleForce.FlagDroopMinDeg, GaleForce.FlagDroopMaxDeg),
        Gale.bFinite && Gale.DroopDeg >= GaleForce.FlagDroopMinDeg && Gale.DroopDeg <= GaleForce.FlagDroopMaxDeg);
    AddInfo(FString::Printf(TEXT("60 mph gale: cloth stretch %.1f%%"), Gale.MaxStretch * 100.0f));

    // A fast flap must read the same at 30 fps as at 60: sampled once a frame, its crossings alias away.
    for (const FSweepPoint& Point : Sweep)
    {
        if (Point.Mph10 < 30.0f || Point.Mph10 > 40.0f)
            continue;
        const float SlowFlapHz = Fly(Point.Mph10, 1.0f / 30.0f).FlapHz;
        TestTrue(FString::Printf(TEXT("%.0f mph flaps at %.2f Hz at 30 fps, %.2f Hz at 60"), Point.Mph10, SlowFlapHz, Point.FlapHz),
            FMath::Abs(SlowFlapHz - Point.FlapHz) <= FrameRateFlapTolerance * Point.FlapHz);
    }

    const FString Report = FPaths::ProjectSavedDir() / TEXT("Automation/FlagWind/sweep.csv");
    TestTrue(TEXT("Sweep report is written"), FFileHelper::SaveStringToFile(Csv, *Report));
    AddInfo(FString::Printf(TEXT("Flag sweep: %s"), *FPaths::ConvertRelativePathToFull(Report)));
    return true;
}

#endif
