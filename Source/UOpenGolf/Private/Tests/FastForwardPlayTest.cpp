// SPDX-License-Identifier: MIT
// Space on the range: holding it plays a rolling ball four times faster, a tap skips to where it stops, the world is
// back at normal speed the moment it rests, and the skipped shot stops exactly where an unskipped one does.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "TestMaps.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "Play/OpenGolfFastForward.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/PermissionTestAccess.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"

namespace FastForwardPlayTestDetail
{
    using namespace OpenGolfPlay;

    class FCheckFastForward : public IAutomationLatentCommand
    {
    public:
        explicit FCheckFastForward(FAutomationTestBase* InTest) : Test(InTest) {}

        virtual bool Update() override
        {
            if (GetCurrentRunTime() > 120.0)
            {
                Test->AddError(FString::Printf(TEXT("Timed out at stage %d."), Stage));
                return true;
            }
            UWorld* World = GEditor->PlayWorld;
            auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
            auto* Controller = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
            if (!Mode || !Mode->GetBall() || !Controller || !Controller->GetKeyMapping())
                return false;
            AOpenFairwayBall* Ball = Mode->GetBall();
            const float Dilation = UGameplayStatics::GetGlobalTimeDilation(World);
            FOpenFairwayShotResult Result;
            const bool bResult = Ball->GetShotResult(Result);
            const bool bRolling = Ball->GetBallState() == EOpenFairwayBallState::Rollout;
            const double Now = FPlatformTime::Seconds();
            switch (Stage)
            {
            case 0:
                // A full shot with a long roll-out, hit on the range.
                if (!Mode->IsBallAtAddress())
                    return false;
                Hit(*Controller);
                break;
            case 1:
                if (Ball->GetBallState() == EOpenFairwayBallState::Flight)
                {
                    // Held in the air, nothing speeds up until the ball lands.
                    if (!bPressedInFlight)
                    {
                        Controller->ForwardUIKey(EKeys::SpaceBar, true);
                        bPressedInFlight = true;
                    }
                    Test->TestEqual(TEXT("Nothing speeds up while the ball flies"), Dilation, 1.0f);
                    return false;
                }
                if (!bRolling || Dilation != FastForwardHoldRate)
                    return Fail(bResult && Result.bAtRest, TEXT("The ball rested before the hold took effect."));
                HeldSince = Now;
                break;
            case 2:
                // Hold past a tap, then let go: normal speed again while the ball still rolls.
                if (Now - HeldSince < 2.0 * FastForwardTapSeconds)
                    return false;
                Controller->ForwardUIKey(EKeys::SpaceBar, false);
                break;
            case 3:
                if (Dilation != 1.0f)
                    return Fail(!bRolling, TEXT("The roll ended before the hold was released."));
                Test->TestTrue(TEXT("Releasing the hold plays the roll at normal speed"), bRolling);
                // A tap: pressed, then released on the next frame, well inside the tap time.
                Controller->ForwardUIKey(EKeys::SpaceBar, true);
                break;
            case 4:
                Controller->ForwardUIKey(EKeys::SpaceBar, false);
                ++Stage;
                return false;
            case 5:
                bSawSkip |= Dilation == FastForwardSkipRate;
                if (!bResult || !Result.bAtRest)
                    return false;
                Test->TestTrue(TEXT("A tap skips to where the ball stops"), bSawSkip);
                Test->TestEqual(TEXT("At rest the world is at normal speed"), Dilation, 1.0f);
                SkippedTotalM = Result.TotalM;
                RestedAt = Now;
                break;
            case 6:
                // The rest timers run in real time: the ball is still where it stopped well into the reset delay.
                if (Now - RestedAt < 0.5 * AUOpenGolfGameMode::ResetDelayS)
                {
                    Test->TestFalse(TEXT("The reset waits its full delay after a skip"), Mode->IsBallAtAddress() && Now - RestedAt < 1.0);
                    return false;
                }
                if (!Mode->IsBallAtAddress())
                    return false;
                Hit(*Controller);
                break;
            case 7:
                if (!bResult || !Result.bAtRest || Result.ShotNumber == SkippedShot)
                    return false;
                Test->TestEqual(TEXT("A skipped shot stops where the same shot stops at normal speed"), SkippedTotalM, Result.TotalM, 0.01f);
                return true;
            }
            if (Stage == 5)
                SkippedShot = Result.ShotNumber;
            ++Stage;
            return false;
        }

    private:
        void Hit(AUOpenGolfPlayerController& Controller) const
        {
            Controller.SelectReplayShot(0);
            FOpenGolfPermissionTestAccess::Set(Controller.GetWorld());
            Controller.InjectReplayShot();
        }

        /** With bGiveUp, ends the test with Message as its error; otherwise the stage keeps waiting. */
        bool Fail(bool bGiveUp, const TCHAR* Message) const
        {
            if (!bGiveUp)
                return false;
            Test->AddError(Message);
            return true;
        }

        FAutomationTestBase* Test;
        int32 Stage = 0;
        bool bPressedInFlight = false;
        bool bSawSkip = false;
        double HeldSince = 0.0;
        double RestedAt = 0.0;
        float SkippedTotalM = 0.0f;
        int32 SkippedShot = INDEX_NONE;
    };
} // namespace FastForwardPlayTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFastForwardPlayTest, "UOpenGolf.Range.FastForward",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFastForwardPlayTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(TestMaps::Range, false, true))
        return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FastForwardPlayTestDetail::FCheckFastForward>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
