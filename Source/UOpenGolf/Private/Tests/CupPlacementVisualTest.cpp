// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Hole/OpenGolfCupComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"
#include "UnrealClient.h"

class FCaptureCupPlacement : public IAutomationLatentCommand
{
public:
    explicit FCaptureCupPlacement(FAutomationTestBase* InTest) : Test(InTest) {}

    bool Update() override
    {
        if (GetCurrentRunTime() > 90)
        {
            Test->AddError(TEXT("Timed out capturing the cup."));
            FScreenshotRequest::Reset();
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        auto* Controller = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
        if (!Mode || !Controller || !Mode->GetRange())
            return false;
        auto* Cup = Mode->GetRange()->GetCup();
        // Hold the flag raised even while the gameplay ball is back at the tee.
        Cup->UpdatePinLift(Cup->GetCupLocation(), 1);
        if (!Camera.IsValid())
        {
            Camera = World->SpawnActor<ACameraActor>();
            Camera->GetCameraComponent()->SetFieldOfView(45);
            Controller->SetViewTarget(Camera.Get());
        }
        if (Stage % 3 == 0)
        {
            const FVector Offset = Stage == 0 ? FVector(-5, 0, 65) : FVector(-65, 0, 12);
            Camera->SetActorLocation(Cup->GetCupLocation() + Offset);
            Camera->SetActorRotation((-Offset).Rotation());
            ReadyAt = GetCurrentRunTime() + 4;
            ++Stage;
        }
        if (GetCurrentRunTime() < ReadyAt)
            return false;
        if (Stage % 3 == 1)
        {
            Filename = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
                FString::Printf(TEXT("Screenshots/CupPlacement/%s-%s.png"),
                    *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")), Stage == 1 ? TEXT("overhead") : TEXT("low")));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
            FScreenshotRequest::RequestScreenshot(Filename, false, false);
            ++Stage;
            return false;
        }
        if (!IFileManager::Get().FileExists(*Filename))
            return false;
        Test->AddInfo(FString::Printf(TEXT("Cup close-up: %s"), *Filename));
        return ++Stage == 6;
    }

private:
    FAutomationTestBase* Test;
    TWeakObjectPtr<ACameraActor> Camera;
    int32 Stage = 0;
    double ReadyAt = 0;
    FString Filename;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCupPlacementVisualTest, "UOpenGolf.Visual.CupPlacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FCupPlacementVisualTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCaptureCupPlacement>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
