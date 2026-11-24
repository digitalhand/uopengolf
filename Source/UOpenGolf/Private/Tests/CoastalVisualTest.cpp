// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraActor.h"
#include "OpenGolfEnvironment.h"
#include "Camera/CameraComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "RangeTestMap.h"
#include "OpenGolfTerrain.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ShaderCompiler.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UnrealClient.h"

class FCaptureCoastal : public IAutomationLatentCommand
{
public:
    explicit FCaptureCoastal(FAutomationTestBase* InTest) : Test(InTest)
    {
        Grass = IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.Grass3D"));
        OriginalGrass = Grass->GetInt();
        FString Label = TEXT("Review");
        FParse::Value(FCommandLine::Get(), TEXT("CoastalCaptureLabel="), Label);
        if (const auto* Override = IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.CoastalCaptureLabel")); Override && !Override->GetString().IsEmpty())
            Label = Override->GetString();
        Label = FPaths::MakeValidFileName(Label);
        Label += TEXT("-") + FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));
        FParse::Value(FCommandLine::Get(), TEXT("CoastalViewCount="), ViewCount);
        ViewCount = FMath::Clamp(ViewCount, 1, 10);
        bReviewClouds = FParse::Param(FCommandLine::Get(), TEXT("EnvironmentCloudReview"));
        if (bReviewClouds)
            ViewCount = 6;
        bCsvProfile = FParse::Param(FCommandLine::Get(), TEXT("CoastalCsvProfile"));
        Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/CoastalReview") / Label);
        IFileManager::Get().MakeDirectory(*Directory, true);
    }
    ~FCaptureCoastal() { Grass->Set(OriginalGrass, ECVF_SetByConsole); }
    bool Update() override
    {
        if (GetCurrentRunTime() > 360)
        {
            Test->AddError(TEXT("Coastal capture timed out"));
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        if (!World || !World->HasBegunPlay() || !World->GetFirstPlayerController() ||
            (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()))
            return false;
        if (!Camera)
        {
            if (auto* Environment = AOpenGolfEnvironment::Find(World))
            {
                auto Settings = Environment->GetSettings();
                FParse::Value(FCommandLine::Get(), TEXT("EnvironmentDaylight="), Settings.Daylight);
                FParse::Value(FCommandLine::Get(), TEXT("EnvironmentCloudCoverage="), Settings.CloudCoverage);
                FParse::Value(FCommandLine::Get(), TEXT("EnvironmentCloudSpeed="), Settings.CloudSpeed);
                Settings.bAutomaticTime = FParse::Param(FCommandLine::Get(), TEXT("EnvironmentAdvance"));
                if (Settings.bAutomaticTime) Settings.DurationMinutes = 1;
                Environment->ApplySettings(Settings);
                // Cloud ray-march density for A/B shimmer reviews; the report records the scale in use.
                if (auto* Clouds = Environment->FindComponentByClass<UVolumetricCloudComponent>())
                {
                    float SampleScale = Clouds->ViewSampleCountScale;
                    if (FParse::Value(FCommandLine::Get(), TEXT("EnvironmentCloudSampleScale="), SampleScale))
                        Clouds->SetViewSampleCountScale(FMath::Clamp(SampleScale, .05f, 8.f));
                    Test->AddInfo(FString::Printf(TEXT("Cloud view sample count scale: %.2f"), Clouds->ViewSampleCountScale));
                }
            }
            else if (bReviewClouds)
            {
                Test->AddError(TEXT("The cloud review requires a course environment."));
                return true;
            }
            auto* PC = Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController());
            PC->SetReplayPanelOpen(false);
            Camera = World->SpawnActor<ACameraActor>();
            Camera->GetCameraComponent()->SetFieldOfView(65);
            Camera->GetCameraComponent()->bConstrainAspectRatio = false;
            PC->SetViewTarget(Camera);
#if CSV_PROFILER
            // -CoastalCsvProfile -csvGpuStats records per-pass GPU timings, with an event per view.
            if (bCsvProfile && !FCsvProfiler::IsCapturing())
                FCsvProfiler::Get()->BeginCapture();
#endif
        }
        if (View == ViewCount)
            return FinishCsvProfile();
        if (Stage == 0)
        {
            FVector Locations[] = {{-600, 0, 220}, {-600, 0, 220}, {9000, -11000, 8500}, {12100, -1700, 650}, {5700, -4200, 380}, {5700, -4200, 380}, {1600, -1600, 100}, {1600, -1600, 100}, {13300, 3900, 500}, {1600, -1600, 100}};
            FVector Targets[] = {{10000, 0, 150}, {10000, 0, 150}, {16000, 0, 0}, {13900, 400, 70}, {6200, -4150, 320}, {6200, -4150, 320}, {4000, -1500, 80}, {4000, -1500, 80}, {14600, 2800, 0}, {4000, -1500, 80}};
            if (View == 8)
                for (TActorIterator<AOpenGolfTerrainArea> It(World); It; ++It)
                {
                    FVector2D Bunker;
                    if (It->Definition && FindRangeBunkerCenter(*It->Definition, Bunker))
                    {
                        const FVector Local(Bunker.X * 100, Bunker.Y * 100, It->Definition->HeightAt(Bunker) * 100);
                        Targets[View] = It->GetActorTransform().TransformPosition(Local);
                        Locations[View] = It->GetActorTransform().TransformPosition(Local + FVector(-1300, 1100, 500));
                        break;
                    }
                }
            if (bReviewClouds)
            {
                // Day at the tee, across the range and overhead, then morning, sunset and clear sky at the tee.
                Locations[View] = Locations[0];
                Targets[View] = Targets[0];
                if (View == 1)
                    Targets[View] = Locations[0] + FVector(0, 10000, 1800);
                if (View == 2)
                    Targets[View] = Locations[0] + FVector(4500, 0, 10000);
                if (auto* Environment = AOpenGolfEnvironment::Find(World))
                {
                    auto Settings = Environment->GetSettings();
                    Settings.CloudSpeed = 0;
                    Settings.Daylight = View == 3 ? 0.f : (View == 4 ? 1.f : .5f);
                    Settings.CloudCoverage = View == 5 ? 0.f : .35f;
                    Environment->ApplySettings(Settings);
                }
            }
            Camera->SetActorLocation(Locations[View]);
            Camera->SetActorRotation((Targets[View] - Locations[View]).Rotation());
            Grass->Set(!bReviewClouds && (View == 1 || View == 5 || View == 7) ? 0 : 1, ECVF_SetByConsole);
            ReadyAt = GetCurrentRunTime() + 5;
            Stage = 1;
        }
        if (GetCurrentRunTime() < ReadyAt)
            return false;
        if (Stage == 2)
        {
            if (!IFileManager::Get().FileExists(*Filename))
                return false;
            if (++Frame == 20)
            {
                CSV_EVENT_GLOBAL(TEXT("CoastalViewEnd%d"), View);
                Test->AddInfo(FString::Printf(TEXT("View %d: 20 frames saved in %s"), View, *Directory));
                Frame = 0;
                ++View;
                Stage = 0;
                return false;
            }
            if (View == 9)
                Camera->AddActorWorldOffset(FVector(0, 5, 0));
            ReadyAt = GetCurrentRunTime() + 0.1;
            Stage = 1;
            return false;
        }
        Filename = Directory / FString::Printf(TEXT("Coastal-View%d-Frame%02d.png"), View, Frame);
        if (IFileManager::Get().FileExists(*Filename))
        {
            Test->AddError(TEXT("Capture file already exists; refusing stale evidence"));
            return true;
        }
        if (Frame == 0)
            CSV_EVENT_GLOBAL(TEXT("CoastalView%d"), View);
        FScreenshotRequest::RequestScreenshot(Filename, false, false, false, FIntRect(), true);
        Stage = 2;
        return false;
    }

private:
    // Ends this test's CSV capture and waits for the file, so it has its full header row.
    bool FinishCsvProfile()
    {
#if CSV_PROFILER
        if (!bCsvProfile)
            return true;
        if (!CsvFile.IsValid())
        {
            if (!FCsvProfiler::IsCapturing())
            {
                Test->AddError(TEXT("The CSV profile was not capturing at the end of the views."));
                return true;
            }
            CsvFile = FCsvProfiler::Get()->EndCapture();
        }
        if (!CsvFile.IsReady())
            return false;
        Test->AddInfo(FString::Printf(TEXT("CSV profile: %s"), *CsvFile.Get()));
#endif
        return true;
    }
    FAutomationTestBase* Test;
    IConsoleVariable* Grass;
    int32 OriginalGrass;
    ACameraActor* Camera = nullptr;
    int32 ViewCount = 10;
    bool bReviewClouds = false;
    bool bCsvProfile = false;
#if CSV_PROFILER
    TSharedFuture<FString> CsvFile;
#endif
    int32 View = 0, Frame = 0, Stage = 0;
    double ReadyAt = 0;
    FString Directory, Filename;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalVisualTest, "UOpenGolf.Visual.CoastalTerrain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FCoastalVisualTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(GetRangeTestMap(), false, true))
        return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCaptureCoastal>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
