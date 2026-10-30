// SPDX-License-Identifier: MIT
// The follow camera closes in on a ball near the cup, backs out as it rolls on past 8 ft or when a miss stops, and stays
// close on a holed ball until it is teed up again.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "LaunchMonitorTypes.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenGolfTerrain.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"

namespace CupZoomCameraTestDetail
{
    /** A putt as the launch monitor sends it, so the mode hits it as a real shot: speed and direction, no launch angle or spin. */
    FLaunchMonitorShot Putt(float Mph, float HorizontalLaunchDeg)
    {
        FLaunchMonitorShot Shot;
        Shot.Source = TEXT("Replay");
        Shot.ShotType = ELaunchMonitorShotType::Putt;
        Shot.BallSpeedMps = Mph / LaunchMonitorMphPerMps;
        Shot.HorizontalLaunchDeg = HorizontalLaunchDeg;
        Shot.BallSpeedSource = Shot.VerticalLaunchSource = Shot.HorizontalLaunchSource = Shot.TotalSpinSource = ELaunchMonitorValueSource::Measured;
        return Shot;
    }
} // namespace CupZoomCameraTestDetail

class FVerifyCupZoomCamera : public IAutomationLatentCommand
{
public:
    explicit FVerifyCupZoomCamera(FAutomationTestBase* InTest) : Test(InTest) {}

    virtual bool Update() override
    {
        using namespace CupZoomCameraTestDetail;
        if (GetCurrentRunTime() > 120.0)
        {
            Test->AddError(FString::Printf(TEXT("Timed out at stage %d."), Stage));
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        auto* Controller = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
        AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
        UOpenLaunchMonitorSubsystem* LaunchMonitor = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>() : nullptr;
        if (!Mode || !Ball || !Controller || !Controller->GetViewTarget() || !LaunchMonitor || !Mode->GetRange() || !Mode->GetRange()->Terrain)
            return false;
        const FVector Cup = Mode->GetCupLocation();
        // A spot on the green Distance cm short of the cup along the target line.
        auto GreenSpot = [&](double DistanceCm)
        {
            FVector Ground, Normal;
            Mode->GetRange()->Terrain->GroundAt(Cup - FVector(DistanceCm, 0, 0), Ground, Normal);
            return Ground;
        };
        const double CameraToBallCm = FVector::Dist2D(Controller->GetViewTarget()->GetActorLocation(), Ball->GetActorLocation());
        const double Now = World->GetTimeSeconds();

        switch (Stage)
        {
        case 0:
            Controller->TogglePlayMode();
            Test->TestTrue(TEXT("The ball is placed 3 m from the cup"), Mode->PlaceBallAt(GreenSpot(300)));
            // Off line, it passes about 26 cm beside the cup and rolls on well past 8 ft.
            LaunchMonitor->OnShotReceivedNative.Broadcast(Putt(8.0f, 5.0f));
            Test->TestTrue(TEXT("The firm putt is hit"), Ball->IsMoving());
            ++Stage;
            return false;
        case 1:
            if (Ball->IsMoving())
            {
                PeakZoom = FMath::Max(PeakZoom, Controller->GetCupZoom());
                const double FromCupCm = FVector::Dist(Ball->GetActorLocation(), Cup);
                if (!bCheckedPast && Ball->GetActorLocation().X > Cup.X && FromCupCm > 400.0)
                {
                    Test->TestTrue(TEXT("The camera backs out once the ball rolls past 8 ft, still moving"), Controller->GetCupZoom() < 0.1f);
                    Test->AddInfo(FString::Printf(TEXT("Rolling %.0f cm from the cup: zoom %.3f (closest %.2f)."), FromCupCm, Controller->GetCupZoom(), PeakZoom));
                    bCheckedPast = true;
                }
                return false;
            }
            Test->TestTrue(TEXT("The putt rolled on more than 4 m past the cup"), bCheckedPast);
            Test->TestTrue(TEXT("The camera closed in as the ball passed the cup"), PeakZoom > 0.1f);
            StoppedAt = Now;
            PeakZoom = 0.0f;
            ++Stage;
            return false;
        case 2:
            if (Now - StoppedAt < AUOpenGolfGameMode::ResetDelayS + 0.5)
                return false;
            Test->TestTrue(TEXT("The practice ball is back at address after the long putt"), Mode->IsBallAtAddress());
            Test->TestTrue(TEXT("The ball is placed 1 m from the cup"), Mode->PlaceBallAt(GreenSpot(100)));
            // Off line, as HolePlayTest's miss.
            LaunchMonitor->OnShotReceivedNative.Broadcast(Putt(3.0f, 5.0f));
            Test->TestTrue(TEXT("The missed putt is hit"), Ball->IsMoving());
            ++Stage;
            return false;
        case 3:
            if (Ball->IsMoving())
            {
                PeakZoom = FMath::Max(PeakZoom, Controller->GetCupZoom());
                return false;
            }
            Test->TestFalse(TEXT("The putt misses"), Ball->IsHoled());
            Test->TestTrue(TEXT("The camera closes in as the ball nears the cup"), PeakZoom > 0.1f);
            Test->AddInfo(FString::Printf(TEXT("Missed putt: closest zoom %.2f."), PeakZoom));
            StoppedAt = Now;
            ++Stage;
            return false;
        case 4:
            // Before the practice reset (ResetDelayS) puts the ball back.
            if (Now - StoppedAt < 2.0)
                return false;
            Test->TestFalse(TEXT("The missed ball still lies where it stopped"), Mode->IsBallAtAddress());
            Test->TestTrue(TEXT("The camera backs out after the ball stops"), Controller->GetCupZoom() < 0.1f);
            Test->TestTrue(TEXT("The camera is back at the follow distance"), CameraToBallCm > 600.0);
            Test->AddInfo(FString::Printf(TEXT("Missed putt at rest: zoom %.3f, camera %.0f cm from the ball."), Controller->GetCupZoom(), CameraToBallCm));
            ++Stage;
            return false;
        case 5:
            if (Now - StoppedAt < AUOpenGolfGameMode::ResetDelayS + 0.5)
                return false;
            Test->TestTrue(TEXT("The practice ball is back at address"), Mode->IsBallAtAddress());
            Test->TestEqual(TEXT("No zoom at address"), Controller->GetCupZoom(), 0.0f);
            LaunchMonitor->OnShotReceivedNative.Broadcast(Putt(3.0f, 0.0f));
            Test->TestTrue(TEXT("The straight putt is hit"), Ball->IsMoving());
            ++Stage;
            return false;
        case 6:
            if (Ball->IsMoving())
                return false;
            Test->TestTrue(TEXT("A dying putt on line drops"), Ball->IsHoled());
            HoledAt = Now;
            ++Stage;
            return false;
        case 7:
            // Before the practice reset (ResetDelayS) takes the ball out of the cup.
            if (Now - HoledAt < 2.5)
                return false;
            Test->TestTrue(TEXT("The ball is still in the cup"), Ball->IsHoled());
            Test->TestTrue(TEXT("The camera stays zoomed in on a holed ball"), Controller->GetCupZoom() > 0.8f);
            Test->TestTrue(TEXT("The camera is close to the cup"), CameraToBallCm < 400.0);
            Test->AddInfo(FString::Printf(TEXT("Holed: zoom %.3f, camera %.0f cm from the ball."), Controller->GetCupZoom(), CameraToBallCm));
            ++Stage;
            return false;
        case 8:
            if (Now - HoledAt < AUOpenGolfGameMode::ResetDelayS + 0.5)
                return false;
            Test->TestTrue(TEXT("After the hole-out the ball is back at address"), Mode->IsBallAtAddress() && !Ball->IsHoled());
            Test->TestEqual(TEXT("The zoom resets with the ball"), Controller->GetCupZoom(), 0.0f);
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
    float PeakZoom = 0.0f;
    bool bCheckedPast = false;
    double StoppedAt = 0.0;
    double HoledAt = 0.0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCupZoomCameraTest, "UOpenGolf.Range.CupZoomCamera",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCupZoomCameraTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FVerifyCupZoomCamera>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
