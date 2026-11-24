// SPDX-License-Identifier: MIT
// The target games' scoring rules: pure functions, no world.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Training/TrainingScoring.h"

namespace TrainingScoringTestDetail
{
    using namespace TrainingScoring;

    /** Plays a run of shots through the ladder from its start. */
    FLadderState Play(int32 Tries, std::initializer_list<bool> Shots)
    {
        FLadderState State;
        for (const bool bInside3 : Shots)
            State = Next(State, Tries, bInside3);
        return State;
    }
} // namespace TrainingScoringTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingScoringRingsTest, "UOpenGolf.Training.Scoring.Rings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingScoringRingsTest::RunTest(const FString&)
{
    using namespace TrainingScoringTestDetail;
    TestEqual(TEXT("3 ft is 91.44 cm"), InnerRadiusCm, 91.44);
    TestEqual(TEXT("6 ft is 182.88 cm"), OuterRadiusCm, 182.88);
    TestTrue(TEXT("Exactly 3 ft is inside"), Classify(91.44, false, false) == ERing::Inside3Ft);
    TestTrue(TEXT("Just past 3 ft"), Classify(91.45, false, false) == ERing::Inside6Ft);
    TestTrue(TEXT("Exactly 6 ft is inside"), Classify(182.88, false, false) == ERing::Inside6Ft);
    TestTrue(TEXT("Just past 6 ft misses"), Classify(182.89, false, false) == ERing::Miss);
    TestTrue(TEXT("A holed ball is inside 3 ft"), Classify(500, true, false) == ERing::Inside3Ft);
    TestTrue(TEXT("A safety stop misses"), Classify(10, false, true) == ERing::Miss);
    TestTrue(TEXT("3 tries"), IsValidTries(3));
    TestTrue(TEXT("5 tries"), IsValidTries(5));
    for (const int32 Tries : {0, 1, 4, 6, -3})
        TestFalse(*FString::Printf(TEXT("%d tries is not offered"), Tries), IsValidTries(Tries));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingScoringSetTest, "UOpenGolf.Training.Scoring.TargetSet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingScoringSetTest::RunTest(const FString&)
{
    using namespace TrainingScoringTestDetail;
    const FTargetSet Start{3};
    const FTargetSet One = Record(Start, ERing::Inside3Ft);
    TestEqual(TEXT("Recording leaves the input unchanged"), Start.Shots, 0);
    TestEqual(TEXT("One shot"), One.Shots, 1);
    TestEqual(TEXT("Inside 3 ft"), One.Inside3, 1);
    TestEqual(TEXT("A 3 ft ball is also inside 6 ft"), One.Inside6, 1);
    const FTargetSet Two = Record(One, ERing::Inside6Ft);
    TestEqual(TEXT("A 6 ft ball is not inside 3 ft"), Two.Inside3, 1);
    TestEqual(TEXT("Inside 6 ft counts both"), Two.Inside6, 2);
    TestFalse(TEXT("Two of three is not complete"), Two.IsComplete());
    const FTargetSet Three = Record(Two, ERing::Miss);
    TestEqual(TEXT("A miss counts as a shot"), Three.Shots, 3);
    TestEqual(TEXT("A miss scores nothing"), Three.Inside6, 2);
    TestTrue(TEXT("Three of three completes"), Three.IsComplete());
    const FTargetSet After = Record(Three, ERing::Inside3Ft);
    TestTrue(TEXT("A complete set takes no more shots"), After.Shots == 3 && After.Inside3 == 1);
    FTargetSet Five{5};
    for (int32 Shot = 0; Shot < 4; ++Shot)
        Five = Record(Five, ERing::Miss);
    TestFalse(TEXT("Four of five is not complete"), Five.IsComplete());
    TestTrue(TEXT("Five of five completes"), Record(Five, ERing::Miss).IsComplete());
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
