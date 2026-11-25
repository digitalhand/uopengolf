// SPDX-License-Identifier: MIT
// A course round's setup: the choices made before play, the round's random wind, their URL form, and how they are checked.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Play/OpenGolfRoundSetup.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfRoundSetupTest, "UOpenGolf.Course.Play.RoundSetup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace RoundSetupTestDetail
{
    TArray<int32> Range(int32 First, int32 Last)
    {
        TArray<int32> Holes;
        for (int32 Hole = First; Hole <= Last; ++Hole)
            Holes.Add(Hole);
        return Holes;
    }
} // namespace RoundSetupTestDetail

bool FOpenGolfRoundSetupTest::RunTest(const FString& Parameters)
{
    using namespace OpenGolfPlay;
    using RoundSetupTestDetail::Range;

    // Hole sets on an 18-hole course, a 9-hole course and a single hole (no layout).
    TestEqual(TEXT("All 18"), HolesFor(EOpenGolfHoleSet::All, 18), Range(1, 18));
    TestEqual(TEXT("Front 9"), HolesFor(EOpenGolfHoleSet::Front, 18), Range(1, 9));
    TestEqual(TEXT("Back 9"), HolesFor(EOpenGolfHoleSet::Back, 18), Range(10, 18));
    TestEqual(TEXT("A 12-hole course's back holes"), HolesFor(EOpenGolfHoleSet::Back, 12), Range(10, 12));
    TestTrue(TEXT("Nines only past 9 holes"), HasNines(18) && HasNines(12) && !HasNines(9) && !HasNines(0));
    TestEqual(TEXT("A 9-hole course has no back nine"), HolesFor(EOpenGolfHoleSet::Back, 9), Range(1, 9));
    TestTrue(TEXT("A single hole has no holes to pick"), HolesFor(EOpenGolfHoleSet::All, 0).IsEmpty());
    TestEqual(TEXT("Detect all"), DetectHoleSet(Range(1, 18), 18), EOpenGolfHoleSet::All);
    TestEqual(TEXT("Detect front"), DetectHoleSet(Range(1, 9), 18), EOpenGolfHoleSet::Front);
    TestEqual(TEXT("Detect back"), DetectHoleSet(Range(10, 18), 18), EOpenGolfHoleSet::Back);
    TestEqual(TEXT("Detect a hand-picked set"), DetectHoleSet({3, 7, 12}, 18), EOpenGolfHoleSet::Custom);
    TestEqual(TEXT("On 9 holes, all nine are all"), DetectHoleSet(Range(1, 9), 9), EOpenGolfHoleSet::All);

    // The URL form of a hole list: ascending runs, parsed back without duplicates or holes the course lacks.
    TestEqual(TEXT("Runs are written as ranges"), FormatHoles({1, 2, 3, 4, 5, 6, 7, 8, 9, 12}), FString(TEXT("1-9,12")));
    TestEqual(TEXT("Single holes stay single"), FormatHoles({3, 7, 12}), FString(TEXT("3,7,12")));
    TestEqual(TEXT("Ranges parse back"), ParseHoles(TEXT("1-9,12"), 18), TArray<int32>({1, 2, 3, 4, 5, 6, 7, 8, 9, 12}));
    TestEqual(TEXT("Parsing sorts, drops repeats and holes past the course"), ParseHoles(TEXT("12,3,3,40,0,-2,7"), 18), TArray<int32>({3, 7, 12}));
    TestEqual(TEXT("A reversed range still parses"), ParseHoles(TEXT("12-10"), 18), Range(10, 12));
    TestTrue(TEXT("Junk parses to nothing"), ParseHoles(TEXT("front,x-y"), 18).IsEmpty());

    // Wind: no choice on the page; each round rolls a whole speed from 0 to 13 mph and any whole direction, fixed in
    // the world for the whole round.
    FOpenGolfRoundSetup Setup;
    TestEqual(TEXT("A new setup has no wind"), Setup.WindMph(), 0.0f);
    {
        FRandomStream Stream(20261124);
        bool bInRange = true;
        bool bWhole = true;
        TSet<int32> Speeds;
        int32 MinDirection = 360;
        int32 MaxDirection = -1;
        for (int32 Roll = 0; Roll < 2000; ++Roll)
        {
            FOpenGolfRoundSetup Rolled;
            Rolled.RollWind(Stream);
            bInRange &= Rolled.WindSpeedMph >= 0 && Rolled.WindSpeedMph <= MaxRoundWindMph && Rolled.WindFromDeg >= 0.0f && Rolled.WindFromDeg < 360.0f;
            bWhole &= Rolled.WindFromDeg == FMath::RoundToFloat(Rolled.WindFromDeg);
            Speeds.Add(Rolled.WindSpeedMph);
            MinDirection = FMath::Min(MinDirection, FMath::RoundToInt(Rolled.WindFromDeg));
            MaxDirection = FMath::Max(MaxDirection, FMath::RoundToInt(Rolled.WindFromDeg));
        }
        TestEqual(TEXT("The random wind tops out at 13 mph"), MaxRoundWindMph, 13);
        TestTrue(TEXT("Every rolled wind is 0-13 mph from 0-359°"), bInRange);
        TestTrue(TEXT("Rolled directions are whole degrees"), bWhole);
        TestEqual(TEXT("Every speed from calm to 13 mph comes up"), Speeds.Num(), MaxRoundWindMph + 1);
        TestTrue(TEXT("Directions reach all the way round"), MinDirection <= 5 && MaxDirection >= 354);
    }
    {
        FRandomStream First(7);
        FRandomStream Second(7);
        FOpenGolfRoundSetup A;
        FOpenGolfRoundSetup B;
        A.RollWind(First);
        B.RollWind(Second);
        TestTrue(TEXT("The same seed rolls the same wind"), A.WindSpeedMph == B.WindSpeedMph && A.WindFromDeg == B.WindFromDeg);
    }

    // Validation: every value is one the setup page can show.
    FOpenGolfRoundSetup Wild;
    Wild.TeeIndex = 7;
    Wild.GreenStimpFeet = 40;
    Wild.WindFromDeg = -100.0f;
    Wild.GimmieFeet = 5;
    Wild.Holes = {18, 2, 2, 30};
    Wild.WindSpeedMph = 40;
    Wild.Validate(18, 3);
    TestEqual(TEXT("Tee set within the course's"), Wild.TeeIndex, 2);
    TestEqual(TEXT("Stimp clamped to the fastest green"), Wild.GreenStimpFeet, MaxRoundStimpFeet);
    TestEqual(TEXT("Direction wrapped, to a whole degree"), Wild.WindFromDeg, 260.0f);
    TestEqual(TEXT("Gimmie radius is 3 or 6 ft"), Wild.GimmieFeet, LongGimmieFeet);
    TestEqual(TEXT("Holes sorted, unique and on the course"), Wild.Holes, TArray<int32>({2, 18}));
    TestEqual(TEXT("Wind no stronger than a rolled one"), Wild.WindSpeedMph, MaxRoundWindMph);
    FOpenGolfRoundSetup Turned;
    Turned.WindFromDeg = 359.6f;
    Turned.Validate(0, 0);
    TestEqual(TEXT("A direction rounding to 360 is 0"), Turned.WindFromDeg, 0.0f);
    FOpenGolfRoundSetup NoHoles;
    NoHoles.Validate(18, 1);
    TestEqual(TEXT("No holes picked plays them all"), NoHoles.Holes, Range(1, 18));
    FOpenGolfRoundSetup SingleHole;
    SingleHole.Holes = {1, 2};
    SingleHole.Validate(0, 0);
    TestTrue(TEXT("A single-hole course keeps no hole list"), SingleHole.Holes.IsEmpty() && SingleHole.TeeIndex == 0);
    TestEqual(TEXT("Gimmie off concedes nothing"), FOpenGolfRoundSetup().AppliedGimmieFeet(), 0);

    // The round travels as URL options and comes back the same.
    FOpenGolfRoundSetup Picked;
    Picked.TeeIndex = 1;
    Picked.GreenStimpFeet = 12;
    Picked.WindSpeedMph = 12;
    Picked.WindFromDeg = 217.0f;
    Picked.bGimmie = true;
    Picked.GimmieFeet = LongGimmieFeet;
    Picked.Holes = Range(10, 18);
    const FString Options = Picked.ToOptions();
    TestTrue(TEXT("Options carry the holes"), Options.Contains(TEXT("?Holes=10-18")));
    TestTrue(TEXT("Options carry the wind"), Options.Contains(TEXT("?WindMph=12?WindFrom=217")));
    TestFalse(TEXT("Rounds have no time of day"), Options.Contains(TEXT("Time=")));
    const FOpenGolfRoundSetup Back = FOpenGolfRoundSetup::FromOptions(Options, FOpenGolfRoundSetup(), 18, 2);
    TestTrue(TEXT("Options round-trip"), Back == Picked);
    TestEqual(TEXT("The applied Gimmie"), Back.AppliedGimmieFeet(), LongGimmieFeet);

    FOpenGolfRoundSetup Defaults;
    Defaults.GreenStimpFeet = 11;
    Defaults.WindSpeedMph = 3;
    Defaults.WindFromDeg = 90.0f;
    const FOpenGolfRoundSetup FromBare = FOpenGolfRoundSetup::FromOptions(TEXT("?Course=DA_Savanna?TeeIndex=0"), Defaults, 0, 0);
    TestTrue(TEXT("Missing options keep the defaults"), FromBare.GreenStimpFeet == 11 && FromBare.WindSpeedMph == 3 &&
        FromBare.WindFromDeg == 90.0f && FromBare.Holes.IsEmpty());
    const FOpenGolfRoundSetup Garbled = FOpenGolfRoundSetup::FromOptions(TEXT("?Stimp=fast?WindMph=gale?Time=Night?Gimmie=9"), Defaults, 18, 1);
    TestTrue(TEXT("Unknown values keep the defaults or the nearest choice; an old time of day is ignored"),
        Garbled.GreenStimpFeet == 11 && Garbled.WindSpeedMph == 3 && Garbled.bGimmie && Garbled.GimmieFeet == LongGimmieFeet);
    TestTrue(TEXT("Gimmie=0 turns it off"), !FOpenGolfRoundSetup::FromOptions(TEXT("?Gimmie=0"), Picked, 18, 2).bGimmie);

    // Labels and the wind turned onto the course.
    TestEqual(TEXT("Green speed label"), GreenSpeedLabel(11), FString(TEXT("11 ft · Medium")));
    TestEqual(TEXT("Slow green"), GreenSpeedLabel(8), FString(TEXT("8 ft · Slow")));
    TestEqual(TEXT("Fast green"), GreenSpeedLabel(13), FString(TEXT("13 ft · Fast")));
    TestEqual(TEXT("Tour green"), GreenSpeedLabel(15), FString(TEXT("15 ft · Tour")));
    // The world wind read against a tee shot, for the page and the in-round Settings row.
    TestEqual(TEXT("A wind from +X on a hole played along +X is a headwind"), RoundWindRelativeFromDeg(0.0f, 0.0f), 0.0f);
    TestEqual(TEXT("A wind from +Y on a hole played toward +Y is a headwind"), RoundWindRelativeFromDeg(90.0f, 90.0f), 0.0f);
    TestEqual(TEXT("A wind from +X on a hole played toward -Y comes from the right"), RoundWindRelativeFromDeg(0.0f, -90.0f), 90.0f);
    TestEqual(TEXT("The relative direction wraps"), RoundWindRelativeFromDeg(90.0f, 180.0f), 270.0f);
    return true;
}

#endif
