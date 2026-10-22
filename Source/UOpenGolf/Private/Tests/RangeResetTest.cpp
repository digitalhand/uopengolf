// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"

// A stopped ball keeps its place for the reset delay, then goes back to the tee with its
// shot result and launch-monitor shot intact. A shot hit during the wait is not interrupted.
class FVerifyRangeAutoReset : public IAutomationLatentCommand
{
public:
    explicit FVerifyRangeAutoReset(FAutomationTestBase* InTest)
        : Test(InTest) {}

    virtual bool Update() override
    {
        if (GetCurrentRunTime() > 90.0)
        {
            Test->AddError(TEXT("Timed out verifying the automatic tee reset."));
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        auto* Controller = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
        if (!World || !World->HasBegunPlay() || !Mode || !Mode->GetRange() || !Mode->GetBall() || !Controller)
        {
            return false;
        }
        const AOpenFairwayBall* Ball = Mode->GetBall();
        const UOpenLaunchMonitorSubsystem* Monitor = World->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>();
        const double Now = World->GetTimeSeconds();
        const double FromTeeCm = FVector::Dist2D(Ball->GetActorLocation(), Mode->GetRange()->GetTeeLocation());
        FOpenFairwayShotResult Result;
        const bool bHasResult = Ball->GetShotResult(Result);

        if (Stage == 0)
        {
            Controller->SelectReplayShot(0);
            Controller->InjectReplayShot();
            ++Stage;
            return false;
        }
        if (Stage == 1)
        {
            if (!bHasResult || !Result.bAtRest)
            {
                return false;
            }
            // Hit again while the first shot's reset is pending.
            const int32 FirstShotNumber = Result.ShotNumber;
            Controller->InjectReplayShot();
            Ball->GetShotResult(Result);
            Test->TestTrue(TEXT("Next shot replaces the result"), Result.ShotNumber == FirstShotNumber + 1 && !Result.bAtRest);
            ++Stage;
            return false;
        }
        if (Stage == 2)
        {
            if (!Result.bAtRest)
            {
                if (!Ball->IsMoving())
                {
                    Test->AddError(TEXT("The previous shot's reset stopped the next shot."));
                    return true;
                }
                return false;
            }
            RestedAt = Now;
            RestLocation = Ball->GetActorLocation();
            Rested = Result;
            FLaunchMonitorShot Shot;
            Test->TestTrue(TEXT("Launch-monitor shot available"), Monitor && Monitor->GetLastShot(Shot));
            MonitorShotNumber = Shot.ShotNumber;
            Test->TestTrue(TEXT("Shot stops downrange"), FromTeeCm > 5000.0);
            ++Stage;
            return false;
        }

        if (Now < RestedAt + AUOpenGolfGameMode::ResetDelayS - 0.5)
        {
            if (!Ball->GetActorLocation().Equals(RestLocation, 0.1))
            {
                Test->AddError(TEXT("The ball left its resting place before the reset delay."));
                return true;
            }
            return false;
        }
        if (Now < RestedAt + AUOpenGolfGameMode::ResetDelayS + 0.5)
        {
            return false;
        }
        Test->AddInfo(FString::Printf(TEXT("Shot %d stopped %.1f yd downrange; %.2f s later the ball is %.2f cm from the tee and shows %.1f yd carry, %.1f yd total."),
            Rested.ShotNumber, Rested.TotalM * OpenFairway::YardsPerMeter, Now - RestedAt, FromTeeCm, Result.CarryM * OpenFairway::YardsPerMeter,
            Result.TotalM * OpenFairway::YardsPerMeter));
        Test->TestEqual(TEXT("Ball back on the tee"), FromTeeCm, 0.0, 1.0);
        Test->TestFalse(TEXT("Teed-up ball at rest"), Ball->IsMoving());
        Test->TestTrue(TEXT("Shot result kept"), bHasResult && Result.ShotNumber == Rested.ShotNumber && Result.bAtRest);
        Test->TestEqual(TEXT("Carry kept"), Result.CarryM, Rested.CarryM);
        Test->TestEqual(TEXT("Roll kept"), Result.RollM, Rested.RollM);
        Test->TestEqual(TEXT("Total kept"), Result.TotalM, Rested.TotalM);
        FLaunchMonitorShot Shot;
        Test->TestTrue(TEXT("Launch-monitor shot kept"), Monitor && Monitor->GetLastShot(Shot) && Shot.ShotNumber == MonitorShotNumber);
        return true;
    }

private:
    FAutomationTestBase* Test;
    int32 Stage = 0;
    double RestedAt = 0.0;
    FVector RestLocation = FVector::ZeroVector;
    FOpenFairwayShotResult Rested;
    int32 MonitorShotNumber = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangeAutoResetTest, "UOpenGolf.Range.AutoReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRangeAutoResetTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FVerifyRangeAutoReset>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
