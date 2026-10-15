// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

// Opt-in rendered verification: NonNullRHI excludes this test from headless runs.
// Images are review artifacts, not automatic pixel-comparison baselines.
class FCaptureLaunchDirections : public IAutomationLatentCommand
{
public:
    explicit FCaptureLaunchDirections(FAutomationTestBase* InTest)
        : Test(InTest) {}

    virtual bool Update() override
    {
        UWorld* World = GEditor->PlayWorld;
        if (GetCurrentRunTime() > 45.0)
        {
            Test->AddError(TEXT("Timed out capturing rendered launch-direction HUD."));
            FScreenshotRequest::Reset();
            return true;
        }
        if (!World || !World->HasBegunPlay())
        {
            return false;
        }
        if (Stage == 0 || Stage == 3)
        {
            UOpenLaunchMonitorSubsystem* Monitor = World->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>();
            // Synthetic frames: 75.7 mph, VLA +20.78 / -0.26, HLA -0.97 / +0.97.
            const FString Frame = Stage == 0 ? TEXT("110237380D1E089FFF6F11F2F700800080") : TEXT("110237380DE6FF61006F11F2F700800080");
            if (!Test->TestTrue(TEXT("Visual replay injected"), Monitor && Monitor->InjectReplayFrames(Frame, TEXT(""))))
            {
                return true;
            }
            Filename = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
                                                         FString::Printf(TEXT("Screenshots/LaunchDirections/%s-%s.png"),
                                                             *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")), Stage == 0 ? TEXT("left-up") : TEXT("right-down")));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
            ReadyAt = GetCurrentRunTime() + 2.0;
            ++Stage;
            return false;
        }
        if (Stage == 1 || Stage == 4)
        {
            if (GetCurrentRunTime() < ReadyAt)
            {
                return false;
            }
            // Show UI: the HUD is a Slate viewport widget.
            FScreenshotRequest::RequestScreenshot(Filename, true, false, false, FIntRect(), true);
            ++Stage;
            return false;
        }
        if (!IFileManager::Get().FileExists(*Filename))
        {
            return false;
        }
        Test->AddInfo(FString::Printf(TEXT("Rendered HUD capture: %s"), *Filename));
        ++Stage;
        return Stage == 6;
    }

private:
    FAutomationTestBase* Test;
    int32 Stage = 0;
    double ReadyAt = 0.0;
    FString Filename;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaunchDirectionVisualTest, "UOpenGolf.Visual.LaunchDirections",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FLaunchDirectionVisualTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCaptureLaunchDirections>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
