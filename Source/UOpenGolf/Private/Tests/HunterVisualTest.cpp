// SPDX-License-Identifier: MIT
// Rendered review of Hunter Ranch: tee, approach and green views of holes 1, 9 and 12, each held still for
// 20 frames to show shimmer, with a slow sideways pan at each green. Frame times are recorded but are provisional:
// captures run off screen and save a file every frame. Also checks that building the hole's minimap picture never holds
// the game thread for long, since a hole change builds one while the camera moves to the next tee.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GolfCourseDefinition.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "OpenGolfTerrain.h"
#include "Serialization/BulkDataScopedLock.h"
#include "ShaderCompiler.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"
#include "UnrealClient.h"

namespace HunterVisualTestDetail
{
    const TCHAR* const CoursePath = TEXT("/HunterRanch/Course/DA_HunterRanch.DA_HunterRanch");
    constexpr int32 Holes[] = {1, 9, 12};
    constexpr int32 FramesPerView = 20;
    constexpr double PanCmPerFrame = 5;

    struct FView
    {
        FString Name;
        FVector Location, Target;
        bool bPan = false;
    };

    FVector OnGround(const UOpenGolfTerrainDefinition& D, FVector2D PointCm, double HeightCm)
    {
        return FVector(PointCm, D.HeightAt(PointCm / 100) * 100 + HeightCm);
    }

    /** Behind the back tee looking down the opening line, 110 m out from the cup, and beside the green. */
    TArray<FView> Views(const UGolfCourseDefinition& Course, const UOpenGolfTerrainDefinition& D)
    {
        TArray<FView> Result;
        for (const int32 Number : Holes)
        {
            const FGolfCourseHole& Hole = Course.Layout[Number - 1];
            const FVector2D Tee(Hole.Tees[0].PositionCm), Cup(Hole.CupCm);
            const FVector2D Aim(Hole.RoutingCm.IsEmpty() ? Hole.CupCm : Hole.RoutingCm[0]);
            const FVector2D Forward = (Aim - Tee).GetSafeNormal();
            Result.Add({FString::Printf(TEXT("Hole%02d-Tee"), Number), OnGround(D, Tee - Forward * 300, 170), OnGround(D, Aim, 0)});
            if (Number == 1)
            {
                const FVector2D AboveFairway = Tee + Forward * 8500;
                Result.Add({TEXT("Hole01-Flyover"), OnGround(D, AboveFairway, 1850), OnGround(D, Cup, 0)});
            }
            const FVector2D Last(Hole.RoutingCm.IsEmpty() ? Hole.Tees[0].PositionCm : Hole.RoutingCm.Last());
            const FVector2D Back = (Last - Cup).GetSafeNormal();
            Result.Add({FString::Printf(TEXT("Hole%02d-Approach"), Number), OnGround(D, Cup + Back * 11000, 200), OnGround(D, Cup, 0)});
            const FVector2D Side(-Back.Y, Back.X);
            Result.Add({FString::Printf(TEXT("Hole%02d-Green"), Number), OnGround(D, Cup + Back * 1200 + Side * 300, 160),
                OnGround(D, Cup, 0), true});
        }
        return Result;
    }

    class FCaptureHunter : public IAutomationLatentCommand
    {
    public:
        explicit FCaptureHunter(FAutomationTestBase* InTest) : Test(InTest)
        {
            FString Label = TEXT("Review");
            FParse::Value(FCommandLine::Get(), TEXT("HunterCaptureLabel="), Label);
            Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/HunterReview") /
                (FPaths::MakeValidFileName(Label) + TEXT("-") + FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"))));
            IFileManager::Get().MakeDirectory(*Directory, true);
        }

        virtual bool Update() override
        {
            if (GetCurrentRunTime() > 900)
            {
                Test->AddError(TEXT("Hunter Ranch capture timed out"));
                return true;
            }
            UWorld* World = GEditor->PlayWorld;
            auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
            if (!Mode || !Mode->GetRange() || !Mode->GetRange()->Terrain || (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()))
                return false;
            if (!Camera && !Start(*World, *Mode))
                return true;
            if (View == ViewList.Num())
                return CheckHoleMap(*Mode->GetRange());
            return Capture(*World);
        }

    private:
        /** The course's hole 1 map, built when the round started, is published and cost no long game-thread step. */
        bool CheckHoleMap(const AUOpenGolfRange& Range)
        {
            if (Range.IsHoleMapBuilding() || !Range.GetHoleMapTexture())
                return false;
            const FTexture2DMipMap& Mip = Range.GetHoleMapTexture()->GetPlatformData()->Mips[0];
            const TBulkDataScopedReadLock<uint8> Lock(Mip.BulkData);
            const auto* Pixels = reinterpret_cast<const FColor*>(Lock.GetData());
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Mip.SizeX, Mip.SizeY, {Pixels, int64(Mip.SizeX) * Mip.SizeY}, Png);
            Test->TestTrue(TEXT("Hole 1 map saved"), FFileHelper::SaveArrayToFile(Png, *(Directory / TEXT("Hole01-Map.png"))));
            const double Ms = Range.GetHoleMapGameThreadMs();
            Test->AddInfo(FString::Printf(TEXT("Hole map: longest game-thread step %.1f ms"), Ms));
            Test->TestTrue(TEXT("Building the hole map never holds the game thread for 100 ms"), Ms > 0 && Ms < 100);
            return true;
        }

        bool Start(UWorld& World, AUOpenGolfGameMode& Mode)
        {
            const auto* Course = LoadObject<UGolfCourseDefinition>(nullptr, CoursePath);
            if (!Test->TestNotNull(TEXT("Course loads"), Course) || Course->Layout.Num() < 12)
                return false;
            Mode.SetPlayMode(EOpenGolfPlayMode::Hole, Course);
            ViewList = Views(*Course, *Mode.GetRange()->Terrain->Definition);
            auto* PC = Cast<AUOpenGolfPlayerController>(World.GetFirstPlayerController());
            PC->SetReplayPanelOpen(false);
            Camera = World.SpawnActor<ACameraActor>();
            Camera->GetCameraComponent()->SetFieldOfView(65);
            Camera->GetCameraComponent()->bConstrainAspectRatio = false;
            PC->SetViewTarget(Camera);
            return true;
        }

        bool Capture(UWorld& World)
        {
            const FView& Current = ViewList[View];
            if (Stage == 0)
            {
                Camera->SetActorLocation(Current.Location);
                Camera->SetActorRotation((Current.Target - Current.Location).Rotation());
                // Settle streaming and lighting, then time 3 s of rendering before capturing.
                ReadyAt = GetCurrentRunTime() + 5;
                TimedFrames = 0;
                TimedSeconds = 0;
                Stage = 1;
                return false;
            }
            if (Stage == 1)
            {
                if (GetCurrentRunTime() < ReadyAt)
                    return false;
                if (GetCurrentRunTime() < ReadyAt + 3)
                {
                    TimedSeconds += World.GetDeltaSeconds();
                    ++TimedFrames;
                    return false;
                }
                Test->AddInfo(FString::Printf(TEXT("%s: %.1f ms per frame over %d frames (provisional, off screen)"), *Current.Name,
                    TimedFrames ? 1000 * TimedSeconds / TimedFrames : 0.0, TimedFrames));
                Stage = 2;
            }
            if (Stage == 3)
            {
                if (!IFileManager::Get().FileExists(*Filename))
                    return false;
                if (++Frame == FramesPerView)
                {
                    Test->AddInfo(FString::Printf(TEXT("%s: %d frames saved in %s"), *Current.Name, FramesPerView, *Directory));
                    Frame = 0;
                    ++View;
                    Stage = 0;
                    return false;
                }
                if (Current.bPan)
                    Camera->AddActorWorldOffset(Camera->GetActorRightVector() * PanCmPerFrame);
                ReadyAt = GetCurrentRunTime() + 0.1;
                Stage = 4;
                return false;
            }
            if (Stage == 4 && GetCurrentRunTime() < ReadyAt)
                return false;
            Filename = Directory / FString::Printf(TEXT("%s-Frame%02d.png"), *Current.Name, Frame);
            if (IFileManager::Get().FileExists(*Filename))
            {
                Test->AddError(TEXT("Capture file already exists; refusing stale evidence"));
                return true;
            }
            FScreenshotRequest::RequestScreenshot(Filename, false, false, false, FIntRect(), true);
            Stage = 3;
            return false;
        }

        FAutomationTestBase* Test;
        ACameraActor* Camera = nullptr;
        TArray<FView> ViewList;
        int32 View = 0, Frame = 0, Stage = 0, TimedFrames = 0;
        double ReadyAt = 0, TimedSeconds = 0;
        FString Directory, Filename;
    };
} // namespace HunterVisualTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHunterVisualTest, "UOpenGolf.Visual.HunterRanch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FHunterVisualTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/HunterRanch/Course/Course"), false, true))
        return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HunterVisualTestDetail::FCaptureHunter>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
