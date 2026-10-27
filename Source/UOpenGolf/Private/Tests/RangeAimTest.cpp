// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"

// The aim turns the teed ball and the tee camera, is limited, cannot change during or after a
// shot, turns the launch itself and stays set after the automatic re-tee.
class FVerifyRangeAim : public IAutomationLatentCommand
{
public:
    explicit FVerifyRangeAim(FAutomationTestBase* InTest)
        : Test(InTest) {}

    virtual bool Update() override
    {
        if (GetCurrentRunTime() > 90.0)
        {
            Test->AddError(TEXT("Timed out verifying the range aim."));
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        auto* Controller = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
        if (!World || !World->HasBegunPlay() || !Mode || !Mode->GetRange() || !Mode->GetBall() || !Controller || !Controller->GetViewTarget())
        {
            return false;
        }
        const AOpenFairwayBall* Ball = Mode->GetBall();
        const FVector FromTee = Ball->GetActorLocation() - Mode->GetRange()->GetTeeLocation();
        const double FromTeeCm = FromTee.Size2D();
        const double BearingDeg = FMath::RadiansToDegrees(FMath::Atan2(FromTee.Y, FromTee.X));
        FOpenFairwayShotResult Result;
        const bool bHasResult = Ball->GetShotResult(Result);

        switch (Stage)
        {
        case 0:
            Test->TestTrue(TEXT("Aim turns on the tee"), Controller->SetAimYaw(30.0f));
            TurnFrame = GFrameCounter;
            CameraYawBeforeTurn = Controller->GetViewTarget()->GetActorRotation().Yaw;
            break;
        case 1:
            // In the editor's first Play session the controller can miss the frame after the turn, so wait
            // for the camera's first update (a few frames at most). That update must already be the full turn.
            if (FMath::IsNearlyEqual(Controller->GetViewTarget()->GetActorRotation().Yaw, CameraYawBeforeTurn, 1.0e-4) && GFrameCounter - TurnFrame < 10)
                return false;
            Test->AddInfo(FString::Printf(TEXT("Tee camera moved %llu frames after the turn."), GFrameCounter - TurnFrame));
            Test->TestEqual(TEXT("Teed ball faces the aim"), Ball->GetActorRotation().Yaw, 30.0, 0.01);
            Test->TestEqual(TEXT("Tee camera faces the aim"), Controller->GetViewTarget()->GetActorRotation().Yaw, 30.0, 0.01);
            {
                // The camera moves with the turn, not after it: it is already behind the ball on the new line.
                const FVector CameraToBall = Ball->GetActorLocation() - Controller->GetViewTarget()->GetActorLocation();
                Test->TestEqual(TEXT("Tee camera sits on the aim line"), FMath::RadiansToDegrees(FMath::Atan2(CameraToBall.Y, CameraToBall.X)), 30.0, 0.5);
                Test->TestEqual(TEXT("Tee camera keeps its distance"), CameraToBall.Size2D(), 600.0, 1.0);
            }
            Controller->SetAimYaw(90.0f);
            Test->TestEqual(TEXT("Aim is limited"), Controller->GetAimYaw(), AUOpenGolfPlayerController::MaxAimYawDeg);
            Controller->SetAimYaw(0.0f);
            Controller->SelectReplayShot(0);
            Controller->InjectReplayShot();
            break;
        case 2:
            if (FromTeeCm < 300.0)
                return false;
            StraightBearingDeg = BearingDeg;
            Test->TestFalse(TEXT("Aim cannot turn in flight"), Controller->SetAimYaw(10.0f));
            Test->TestEqual(TEXT("Aim unchanged in flight"), Controller->GetAimYaw(), 0.0f);
            Controller->ResetBallToTee();
            break;
        case 3:
            Test->TestTrue(TEXT("Aim turns after the re-tee"), Controller->SetAimYaw(20.0f));
            Controller->InjectReplayShot();
            break;
        case 4:
            if (FromTeeCm < 300.0)
                return false;
            // The same shot leaves the tee turned by exactly the aim.
            Test->TestEqual(TEXT("Launch follows the aim"), BearingDeg - 20.0, StraightBearingDeg, 0.5);
            break;
        case 5:
            if (!bHasResult || !Result.bAtRest)
                return false;
            Test->TestFalse(TEXT("Aim cannot turn under a stopped ball"), Controller->SetAimYaw(0.0f));
            Test->AddInfo(FString::Printf(TEXT("Aimed 20 deg right: stopped %.1f m from the tee at bearing %.1f deg, %.1f m offline, lie %s."),
                FromTeeCm / 100.0, BearingDeg, Result.LateralM, *UEnum::GetValueAsString(Result.Surface)));
            break;
        default:
            if (Ball->IsMoving() || FromTeeCm > 1.0)
                return false;
            Test->TestEqual(TEXT("Aim kept after the automatic re-tee"), Controller->GetAimYaw(), 20.0f);
            Test->TestEqual(TEXT("Re-teed ball faces the aim"), Ball->GetActorRotation().Yaw, 20.0, 0.01);
            return true;
        }
        ++Stage;
        return false;
    }

private:
    FAutomationTestBase* Test;
    int32 Stage = 0;
    uint64 TurnFrame = 0;
    double CameraYawBeforeTurn = 0.0;
    double StraightBearingDeg = 0.0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangeAimTest, "UOpenGolf.Range.Aim",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRangeAimTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FVerifyRangeAim>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
