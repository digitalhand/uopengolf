// SPDX-License-Identifier: MIT
// Playing the hole in the saved map: the mode switch, strokes, placing the ball, and the cup capturing a putt.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Hole/OpenGolfCupComponent.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenGolfTerrain.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"

namespace HolePlayTestDetail
{
    /** A putt in the OpenFairway sense: ball speed only, no launch angle or spin. */
    FOpenFairwayLaunch Putt(float Mph)
    {
        FOpenFairwayLaunch Launch;
        Launch.BallSpeedMph = Mph;
        Launch.bHasTotalSpin = true;
        return Launch;
    }
} // namespace HolePlayTestDetail

class FVerifyHolePlay : public IAutomationLatentCommand
{
public:
    explicit FVerifyHolePlay(FAutomationTestBase* InTest) : Test(InTest) {}

    virtual bool Update() override
    {
        using namespace HolePlayTestDetail;
        if (GetCurrentRunTime() > 120.0)
        {
            Test->AddError(FString::Printf(TEXT("Timed out at stage %d."), Stage));
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        auto* Controller = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
        AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
        if (!Mode || !Ball || !Controller || !Mode->GetRange() || !Mode->GetRange()->Terrain)
            return false;
        const FVector Cup = Mode->GetCupLocation();
        // A spot on the green Distance cm short of the cup along the target line, Offset cm to the side.
        auto GreenSpot = [&](double DistanceCm, double OffsetCm)
        {
            FVector Ground, Normal;
            Mode->GetRange()->Terrain->GroundAt(Cup - FVector(DistanceCm, OffsetCm, 0), Ground, Normal);
            return Ground;
        };
        const double Now = World->GetTimeSeconds();

        switch (Stage)
        {
        case 0:
        {
            FVector Ground, Normal;
            if (Test->TestTrue(TEXT("The cup has terrain beneath it"), Mode->GetRange()->Terrain->GroundAt(Cup, Ground, Normal)))
            {
                Test->TestTrue(TEXT("The physics cup stays at ground level"), Cup.Equals(Ground, 0.01));
                Test->TestTrue(TEXT("The visual cup follows the green's slope"), Mode->GetRange()->GetCup()->GetUpVector().Equals(Normal, 0.0001));
            }
        }
            Test->TestEqual(TEXT("Play starts on the range"), Mode->GetPlayMode(), EOpenGolfPlayMode::Range);
            Test->TestTrue(TEXT("The ball can drop into the flag's cup"), Ball->GetCupLocations().Num() == 1 && Ball->GetCupLocations()[0].Equals(Cup, 0.1));
            Test->TestFalse(TEXT("Range mode places no ball"), Mode->PlaceBallAt(GreenSpot(300, 0)));
            Controller->TogglePlayMode();
            Test->TestEqual(TEXT("G plays the hole"), Mode->GetPlayMode(), EOpenGolfPlayMode::Hole);
            Test->TestTrue(TEXT("The hole starts at the tee"), Mode->IsBallAtAddress() &&
                FVector::Dist2D(Ball->GetActorLocation(), Mode->GetRange()->GetTeeLocation()) < 50.0);
            Test->TestEqual(TEXT("A new hole has no strokes"), Mode->GetHoleRound().Strokes, 0);
            Test->TestFalse(TEXT("No ball on the cup"), Mode->PlaceBallAt(Cup + FVector(3, 0, 0)));
            // A firm putt straight over the cup lips out over its far side, slower.
            Test->TestTrue(TEXT("The ball is placed on the green"), Mode->PlaceBallAt(GreenSpot(100, 0)));
            Test->TestEqual(TEXT("Placing adds no stroke"), Mode->GetHoleRound().Strokes, 0);
            Test->TestTrue(TEXT("A placed ball aims at the cup"), FMath::Abs(Ball->AimYawDeg) < 2.0f);
            Test->TestEqual(TEXT("A placed ball reads straight at the cup"), Controller->GetAimOffsetDeg(), 0.0f, 0.01f);
            Test->TestTrue(TEXT("A placed hole is practice"), Mode->GetHoleRound().bPractice);
            Ball->Launch(Putt(9.0f));
            ++Stage;
            return false;
        case 1:
            if (Ball->IsMoving())
            {
                Path.Emplace(Now, Ball->GetActorLocation().X - Cup.X);
                return false;
            }
            Test->TestFalse(TEXT("A fast putt rolls over the cup"), Ball->IsHoled());
            {
                FOpenFairwayShotResult Result;
                Test->TestTrue(TEXT("The putt is reported over the cup"), Ball->GetShotResult(Result) && Result.bOverCup);
            }
            {
                // Speed along the line either side of the cup, from the ball's travel, so a hop off the lip is not speed.
                auto SpanMps = [this](double FromCm, double ToCm)
                {
                    const TPair<double, double>* First = Path.FindByPredicate([FromCm](const TPair<double, double>& Sample) { return Sample.Value >= FromCm; });
                    const TPair<double, double>* Last = nullptr;
                    for (const TPair<double, double>& Sample : Path)
                        if (Sample.Value <= ToCm)
                            Last = &Sample;
                    return First && Last && Last->Key > First->Key ? (Last->Value - First->Value) / (Last->Key - First->Key) / 100.0 : 0.0;
                };
                const double Before = SpanMps(-90.0, -10.0);
                const double After = SpanMps(15.0, 80.0);
                Test->AddInfo(FString::Printf(TEXT("Firm putt along its line: %.2f m/s before the cup, %.2f m/s past it."), Before, After));
                Test->TestTrue(TEXT("The lip takes speed off a firm putt"), Before > 2.0 && After > 0.0 && After < 0.85 * Before);
            }
            Test->TestEqual(TEXT("The missed putt is a stroke"), Mode->GetHoleRound().Strokes, 0);
            RestedAt = Now;
            RestLocation = Ball->GetActorLocation();
            ++Stage;
            return false;
        case 2:
            // Past the range's reset delay the ball still lies where it stopped.
            if (Now - RestedAt < AUOpenGolfGameMode::ResetDelayS + 0.5)
                return false;
            Test->TestTrue(TEXT("Hole mode plays the ball as it lies"), Ball->GetActorLocation().Equals(Mode->GetPracticeOrigin(), 1.0));
            Test->TestTrue(TEXT("The ball is ready for the next shot"), Mode->IsBallAtAddress());
            // A putt that misses the cup by more than its radius stays out.
            Mode->PlaceBallAt(GreenSpot(100, 0));
            Ball->AimYawDeg += 5.0f;
            Ball->Launch(Putt(3.0f));
            ++Stage;
            return false;
        case 3:
            if (Ball->IsMoving())
                return false;
            Test->TestFalse(TEXT("A putt off line misses"), Ball->IsHoled());
            Mode->PlaceBallAt(GreenSpot(100, 0));
            Ball->Launch(Putt(3.0f));
            ++Stage;
            return false;
        case 4:
            if (Ball->IsMoving())
            {
                bSawPinLift |= Mode->GetRange()->GetCup()->GetPinLiftCm() > 1.0f;
                return false;
            }
            Test->TestTrue(TEXT("A dying putt on line drops"), Ball->IsHoled());
            Test->TestTrue(TEXT("The pin lifted for the ball"), bSawPinLift);
            {
                FOpenFairwayShotResult Result;
                Test->TestTrue(TEXT("The result says holed"), Ball->GetShotResult(Result) && Result.bHoled);
                Test->TestFalse(TEXT("A holed putt is not over the cup"), Result.bOverCup);
            }
            Test->TestTrue(TEXT("The ball sits below the green in the cup"), Ball->GetActorLocation().Z < Cup.Z);
            Test->TestTrue(TEXT("The hole is finished"), Ball->IsHoled());
            Test->TestEqual(TEXT("A practice hole names its strokes"), Mode->GetHoleRound().ResultName(), FString());
            Test->TestFalse(TEXT("A holed ball is not at address"), Mode->IsBallAtAddress());
            HoledAt = Now;
            ++Stage;
            return false;
        case 5:
            if (Now - HoledAt < AUOpenGolfGameMode::HoleOutDelayS + 0.5)
                return false;
            Test->TestTrue(TEXT("After a hole-out the ball goes back to the tee"),
                Ball->GetActorLocation().Equals(Mode->GetPracticeOrigin(), 1.0) && !Ball->IsHoled());
            Test->TestEqual(TEXT("A new hole starts"), Mode->GetHoleRound().Strokes, 0);
            Test->TestTrue(TEXT("Practice persists"), Mode->GetHoleRound().bPractice);
            {
                // A practice shot from the test-shot menu keeps the aim the player turned to (A/D), not the cup's line.
                const float Turned = OpenGolfPlay::BearingDeg(Ball->GetActorLocation(), Cup) + 20.0f;
                Test->TestTrue(TEXT("Aim turns at the practice spot"), Controller->SetAimYaw(Turned));
                Test->TestEqual(TEXT("The aim reads off the line to the cup"), Controller->GetAimOffsetDeg(), 20.0f, 0.01f);
                Controller->InjectReplayShot();
                Test->TestTrue(TEXT("The practice shot is hit"), Ball->IsMoving());
                Test->TestEqual(TEXT("The practice shot keeps the turned aim"), Ball->AimYawDeg, Turned, 0.01f);
                // The mode only switches with the ball still.
                Ball->PlaceAt(Ball->GetActorLocation());
            }
            Controller->TogglePlayMode();
            Test->TestEqual(TEXT("G goes back to the range"), Mode->GetPlayMode(), EOpenGolfPlayMode::Range);
            return true;
        default:
            return true;
        }
    }

private:
    FAutomationTestBase* Test;
    int32 Stage = 0;
    double RestedAt = 0.0;
    double HoledAt = 0.0;
    FVector RestLocation = FVector::ZeroVector;
    bool bSawPinLift = false;
    // The firm putt's travel along its line from the cup (cm) over time (s).
    TArray<TPair<double, double>> Path;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHolePlayTest, "UOpenGolf.Range.HolePlay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHolePlayTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FVerifyHolePlay>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
