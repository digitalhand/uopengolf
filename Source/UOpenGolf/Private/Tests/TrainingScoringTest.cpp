// SPDX-License-Identifier: MIT
// The target games' scoring rules: pure functions, no world.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Training/TrainingScoring.h"
#include "Training/TargetRings.h"
#include "OpenFairway/GolfPhysics.h"

namespace TrainingScoringTestDetail
{
    using namespace TrainingScoring;

    /** Plays a run of shots through the ladder from its start. */
    FLadderState Play(int32 Tries, std::initializer_list<bool> Shots)
    {
        FLadderState State;
        for (const bool bHit : Shots)
            State = Next(State, Tries, bHit);
        return State;
    }
} // namespace TrainingScoringTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingScoringRingsTest, "UOpenGolf.Training.Scoring.Rings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingScoringRingsTest::RunTest(const FString&)
{
    using namespace TrainingScoringTestDetail;
    for (const int32 Feet : {3, 6, 10})
    {
        TestTrue(TEXT("Offered radius is valid"), IsValidRadius(Feet));
        for (const int32 Yards : {35, 150})
        {
            const double Radius = Feet * CmPerFoot;
            const double Width = UTargetRings::EdgeWidthCm(Yards * CmPerYard);
            const double Touch = Radius + Width + OpenFairway::GolfPhysics::Radius * 100.;
            for (const double Distance : {50., Radius + Width / 2, Touch})
                TestTrue(TEXT("Inside, on the stripe, and touching its outer edge all win"), IsHit(Distance, false, false, Radius, Width));
            TestFalse(TEXT("A 1 mm gap misses"), IsHit(Touch + .1, false, false, Radius, Width));
            TestFalse(TEXT("A safety stop on the stripe misses"), IsHit(Touch, false, true, Radius, Width));
            TestTrue(TEXT("A holed shot wins"), IsHit(500, true, false, Radius, Width));
        }
    }
    TestFalse(TEXT("A radius outside the offered choices is invalid"), IsValidRadius(5));
    TestTrue(TEXT("3 tries"), IsValidTries(3));
    TestTrue(TEXT("5 tries"), IsValidTries(5));
    for (const int32 Tries : {0, 1, 4, 6, -3})
        TestFalse(*FString::Printf(TEXT("%d tries is not offered"), Tries), IsValidTries(Tries));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingScoringTallyTest, "UOpenGolf.Training.Scoring.HitTally",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingScoringTallyTest::RunTest(const FString&)
{
    using namespace TrainingScoringTestDetail;
    const FHitTally Start;
    const FHitTally One = Record(Start, true);
    TestEqual(TEXT("Recording leaves the input unchanged"), Start.Shots, 0);
    TestEqual(TEXT("One shot"), One.Shots, 1);
    TestEqual(TEXT("One hit"), One.Hits, 1);
    const FHitTally Two = Record(One, false);
    TestEqual(TEXT("A miss counts as a shot"), Two.Shots, 2);
    TestEqual(TEXT("A miss leaves the hit count alone"), Two.Hits, 1);
    // Distance Practice has no shot limit: the tally keeps counting.
    FHitTally Long = Two;
    for (int32 Shot = 0; Shot < 50; ++Shot)
        Long = Record(Long, Shot % 2 == 0);
    TestTrue(TEXT("Fifty more shots are all counted"), Long.Shots == 52 && Long.Hits == 26);
    TestEqual(TEXT("The default tries are the first choice"), DefaultTries, TryChoices[0]);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingScoringLadderTest, "UOpenGolf.Training.Scoring.Ladder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingScoringLadderTest::RunTest(const FString&)
{
    using namespace TrainingScoringTestDetail;
    const TConstArrayView<int32> Yards = LadderYards();
    if (!TestEqual(TEXT("Fifteen targets"), Yards.Num(), 15))
        return false;
    TestEqual(TEXT("Starts at 15 yd"), Yards[0], 15);
    for (int32 Stage = 1; Stage < 14; ++Stage)
        TestEqual(TEXT("10 yd steps"), Yards[Stage] - Yards[Stage - 1], 10);
    TestEqual(TEXT("145 yd before the last"), Yards[13], 145);
    TestEqual(TEXT("Ends at 150 yd"), Yards[14], 150);

    const FLadderState Start;
    TestTrue(TEXT("The first target is 15 yd"), Start.Stage == 0 && Start.TriesUsed == 0 && Start.BestStage == INDEX_NONE);
    const FLadderState Hit = Next(Start, 3, true);
    TestEqual(TEXT("Next leaves the input unchanged"), Start.TotalShots, 0);
    TestTrue(TEXT("A 3 ft ball advances"), Hit.Stage == 1 && Hit.LastStep == ELadderStep::Advanced && Hit.BestStage == 0);
    TestEqual(TEXT("The new target has all its tries"), Hit.TriesUsed, 0);

    const FLadderState Miss = Next(Hit, 3, false);
    TestTrue(TEXT("A miss with tries left stays"), Miss.Stage == 1 && Miss.TriesUsed == 1 && Miss.LastStep == ELadderStep::Missed);
    const FLadderState Dropped = Play(3, {true, false, false, false});
    TestTrue(TEXT("Missing every try drops back a target"), Dropped.Stage == 0 && Dropped.LastStep == ELadderStep::DroppedBack);
    TestEqual(TEXT("Dropping back resets the tries"), Dropped.TriesUsed, 0);
    TestEqual(TEXT("Dropping back keeps the best target"), Dropped.BestStage, 0);
    TestEqual(TEXT("Every shot is counted"), Dropped.TotalShots, 4);

    const FLadderState Floor = Play(3, {false, false, false});
    TestTrue(TEXT("The first target cannot drop back"), Floor.Stage == 0 && Floor.TriesUsed == 0 && Floor.LastStep == ELadderStep::Repeated);
    const FLadderState FiveTries = Play(5, {true, false, false, false, false});
    TestTrue(TEXT("Five tries allow a fourth miss"), FiveTries.Stage == 1 && FiveTries.TriesUsed == 4);
    TestTrue(TEXT("The fifth miss drops back"), Next(FiveTries, 5, false).Stage == 0);
    TestTrue(TEXT("A hit on the last try still advances"), Play(3, {false, false, true}).Stage == 1);

    FLadderState Climb;
    for (int32 Stage = 0; Stage < 14; ++Stage)
        Climb = Next(Climb, 3, true);
    TestTrue(TEXT("Fourteen hits reach 150 yd"), Climb.Stage == 14 && !Climb.IsComplete());
    const FLadderState Done = Next(Climb, 3, true);
    TestTrue(TEXT("A hit at 150 yd completes the ladder"), Done.IsComplete() && Done.Stage == 14 && Done.BestStage == 14);
    TestEqual(TEXT("Fifteen shots"), Done.TotalShots, 15);
    const FLadderState After = Next(Done, 3, false);
    TestTrue(TEXT("A complete ladder takes no more shots"), After.TotalShots == 15 && After.IsComplete());
    return true;
}
#endif
