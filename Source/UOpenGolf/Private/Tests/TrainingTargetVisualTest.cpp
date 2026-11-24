// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraActor.h"
#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Misc/FileHelper.h"
#include "OpenFairwayBall.h"
#include "OpenGolfTerrain.h"
#include "ProceduralMeshComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/RenderedPlay.h"
#include "Training/DistancePractice.h"
#include "Training/TargetRings.h"
#include "Training/TrainingScoring.h"
#include "UOpenGolfBall.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"

namespace
{
    class FTrainingTargetView : public IAutomationLatentCommand
    {
    public:
        explicit FTrainingTargetView(FAutomationTestBase& InTest) : Test(InTest) {}
        bool Update() override
        {
            if (GetCurrentRunTime() > 240)
            {
                Test.AddError(FString::Printf(TEXT("Timed out capturing the drill at step %d"), Step));
                return true;
            }
            UWorld* World = GEditor->PlayWorld;
            auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
            auto* Player = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
            if (!Mode || !Player || !Mode->GetRange() || !Mode->GetBall() || !World->GetGameViewport())
                return false;
            auto Window = World->GetGameViewport()->GetWindow();
            if (!Window || GetCurrentRunTime() < WaitUntil)
                return false;
            auto* Range = Mode->GetRange();
            auto* Ball = Mode->GetBall();
            auto* Session = Mode->GetTrainingSession();
            const FVector Pin = Mode->GetCupLocation();
            switch (Step)
            {
            case 0:
                if (!Test.TestTrue(TEXT("The drill starts during map loading"), Session && Session->StartDrill(UDistancePractice::StaticClass(), 3, 6)))
                    return true;
                Range->InvalidateHoleMap();
                break;
            case 1:
                if (!Range->GetHoleMapTexture())
                    return false;
                {
                    const auto Frame = FHoleMapFrame::FromHole(Range->GetTeeLocation(), Range->GetOriginalCup().Position, Range->MapEndMargin,
                        Range->MapMinimumWidth, Range->MapSideOffset);
                    Test.TestTrue(TEXT("Map captures the full authored hole during the drill"), Range->GetHoleMapFrame().Equals(Frame));
                    OriginalPicture = Range->GetHoleMapTexture();
                    FOpenFairwayLaunch Chip;
                    Chip.BallSpeedMph = 27;
                    Chip.VerticalLaunchDeg = 30;
                    if (!Test.TestTrue(TEXT("Chip from the tee"), Ball->Launch(Chip)))
                        return true;
                }
                break;
            case 2:
                if (Ball->IsMoving())
                    return false;
                {
                    // A new run brings the pin to the ball; the ball stays where it stopped.
                    const int32 Yards = FMath::Clamp(FMath::RoundToInt(FVector::Dist2D(Range->GetTeeLocation(), Ball->GetActorLocation()) / 91.44),
                        UDistancePractice::MinYards, UDistancePractice::MaxYards);
                    Session->ExitDrill();
                    if (!Test.TestTrue(TEXT("Start a new run"), Session->StartDrill(UDistancePractice::StaticClass(), 3, 6)))
                        return true;
                    Test.TestTrue(TEXT("Pin at the ball"), CastChecked<UDistancePractice>(Session->Active())->SetDistance(Yards));
                    Test.TestEqual(TEXT("Moving the flag reuses the overhead picture"), Range->GetHoleMapTexture(), OriginalPicture.Get());
                }
                break;
            case 3:
                if (!Mode->IsBallAtAddress())
                    return false;
                Test.TestEqual(TEXT("The new run keeps the finished ball"), Session->FinishedBallCount(), 1);
                WaitUntil = GetCurrentRunTime() + 2.5;
                break;
            case 4:
                if (!CaptureTee(*Mode, *Player, Window.ToSharedRef(), TEXT("tee")))
                    return false;
                OriginalView = Player->GetViewTarget();
                Camera = World->SpawnActor<ACameraActor>();
                Camera->SetActorLocationAndRotation(Pin + FVector(-1260, -325, 740), FVector(1260, 325, -740).Rotation());
                Player->SetViewTarget(Camera);
                WaitUntil = GetCurrentRunTime() + 1;
                break;
            case 5:
                if (!Capture(Window.ToSharedRef(), TEXT("target")))
                    return false;
                break;
            case 6:
                Camera->AddActorWorldOffset(FVector(0, 3, 0));
                if (!Capture(Window.ToSharedRef(), TEXT("pan")))
                    return false;
                Player->SetViewTarget(OriginalView.Get());
                Camera->Destroy();
                // From the tee the near rings lie inside the grass draw distance; they must show over the blades.
                if (!MoveTarget(*Session, 15))
                    return true;
                break;
            case 7:
                if (!CaptureTee(*Mode, *Player, Window.ToSharedRef(), TEXT("tee-15yd")))
                    return false;
                if (!MoveTarget(*Session, 50))
                    return true;
                break;
            case 8:
                if (!CaptureTee(*Mode, *Player, Window.ToSharedRef(), TEXT("tee-50yd")))
                    return false;
                if (!MoveTarget(*Session, 150))
                    return true;
                break;
            case 9:
                if (!CaptureTee(*Mode, *Player, Window.ToSharedRef(), TEXT("tee-150yd")))
                    return false;
                Session->ExitDrill();
                return true;
            }
            ++Step;
            return false;
        }

    private:
        static constexpr int32 FramesPerSequence = 6;

        bool Capture(const TSharedRef<SWindow>& Window, const FString& View)
        {
            RenderedPlay::SaveWindowPng(Test, Window, FString::Printf(TEXT("TrainingGames/%s-%02d.png"), *View, FrameIndex));
            WaitUntil = GetCurrentRunTime() + .08;
            if (++FrameIndex < FramesPerSequence)
                return false;
            FrameIndex = 0;
            return true;
        }

        bool MoveTarget(ATrainingSession& Session, int32 Yards)
        {
            const bool bMoved = Test.TestTrue(FString::Printf(TEXT("Pin at %d yd"), Yards),
                CastChecked<UDistancePractice>(Session.Active())->SetDistance(Yards));
            WaitUntil = GetCurrentRunTime() + 1.5;
            return bMoved;
        }

        /**
         * A still tee view twice, rings shown then hidden, so ring shimmer can be told apart from the grass, plus the
         * selected ring's screen box and far arc for Assets/Scripts/ring_stability.py.
         */
        bool CaptureTee(const AUOpenGolfGameMode& Mode, APlayerController& Player, const TSharedRef<SWindow>& Window, const TCHAR* View)
        {
            const auto* Drill = Cast<UTargetDrill>(Mode.GetTrainingSession()->Active());
            UProceduralMeshComponent* Mesh = Drill && Drill->GetRings() ? Drill->GetRings()->GetMesh() : nullptr;
            if (!Test.TestNotNull(TEXT("Target rings"), Mesh))
                return true;
            if (!bHiddenPhase)
            {
                if (!Capture(Window, View))
                    return false;
                WriteRingSidecar(Mode, Player, View);
                Mesh->SetVisibility(false);
                bHiddenPhase = true;
                // Let TSR settle on the view without the rings.
                WaitUntil = GetCurrentRunTime() + 1;
                return false;
            }
            if (!Capture(Window, FString(View) + TEXT("-hidden")))
                return false;
            Mesh->SetVisibility(true);
            bHiddenPhase = false;
            return true;
        }

        void WriteRingSidecar(const AUOpenGolfGameMode& Mode, APlayerController& Player, const TCHAR* View)
        {
            const AUOpenGolfRange* Range = Mode.GetRange();
            FVector2D ViewportSize;
            Player.GetWorld()->GetGameViewport()->GetViewportSize(ViewportSize);
            const FVector Pin = Mode.GetCupLocation();
            const FVector Eye = Player.PlayerCameraManager->GetCameraLocation();
            const FVector Away = (Pin - Eye).GetSafeNormal2D();
            const auto* Drill = CastChecked<UTargetDrill>(Mode.GetTrainingSession()->Active());
            const double RadiusCm = Drill->GetRadiusCm();
            const double Edge = UTargetRings::EdgeWidthCm(Drill->GetPinYards() * TrainingScoring::CmPerYard);
            // A point on the ring at Radius, Degrees around from the far side, as a viewport fraction.
            auto Project = [&](double Radius, double Degrees, FVector2D& Fraction)
            {
                const FVector Flat = Pin + Away.RotateAngleAxis(Degrees, FVector::UpVector) * Radius;
                FVector Ground, Normal;
                if (!Range->Terrain || !Range->Terrain->GroundAt(Flat, Ground, Normal, AUOpenGolfBall::ContactChannel))
                    return false;
                FVector2D Screen;
                if (!Player.ProjectWorldLocationToScreen(Ground + Normal * UTargetRings::TurfClearanceCm, Screen, true))
                    return false;
                Fraction = Screen / ViewportSize;
                return true;
            };
            FBox2D Box(ForceInit);
            for (int32 Degrees = 0; Degrees < 360; Degrees += 10)
            {
                FVector2D Point;
                if (Project(RadiusCm + Edge, Degrees, Point))
                    Box += Point;
            }
            const FVector2D Margin(.01, .01);
            const FVector2D Min = (Box.Min - Margin).ComponentMax(FVector2D::ZeroVector), Max = (Box.Max + Margin).ComponentMin(FVector2D::UnitVector);
            TArray<FString> Arc;
            // The top of the far arc, where it runs nearly level on screen and breaks into dashes when sub-pixel.
            for (int32 Degrees = -60; Degrees <= 60; Degrees += 5)
            {
                FVector2D Point;
                if (Project(RadiusCm + Edge * .5, Degrees, Point))
                    Arc.Add(FString::Printf(TEXT("[%.5f,%.5f]"), Point.X, Point.Y));
            }
            const FString Json = FString::Printf(TEXT("{\"view\":\"%s\",\"box\":[%.5f,%.5f,%.5f,%.5f],\"far_arc\":[%s]}\n"), View, Min.X,
                Min.Y, Max.X, Max.Y, *FString::Join(Arc, TEXT(",")));
            const FString Path = FPaths::ProjectSavedDir() / TEXT("Screenshots/TrainingGames") / FString(View) + TEXT("-ring.json");
            Test.TestTrue(TEXT("Ring sidecar saved"), Box.bIsValid && Arc.Num() > 1 && FFileHelper::SaveStringToFile(Json, *Path));
        }

        FAutomationTestBase& Test;
        int32 Step = 0, FrameIndex = 0;
        bool bHiddenPhase = false;
        double WaitUntil = 0;
        TWeakObjectPtr<UTexture2D> OriginalPicture;
        TWeakObjectPtr<AActor> OriginalView;
        ACameraActor* Camera = nullptr;
    };
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingTargetVisualTest, "UOpenGolf.Visual.TrainingTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FTrainingTargetVisualTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
        return false;
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<RenderedPlay::FStartInWindow>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FTrainingTargetView>(*this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
