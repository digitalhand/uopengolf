// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Editor.h"
#include "TestMaps.h"
#include "Engine/GameViewportClient.h"
#include "FileHelpers.h"
#include "OpenFairwayBall.h"
#include "LaunchMonitorTypes.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/PermissionTestAccess.h"
#include "Tests/RenderedPlay.h"
#include "Training/DistanceLadder.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"

namespace
{
    class FReplayLadderShots : public IAutomationLatentCommand
    {
    public:
        explicit FReplayLadderShots(FAutomationTestBase& InTest) : Test(InTest) {}

        bool Update() override
        {
            if (GetCurrentRunTime() > 90)
            {
                Test.AddError(TEXT("Timed out replaying the 35 yd ladder shot"));
                return true;
            }
            UWorld* World = GEditor->PlayWorld;
            auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
            auto* Player = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
            if (!Mode || !Player || !Mode->GetBall() || !Mode->GetTrainingSession() || !World->GetGameViewport())
                return false;

            ATrainingSession* Session = Mode->GetTrainingSession();
            if (ShotsHit == 0)
            {
                if (!Test.TestTrue(TEXT("Start Distance Ladder"), Session->StartDrill(UDistanceLadder::StaticClass(), 3)))
                    return true;
                FOpenGolfPermissionTestAccess::Set(World);
                Player->SelectCustomShot();
            }
            auto* Ladder = CastChecked<UDistanceLadder>(Session->Active());
            if (Mode->GetBall()->IsMoving())
            {
                if (Ladder->GetState().TotalShots != ShotsHit - 1)
                {
                    Test.AddError(TEXT("The ladder scored a ball still in motion"));
                    return true;
                }
                return false;
            }
            // These were the three scoring shots in the reported session, all hit at 16 degrees with 5000 rpm backspin.
            constexpr int32 Targets[] = {15, 25, 35};
            constexpr float SpeedsMph[] = {30, 38, 45};
            if (ShotsHit < UE_ARRAY_COUNT(Targets))
            {
                if (!Test.TestEqual(TEXT("Target before the shot"), Ladder->TargetYards(), Targets[ShotsHit]))
                    return true;
                FOpenGolfCustomShot Shot;
                Shot.BallSpeedMps = SpeedsMph[ShotsHit] / LaunchMonitorMphPerMps;
                Player->SetCustomShot(Shot);
                Player->InjectReplayShot();
                Pin = Mode->GetCupLocation();
                ++ShotsHit;
                return !Test.TestTrue(TEXT("The custom shot launched"), Mode->GetBall()->IsMoving());
            }

            if (!bCaptured)
            {
                const double Distance = FVector::Dist2D(Mode->GetBall()->GetActorLocation(), Pin);
                Test.AddInfo(FString::Printf(TEXT("35 yd shot stopped %.17g cm from the cup"), Distance));
                Test.TestEqual(TEXT("The 35 yd shot advances to 45 yd"), Ladder->TargetYards(), 45);
                Test.TestEqual(TEXT("Each stopped ball scored once"), Ladder->GetState().TotalShots, 3);
                const auto Window = World->GetGameViewport()->GetWindow();
                if (!Test.TestTrue(TEXT("A rendered Play window"), Window.IsValid()))
                    return true;
                Test.TestTrue(TEXT("The HUD awards the line touch"), RenderedPlay::HasText(Window.ToSharedRef(), TEXT("on the 3 ft line"), true));
                RenderedPlay::SaveWindowPng(Test, Window.ToSharedRef(), TEXT("TrainingGames/ladder-35yd-rest.png"));
                bCaptured = true;
            }
            if (Ladder->GetPendingYards())
                return false;
            Test.TestEqual(TEXT("The next pin is at 45 yd"), Ladder->GetPinYards(), 45);
            return true;
        }

    private:
        FAutomationTestBase& Test;
        FVector Pin;
        int32 ShotsHit = 0;
        bool bCaptured = false;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDistanceLadderReplayTest, "UOpenGolf.Visual.DistanceLadderReplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FDistanceLadderReplayTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(TestMaps::Range, false, true))
        return false;
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<RenderedPlay::FStartInWindow>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FReplayLadderShots>(*this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
