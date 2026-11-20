// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Tests/RenderedPlay.h"
#include "Tests/HoleMapContent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "OpenFairwayBall.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"
#include "Tests/PermissionTestAccess.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoleMapFrameTest, "UOpenGolf.Minimap.Projection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHoleMapFrameTest::RunTest(const FString&)
{
    TestEqual(TEXT("The render target keeps the frame's aspect"), double(FHoleMapFrame::TextureWidth) / FHoleMapFrame::TextureHeight,
        FHoleMapFrame::Aspect, 1e-3);
    for (const FVector Cup : {FVector(36576, 0, 20), FVector(-19000, 24000, 300)})
    {
        const FVector Tee(-3500, 700, 95);
        const auto Frame = FHoleMapFrame::FromHole(Tee, Cup, 3200, 18000, 2800);
        TestTrue(TEXT("Pin above tee"), Frame.ToUV(FVector2D(Cup)).Y < Frame.ToUV(FVector2D(Tee)).Y);
        for (const FVector Point : {Tee, Cup, Tee + FVector(5000, -890, 0)})
            TestTrue(TEXT("World hit round trip"), Frame.ToWorld(Frame.ToUV(FVector2D(Point))).Equals(FVector2D(Point), .01));
    }
    TestEqual(TEXT("A tee on its cup still frames the minimum"), FHoleMapFrame::FromHole(FVector::ZeroVector, FVector::ZeroVector, 0, 0, 0).Length,
        FHoleMapFrame::MinimumLength);
    return true;
}

namespace
{
    /** Clicks on the HUD's hole map: aiming at address, nothing during placement or flight, right clicks to the game. */
    class FClickHoleMap : public IAutomationLatentCommand
    {
    public:
        explicit FClickHoleMap(FAutomationTestBase& InTest)
            : Test(InTest) {}

        bool Update() override
        {
            if (GetCurrentRunTime() > 100)
            {
                Test.AddError(FString::Printf(TEXT("Timed out at stage %d waiting for %s"), Stage, WaitingFor));
                return true;
            }
            // Records what the test is waiting for, so a timeout names it.
            const auto Wait = [this](const TCHAR* For) { WaitingFor = For; return false; };
            UWorld* World = GEditor->PlayWorld;
            auto* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
            if (!Mode || !Mode->GetBall() || !World->GetGameViewport())
                return Wait(TEXT("Play to start"));
            const TSharedPtr<SWindow> Window = World->GetGameViewport()->GetWindow();
            if (!Window)
                return Wait(TEXT("the game window"));
            const TSharedPtr<SWidget> Map = RenderedPlay::WithTag(Window.ToSharedRef(), TEXT("HoleMinimap"));
            if (!Map)
                return Wait(TEXT("the minimap widget"));
            if (Map->GetTickSpaceGeometry().GetLocalSize().X < 1)
                return Wait(Map->GetVisibility().IsVisible() ? TEXT("the minimap's layout") : TEXT("the hole map capture"));

            auto* Player = CastChecked<AUOpenGolfPlayerController>(World->GetFirstPlayerController());
            const FVector2D AimPoint = RenderedPlay::PointOn(*Map, {.9, .65});
            const auto Click = [&] { RenderedPlay::Click(Window.ToSharedRef(), AimPoint); };
            switch (Stage)
            {
            case 0:
            {
                const UTextureRenderTarget2D* Picture = Mode->GetRange()->GetHoleMapTexture();
                Test.TestTrue(TEXT("The hole map is captured at the frame's size"), Picture && Picture->SizeX == FHoleMapFrame::TextureWidth
                    && Picture->SizeY == FHoleMapFrame::TextureHeight);
                HoleMapContent::ShowsTurf(Test, Mode->GetRange()->GetHoleMapTexture(), TEXT("Training"));
                Click();
                Test.TestTrue(TEXT("Click turns range aim right"), Player->GetAimYaw() > 5);
                const FVector Lie = Mode->GetRange()->GetTeeLocation() + FVector(6500, 1000, 0);
                Test.TestTrue(TEXT("Practice lie placed"), Mode->PlaceBallAt(Lie));
                PreviousYaw = Player->GetAimYaw();
                Player->SetPlacingBall(true);
                Click();
                Test.TestTrue(TEXT("Map cannot place ball or change aim during placement"),
                    Player->IsPlacingBall() && Player->GetAimYaw() == PreviousYaw);
                // Other buttons pass through the map to the game, where a right click cancels placing as anywhere else.
                const FPointerEvent RightDown(0, AimPoint, AimPoint, TSet<FKey>{EKeys::RightMouseButton}, EKeys::RightMouseButton, 0.0f, FModifierKeysState());
                const FPointerEvent RightUp(0, AimPoint, AimPoint, TSet<FKey>(), EKeys::RightMouseButton, 0.0f, FModifierKeysState());
                Test.TestFalse(TEXT("The map leaves the right button to the game"),
                    Map->OnMouseButtonDown(Map->GetTickSpaceGeometry(), RightDown).IsEventHandled());
                FSlateApplication::Get().ProcessMouseButtonDownEvent(nullptr, RightDown);
                FSlateApplication::Get().ProcessMouseButtonUpEvent(RightUp);
                CancelBy = GetCurrentRunTime() + 1;
                ++Stage;
                return false;
            }
            case 1:
                // The game reads the right click on its next input tick.
                if (Player->IsPlacingBall() && GetCurrentRunTime() < CancelBy)
                    return Wait(TEXT("the right click to cancel placing"));
                Test.TestFalse(TEXT("A right click on the map cancels placing"), Player->IsPlacingBall());
                Player->CancelPlacingBall();
                Click();
                Test.TestTrue(TEXT("Click aims from new lie"), Player->GetAimYaw() > PreviousYaw + 5);
                ShotOrigin = Mode->GetBall()->GetActorLocation();
                CaptureAt = GetCurrentRunTime() + 1;
                ++Stage;
                return false;
            case 2:
            {
                if (GetCurrentRunTime() < CaptureAt)
                    return Wait(TEXT("the address view to settle"));
                RenderedPlay::SaveWindowPng(Test, Window.ToSharedRef(), TEXT("Minimap-address.png"));
                FOpenGolfPermissionTestAccess::Set(World);
                Player->InjectReplayShot();
                const float MovingYaw = Player->GetAimYaw();
                RenderedPlay::Click(Window.ToSharedRef(), RenderedPlay::PointOn(*Map, {.1, .6}));
                Test.TestEqual(TEXT("Map click preserves aim in flight"), Player->GetAimYaw(), MovingYaw);
                Window->Resize(FVector2D(960, 600));
                ++Stage;
                return false;
            }
            default:
                if (!Mode->GetBall()->GetResult().bAtRest)
                    return Wait(TEXT("the shot to come to rest"));
                Test.TestTrue(TEXT("New lie after rollout"), FVector::Dist2D(Mode->GetBall()->GetActorLocation(), ShotOrigin) > 100);
                RenderedPlay::SaveWindowPng(Test, Window.ToSharedRef(), TEXT("Minimap-landed.png"));
                Player->ResetBallToTee();
                return true;
            }
        }

    private:
        FAutomationTestBase& Test;
        int32 Stage = 0;
        float PreviousYaw = 0;
        double CancelBy = 0, CaptureAt = 0;
        const TCHAR* WaitingFor = TEXT("Play to start");
        FVector ShotOrigin;
    };
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoleMapClickTest, "UOpenGolf.Minimap.RenderedClick",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FHoleMapClickTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
        return false;
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<RenderedPlay::FStartInWindow>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FClickHoleMap>(*this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
