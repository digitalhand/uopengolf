// SPDX-License-Identifier: MIT
// A Hunter Ranch round from its setup: the picked holes play in order and end the round, the green speed and Gimmie
// chosen before play are the ones the round plays, the wind is fixed in the world, and the round is played in daylight.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Course/RoundConditions.h"
#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GolfCourseDefinition.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenGolfEnvironment.h"
#include "Play/OpenGolfRoundSetup.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfRange.h"

namespace RoundSelectionTestDetail
{
    using namespace OpenGolfPlay;
    const TCHAR* const CoursePath = TEXT("/HunterRanch/Course/DA_HunterRanch.DA_HunterRanch");
    const TCHAR* const MapPath = TEXT("/HunterRanch/Course/Course");

    AUOpenGolfGameMode* PlayingMode()
    {
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        return Mode && Mode->GetBall() && Mode->GetRange() ? Mode : nullptr;
    }

    /** The tee set the round plays, by the setup's index on the first hole. */
    FName PlayedTeeSet(const AUOpenGolfGameMode& Mode)
    {
        return Mode.GetCourse()->Layout[0].Tees[Mode.GetTeeIndex()].SetName;
    }

    /** The world direction, in whole degrees, of a wind RelativeDeg from the tee shot of layout hole LayoutIndex (first tees). */
    float WorldFrom(int32 LayoutIndex, float RelativeDeg)
    {
        const auto* Course = LoadObject<UGolfCourseDefinition>(nullptr, CoursePath);
        const float Bearing = Course ? RoundConditions::TeeShotBearingDeg(*Course, LayoutIndex, Course->Layout[0].Tees[0].SetName) : 0.0f;
        return static_cast<float>(FMath::RoundToInt(FRotator::ClampAxis(Bearing + RelativeDeg)) % 360);
    }

    /** The round's daylight is the sky's Day preset, set after the sky's own reset. */
    void ExpectDaylight(FAutomationTestBase& Test, const AUOpenGolfGameMode& Mode)
    {
        const AOpenGolfEnvironment* Environment = AOpenGolfEnvironment::Find(Mode.GetWorld());
        if (Test.TestNotNull(TEXT("The course has a sky"), Environment))
            Test.TestEqual(TEXT("Course rounds are played in daylight"), Environment->GetSettings().Daylight, 0.5f);
    }

    /** The world wind along the tee shot of layout hole LayoutIndex: negative is into the player. */
    float WindAlongTeeShot(const AUOpenGolfGameMode& Mode, int32 LayoutIndex)
    {
        const float Bearing = FMath::DegreesToRadians(RoundConditions::TeeShotBearingDeg(*Mode.GetCourse(), LayoutIndex, PlayedTeeSet(Mode)));
        const FVector Wind = Mode.GetBall()->GetWindVelocityMps();
        return static_cast<float>(FVector::DotProduct(FVector(Wind.X, Wind.Y, 0), FVector(FMath::Cos(Bearing), FMath::Sin(Bearing), 0)));
    }

    /** Plays the setup's holes by holing each one, checking the order, the card and when the round ends. */
    class FPlayPickedHoles : public IAutomationLatentCommand
    {
    public:
        FPlayPickedHoles(FAutomationTestBase* InTest, FOpenGolfRoundSetup InSetup, TFunction<void(AUOpenGolfGameMode&)> InCheckConditions)
            : Test(InTest), Setup(MoveTemp(InSetup)), CheckConditions(MoveTemp(InCheckConditions))
        {
        }

        virtual bool Update() override
        {
            if (GetCurrentRunTime() > 200)
            {
                Test->AddError(FString::Printf(TEXT("Timed out after %d of %d holes."), Holed, Setup.Holes.Num()));
                return true;
            }
            AUOpenGolfGameMode* Mode = PlayingMode();
            if (!Mode)
                return false;
            if (!bStarted)
            {
                const auto* Course = LoadObject<UGolfCourseDefinition>(nullptr, CoursePath);
                if (!Test->TestNotNull(TEXT("Course loads"), Course))
                    return true;
                bStarted = true;
                Mode->SetPlayMode(EOpenGolfPlayMode::Hole, Course, &Setup);
                Test->TestTrue(TEXT("The round keeps its setup"), Mode->GetRoundSetup() == Setup);
                CheckConditions(*Mode);
            }
            const int32 Count = Setup.Holes.Num();
            if (Holed == Count)
            {
                if (Mode->GetCompletedHoles().Num() < Count)
                {
                    Test->TestFalse(TEXT("The round waits for the last hole's score"), Mode->IsRoundComplete());
                    return false;
                }
                Test->TestTrue(TEXT("The last picked hole ends the round"), Mode->IsRoundComplete());
                Test->TestEqual(TEXT("The card numbers the holes played"), Mode->GetCompletedHoleNumbers(), Setup.Holes);
                return true;
            }
            if (Mode->GetHoleNumber() != Setup.Holes[Holed])
                return false;
            Test->TestTrue(FString::Printf(TEXT("Hole %d is open before its hole out"), Setup.Holes[Holed]), !Mode->IsRoundComplete());
            Test->TestEqual(TEXT("Each hole plays the round's green speed"), Mode->GetBall()->GreenStimpFeet, static_cast<float>(Setup.GreenStimpFeet));
            Test->TestEqual(TEXT("Each hole plays the round's Gimmie"), Mode->GetGimmieRadiusFeet(), static_cast<float>(Setup.AppliedGimmieFeet()));
            FOpenFairwayShotResult Rest;
            Rest.ShotNumber = ++Holed;
            Rest.bHoled = true;
            Mode->GetBall()->OnBallAtRest.Broadcast(Rest);
            return false;
        }

    private:
        FAutomationTestBase* Test;
        FOpenGolfRoundSetup Setup;
        TFunction<void(AUOpenGolfGameMode&)> CheckConditions;
        bool bStarted = false;
        int32 Holed = 0;
    };

    void Enqueue(FAutomationTestBase* Test, const FOpenGolfRoundSetup& Setup, TFunction<void(AUOpenGolfGameMode&)> CheckConditions)
    {
        ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
        FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FPlayPickedHoles>(Test, Setup, MoveTemp(CheckConditions)));
        ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    }
} // namespace RoundSelectionTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundSelectionBackNineTest, "UOpenGolf.Course.HunterRanch.BackNine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRoundSelectionBackNineTest::RunTest(const FString&)
{
    using namespace RoundSelectionTestDetail;
    if (!FEditorFileUtils::LoadMap(MapPath, false, true))
        return false;
    FOpenGolfRoundSetup Setup;
    Setup.GreenStimpFeet = 12;
    Setup.WindSpeedMph = MaxRoundWindMph;
    Setup.WindFromDeg = WorldFrom(9, 90.0f);
    Setup.bGimmie = true;
    Setup.GimmieFeet = LongGimmieFeet;
    Setup.Holes = HolesFor(EOpenGolfHoleSet::Back, 18);
    Enqueue(this, Setup, [this, Setup](AUOpenGolfGameMode& Mode)
    {
        TestEqual(TEXT("The back nine starts at hole 10"), Mode.GetHoleNumber(), 10);
        const AUOpenGolfRange& Range = *Mode.GetRange();
        TestEqual(TEXT("The round's 13 mph"), Range.WindSpeedMph, static_cast<float>(MaxRoundWindMph), 0.01f);
        TestEqual(TEXT("The wind is fixed in the world"), Range.WindFromDeg, Setup.WindFromDeg, 0.01f);
        // Within the whole degree the setup keeps.
        TestTrue(TEXT("A crosswind on hole 10 neither helps nor hurts its tee shot"), FMath::Abs(WindAlongTeeShot(Mode, 9)) < 0.1f);
        ExpectDaylight(*this, Mode);
    });
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundSelectionCustomTest, "UOpenGolf.Course.HunterRanch.PickedHoles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRoundSelectionCustomTest::RunTest(const FString&)
{
    using namespace RoundSelectionTestDetail;
    if (!FEditorFileUtils::LoadMap(MapPath, false, true))
        return false;
    FOpenGolfRoundSetup Setup;
    Setup.GreenStimpFeet = 9;
    Setup.WindSpeedMph = 3;
    Setup.WindFromDeg = WorldFrom(2, 0.0f);
    Setup.bGimmie = false;
    Setup.Holes = {3, 7, 12};
    Enqueue(this, Setup, [this](AUOpenGolfGameMode& Mode)
    {
        TestEqual(TEXT("The first picked hole starts"), Mode.GetHoleNumber(), 3);
        TestTrue(TEXT("A wind from straight down hole 3 is a headwind on its tee shot"), WindAlongTeeShot(Mode, 2) < -1.0f);
        TestEqual(TEXT("Gimmie off concedes nothing"), Mode.GetGimmieRadiusFeet(), 0.0f);
        ExpectDaylight(*this, Mode);
    });
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundSelectionRolledWindTest, "UOpenGolf.Course.HunterRanch.RolledWind",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRoundSelectionRolledWindTest::RunTest(const FString&)
{
    using namespace RoundSelectionTestDetail;
    if (!FEditorFileUtils::LoadMap(MapPath, false, true))
        return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    // A round started without a wind (no setup, as a travel URL without one) rolls its own and keeps it.
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Started = FPlatformTime::Seconds()]
    {
        AUOpenGolfGameMode* Mode = PlayingMode();
        if (!Mode)
        {
            if (FPlatformTime::Seconds() - Started < 60.0)
                return false;
            AddError(TEXT("Play did not start."));
            return true;
        }
        const auto* Course = LoadObject<UGolfCourseDefinition>(nullptr, CoursePath);
        if (!TestNotNull(TEXT("Course loads"), Course))
            return true;
        Mode->SetPlayMode(EOpenGolfPlayMode::Hole, Course);
        const FOpenGolfRoundSetup& Setup = Mode->GetRoundSetup();
        TestTrue(TEXT("A rolled wind is 0-13 mph"), Setup.WindSpeedMph >= 0 && Setup.WindSpeedMph <= MaxRoundWindMph);
        TestTrue(TEXT("From a whole degree, 0-359"), Setup.WindFromDeg >= 0.0f && Setup.WindFromDeg < 360.0f && Setup.WindFromDeg == FMath::RoundToFloat(Setup.WindFromDeg));
        const AUOpenGolfRange& Range = *Mode->GetRange();
        TestEqual(TEXT("The range plays the rolled speed"), Range.WindSpeedMph, static_cast<float>(Setup.WindSpeedMph), 0.01f);
        TestEqual(TEXT("And the rolled direction, in the world"), Range.WindFromDeg, Setup.WindFromDeg, 0.01f);
        ExpectDaylight(*this, *Mode);
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
