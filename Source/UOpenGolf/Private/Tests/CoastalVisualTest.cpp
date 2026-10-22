// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
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
            auto* PC = Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController());
            PC->SetReplayPanelOpen(false);
            Camera = World->SpawnActor<ACameraActor>();
            Camera->GetCameraComponent()->SetFieldOfView(65);
            Camera->GetCameraComponent()->bConstrainAspectRatio = false;
            PC->SetViewTarget(Camera);
        }
        if (View == 10)
            return true;
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
            Camera->SetActorLocation(Locations[View]);
            Camera->SetActorRotation((Targets[View] - Locations[View]).Rotation());
            Grass->Set((View == 1 || View == 5 || View == 7) ? 0 : 1, ECVF_SetByConsole);
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
        FScreenshotRequest::RequestScreenshot(Filename, false, false, false, FIntRect(), true);
        Stage = 2;
        return false;
    }

private:
    FAutomationTestBase* Test;
    IConsoleVariable* Grass;
    int32 OriginalGrass;
    ACameraActor* Camera = nullptr;
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
