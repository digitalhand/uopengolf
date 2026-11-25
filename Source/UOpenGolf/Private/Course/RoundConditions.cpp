// SPDX-License-Identifier: MIT
#include "Course/RoundConditions.h"
#include "GolfCourseDefinition.h"
#include "Math/RandomStream.h"
#include "OpenFairway/Units.h"
#include "OpenGolfEnvironment.h"
#include "OpenGolfSettings.h"
#include "Play/OpenGolfPlayRules.h"
#include "Play/OpenGolfWindSetting.h"

namespace RoundConditions
{
    using namespace OpenGolfPlay;

    int32 HoleCount(const UGolfCourseDefinition& Course)
    {
        return Course.Layout.Num();
    }

    int32 TeeCount(const UGolfCourseDefinition& Course)
    {
        return Course.Layout.IsEmpty() ? 0 : Course.Layout[0].Tees.Num();
    }

    FOpenGolfRoundSetup Defaults(const UGolfCourseDefinition& Course, const GolfSettings::FPreferences& Preferences)
    {
        FOpenGolfRoundSetup Setup;
        Setup.GreenStimpFeet = Preferences.Stimp;
        Setup.bGimmie = Preferences.GimmieFeet > 0;
        Setup.GimmieFeet = Setup.bGimmie ? Preferences.GimmieFeet : ShortGimmieFeet;
        Setup.Validate(HoleCount(Course), TeeCount(Course));
        return Setup;
    }

    void RollWind(FOpenGolfRoundSetup& Setup)
    {
        FRandomStream Stream;
        Stream.GenerateNewSeed();
        Setup.RollWind(Stream);
    }

    FName TeeSetName(const UGolfCourseDefinition& Course, int32 TeeIndex)
    {
        return Course.Layout.IsEmpty() || !Course.Layout[0].Tees.IsValidIndex(TeeIndex) ? NAME_None : Course.Layout[0].Tees[TeeIndex].SetName;
    }

    TArray<int32> HoleOrder(const FOpenGolfRoundSetup& Setup, const UGolfCourseDefinition& Course)
    {
        TArray<int32> Order;
        for (const int32 Hole : Setup.Holes)
            if (Course.Layout.IsValidIndex(Hole - 1))
                Order.Add(Hole - 1);
        return Order;
    }

    float TeeShotBearingDeg(const UGolfCourseDefinition& Course, int32 LayoutIndex, FName TeeSet)
    {
        if (!Course.Layout.IsValidIndex(LayoutIndex))
            return 0.0f;
        const FGolfCourseHole& Hole = Course.Layout[LayoutIndex];
        const FGolfCourseTee* Tee = Hole.FindTee(TeeSet);
        return Tee ? BearingDeg(Tee->PositionCm, Hole.RoutingCm.IsEmpty() ? Hole.CupCm : Hole.RoutingCm[0], 0.0f) : 0.0f;
    }

    int32 Par(const UGolfCourseDefinition& Course, const TArray<int32>& Holes)
    {
        if (Course.Layout.IsEmpty())
            return Course.Par;
        int32 Total = 0;
        for (const int32 Hole : Holes)
            if (Course.Layout.IsValidIndex(Hole - 1))
                Total += Course.Layout[Hole - 1].Par;
        return Total;
    }

    int32 Yards(const UGolfCourseDefinition& Course, const TArray<int32>& Holes, FName TeeSet)
    {
        int32 Total = 0;
        for (const int32 Hole : Holes)
            if (Course.Layout.IsValidIndex(Hole - 1))
                if (const FGolfCourseTee* Tee = Course.Layout[Hole - 1].FindTee(TeeSet))
                    Total += Tee->PublishedYards;
        return Total;
    }

    int32 CoursePar(const UGolfCourseDefinition& Course)
    {
        return Par(Course, HolesFor(EOpenGolfHoleSet::All, HoleCount(Course)));
    }

    float CourseLengthM(const UGolfCourseDefinition& Course, FName TeeSet)
    {
        return Course.Layout.IsEmpty() ? Course.LengthM : Course.TeeSetYards(TeeSet) * MetersPerYard;
    }

    FString ElevationText(const UGolfCourseDefinition& Course)
    {
        // Within half a metre of zero reads as sea level, not "0 ft" or "-0 m".
        return FMath::Abs(Course.ElevationM) < 0.5f ? FString(TEXT("Sea level")) : GolfSettings::RoundDistance(Course.ElevationM, true);
    }

    bool ApplyDaylight(const UWorld* World)
    {
        AOpenGolfEnvironment* Environment = AOpenGolfEnvironment::Find(World);
        if (!Environment)
            return false;
        Environment->SelectPreset(EOpenGolfDaylightPreset::Day);
        return true;
    }

    FString WindSpeedText(int32 Mph)
    {
        return FString::Printf(TEXT("%d %s"), FMath::RoundToInt(Mph * OpenFairway::MpsPerMph * GolfSettings::SpeedFactor()), *GolfSettings::SpeedUnit());
    }

    FString WindDirectionText(const FOpenGolfRoundSetup& Setup, const UGolfCourseDefinition& Course, int32 LayoutIndex)
    {
        const float Bearing = TeeShotBearingDeg(Course, LayoutIndex, TeeSetName(Course, Setup.TeeIndex));
        return WindDirectionLabel(RoundWindRelativeFromDeg(Setup.WindFromDeg, Bearing));
    }

    FString WindLabel(const FOpenGolfRoundSetup& Setup, const UGolfCourseDefinition& Course, int32 LayoutIndex)
    {
        if (Setup.WindSpeedMph <= 0)
            return TEXT("No wind");
        return WindSpeedText(Setup.WindSpeedMph) + TEXT(" · ") + WindDirectionText(Setup, Course, LayoutIndex);
    }
} // namespace RoundConditions
