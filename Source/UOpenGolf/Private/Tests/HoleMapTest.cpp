// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Tests/RenderedPlay.h"
#include "TestMaps.h"
#include "Tests/HoleMapContent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "OpenFairwayBall.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Tests/BundledCourse.h"
#include "Tests/LieSearch.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"
#include "Tests/PermissionTestAccess.h"
#include "UI/Hud/HoleMinimap.h"
#include "Widgets/Text/STextBlock.h"

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

namespace HoleMapTestDetail
{
    /** The text of the header's block tagged Tag, or empty when it is missing. */
    FString HeaderText(const TSharedRef<SWindow>& Window, FName Tag)
    {
        const TSharedPtr<SWidget> Text = RenderedPlay::WithTag(Window, Tag);
        return Text ? StaticCastSharedPtr<STextBlock>(Text)->GetText().ToString() : FString();
    }

    /** The lie the hole map's header shows, or empty when the header is missing. */
    FString HeaderLie(const TSharedRef<SWindow>& Window) { return HeaderText(Window, TEXT("HoleMapLie")); }
} // namespace HoleMapTestDetail

namespace
{
    using HoleMapTestDetail::HeaderLie;
    using HoleMapTestDetail::HeaderText;

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
                const AUOpenGolfRange* Range = Mode->GetRange();
                const UTextureRenderTarget2D* Capture = Range->GetHoleMapCaptureTarget();
                Test.TestTrue(TEXT("The hole map is captured at the frame's size"), Capture && Capture->SizeX == FHoleMapFrame::TextureWidth
                    && Capture->SizeY == FHoleMapFrame::TextureHeight);
                HoleMapContent::ShowsTurf(Test, Range->GetHoleMapCaptureTarget(), TEXT("Training"));
                const UTexture2D* Picture = Range->GetHoleMapTexture();
                Test.TestTrue(TEXT("The finished picture has mips for the zoomed-out view"), Picture && Picture->GetNumMips() > 1);
                Test.TestTrue(TEXT("The hole map samples the ground's height"), Range->GetHoleMapHeights().IsValid());
                Test.TestTrue(TEXT("The hole map has its header and elevation footer"), RenderedPlay::WithTag(Window.ToSharedRef(), TEXT("HoleMapHeader")).IsValid()
                    && RenderedPlay::WithTag(Window.ToSharedRef(), TEXT("HoleMapFooter")).IsValid());
                // The hole and par are in the course heading; the header keeps the lie and its penalties.
                Test.TestTrue(TEXT("The header shows the lie's speed and spin penalties"),
                    RenderedPlay::WithTag(Window.ToSharedRef(), TEXT("HoleMapLieCut")).IsValid() && RenderedPlay::WithTag(Window.ToSharedRef(), TEXT("HoleMapSpinCut")).IsValid());
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
                {
                    // Away from the tee the map zooms toward the ball and the cup; a click aims at the point it shows.
                    const FHoleMapFrame View = StaticCastSharedPtr<SHoleMinimap>(Map)->GetView();
                    const FVector2D Target = View.ToWorld({.9, .65});
                    const FVector2D FromBall = Target - FVector2D(Mode->GetBall()->GetActorLocation());
                    Click();
                    Test.TestTrue(TEXT("Click aims from new lie"), Player->GetAimYaw() > PreviousYaw + 5);
                    // The aim is limited either side of the line to the cup, which a practice lie aims along.
                    const FVector2D ToCup = FVector2D(Mode->GetCupLocation() - Mode->GetBall()->GetActorLocation());
                    const double CupDeg = FMath::RadiansToDegrees(FMath::Atan2(ToCup.Y, ToCup.X));
                    const double ClickDeg = FMath::RadiansToDegrees(FMath::Atan2(FromBall.Y, FromBall.X));
                    const double Limit = AUOpenGolfPlayerController::MaxAimYawDeg;
                    Test.TestEqual(TEXT("Click aims at the point the zoomed map shows"), double(Player->GetAimYaw()),
                        CupDeg + FMath::Clamp(FRotator::NormalizeAxis(ClickDeg - CupDeg), -Limit, Limit), 0.5);
                    Test.TestEqual(TEXT("Click sets the target at that point"), Player->GetAimTargetDistanceCm(), FromBall.Size(), 50.0);
                }
                ShotOrigin = Mode->GetBall()->GetActorLocation();
                CaptureAt = GetCurrentRunTime() + 1;
                ++Stage;
                return false;
            case 2:
            {
                if (GetCurrentRunTime() < CaptureAt)
                    return Wait(TEXT("the address view to settle"));
                RenderedPlay::SaveWindowPng(Test, Window.ToSharedRef(), TEXT("Minimap-address.png"));
                AddressLie = HeaderLie(Window.ToSharedRef());
                Test.TestFalse(TEXT("The header names the lie at address"), AddressLie.IsEmpty());
                Test.TestTrue(TEXT("A Training lie shows no penalty"), HeaderText(Window.ToSharedRef(), TEXT("HoleMapLieCut")).IsEmpty()
                    && HeaderText(Window.ToSharedRef(), TEXT("HoleMapSpinCut")).IsEmpty());
                FOpenGolfPermissionTestAccess::Set(World);
                Player->InjectReplayShot();
                const float MovingYaw = Player->GetAimYaw();
                RenderedPlay::Click(Window.ToSharedRef(), RenderedPlay::PointOn(*Map, {.1, .6}));
                Test.TestEqual(TEXT("Map click preserves aim in flight"), Player->GetAimYaw(), MovingYaw);
                Window->Resize(FVector2D(960, 600));
                ++Stage;
                return false;
            }
            case 3:
                // The header has ticked at least once since the launch: in the air it keeps the lie the ball was hit from.
                if (!bCheckedFlightLie && Mode->GetBall()->GetBallState() == EOpenFairwayBallState::Flight)
                {
                    const FString FlightLie = HeaderLie(Window.ToSharedRef());
                    Test.TestEqual(TEXT("In flight the header keeps the lie the ball was hit from"), FlightLie, AddressLie);
                    Test.TestFalse(TEXT("The header never says In flight"), FlightLie == TEXT("In flight"));
                    bCheckedFlightLie = true;
                }
                if (!Mode->GetBall()->GetResult().bAtRest)
                    return Wait(TEXT("the shot to come to rest"));
                Test.TestTrue(TEXT("The flight lie was checked"), bCheckedFlightLie);
                Test.TestTrue(TEXT("New lie after rollout"), FVector::Dist2D(Mode->GetBall()->GetActorLocation(), ShotOrigin) > 100);
                RenderedPlay::SaveWindowPng(Test, Window.ToSharedRef(), TEXT("Minimap-landed.png"));
                Player->ResetBallToTee();
                ++Stage;
                return false;
            case 4:
            {
                // A course round, as Courses › Play starts one but without level travel, with the ball in the rough.
                const UGolfCourseDefinition* Course = BundledCourse::Load();
                if (!Test.TestNotNull(TEXT("The bundled course loads"), Course))
                    return true;
                if (Mode->GetBall()->IsMoving())
                    Mode->GetBall()->PlaceAt(Mode->GetBall()->GetActorLocation());
                PlayersClub = World->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>()->GetClub();
                Mode->SetPlayMode(EOpenGolfPlayMode::Hole, Course);
                FVector Rough;
                if (!Test.TestTrue(TEXT("The hole has rough"), LieSearch::FindApproachRough(*Mode, Rough)))
                    return true;
                Mode->GetBall()->PlaceAt(Rough);
                CaptureAt = GetCurrentRunTime() + 1;
                ++Stage;
                return false;
            }
            case 5:
            {
                if (GetCurrentRunTime() < CaptureAt)
                    return Wait(TEXT("the course header to update"));
                // The hole and par are in the course heading; the header is the lie and what it takes off the shot.
                const FOpenGolfLieCut Cut = Mode->GetLieCut();
                Test.TestFalse(TEXT("The rough is penalised in a course round"), Cut.IsNone());
                Test.TestEqual(TEXT("The header names the rough"), HeaderLie(Window.ToSharedRef()), FString(TEXT("Rough")));
                Test.TestEqual(TEXT("The speed penalty is beside the lie"), HeaderText(Window.ToSharedRef(), TEXT("HoleMapLieCut")), OpenGolfPlay::LieCutLabel(Cut));
                Test.TestEqual(TEXT("The spin penalty is on the right"), HeaderText(Window.ToSharedRef(), TEXT("HoleMapSpinCut")), OpenGolfPlay::SpinCutLabel(Cut));
                RenderedPlay::SaveWindowPng(Test, Window.ToSharedRef(), TEXT("Minimap-course-rough.png"));
                Mode->SetPlayMode(EOpenGolfPlayMode::Range);
                World->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>()->SetClub(PlayersClub);
                return true;
            }
            default:
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
        FString AddressLie;
        bool bCheckedFlightLie = false;
        ELaunchMonitorClub PlayersClub = ELaunchMonitorClub::Driver;
    };
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoleMapClickTest, "UOpenGolf.Minimap.RenderedClick",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FHoleMapClickTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(TestMaps::Range, false, true))
        return false;
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<RenderedPlay::FStartInWindow>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FClickHoleMap>(*this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
