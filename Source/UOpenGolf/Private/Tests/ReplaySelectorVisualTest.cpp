// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfPlayerController.h"
#include "UnrealClient.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SWindow.h"

namespace
{
    TSharedPtr<SWidget> FindWidget(const TSharedRef<SWidget>& Root, const FString& Type)
    {
        if (Root->GetTypeAsString().Contains(Type))
        {
            return Root;
        }
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            if (TSharedPtr<SWidget> Found = FindWidget(Children->GetChildAt(Index), Type))
            {
                return Found;
            }
        }
        return nullptr;
    }
} // namespace

class FCaptureReplaySelector : public IAutomationLatentCommand
{
public:
    explicit FCaptureReplaySelector(FAutomationTestBase* InTest)
        : Test(InTest) {}

    virtual bool Update() override
    {
        UWorld* World = GEditor->PlayWorld;
        auto* Controller = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
        if (GetCurrentRunTime() > 45.0)
        {
            Test->AddError(TEXT("Timed out capturing replay selector."));
            FScreenshotRequest::Reset();
            return true;
        }
        if (!World || !World->HasBegunPlay() || !Controller)
        {
            return false;
        }
        if (Stage == 0)
        {
            Controller->SelectReplayShot(3);
            Controller->SetReplayPanelOpen(true);
            ReadyAt = GetCurrentRunTime() + 2.0;
            ++Stage;
        }
        if (GetCurrentRunTime() < ReadyAt)
        {
            return false;
        }
        if (Stage == 1 || Stage == 3 || Stage == 5)
        {
            Filename = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
                                                         FString::Printf(TEXT("Screenshots/ReplaySelector/%s-%d.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")), Stage));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
            FScreenshotRequest::RequestScreenshot(Filename, true, false, false, FIntRect(), true);
            ++Stage;
            return false;
        }
        if (!IFileManager::Get().FileExists(*Filename))
        {
            return false;
        }
        Test->AddInfo(FString::Printf(TEXT("Replay selector capture: %s"), *Filename));
        if (Stage == 2)
        {
            for (const TSharedRef<SWindow>& Window : FSlateApplication::Get().GetTopLevelWindows())
            {
                if (TSharedPtr<SWidget> Panel = FindWidget(Window, TEXT("SReplayShotPanel")))
                {
                    if (TSharedPtr<SWidget> Combo = FindWidget(Panel.ToSharedRef(), TEXT("SComboBox")))
                    {
                        ShotList = StaticCastSharedPtr<SComboBox<TSharedPtr<FString>>>(Combo);
                        ShotList->SetIsOpen(true);
                        break;
                    }
                }
            }
            Test->TestTrue(TEXT("Dropdown opened"), ShotList.IsValid() && ShotList->IsOpen());
        }
        if (Stage == 4)
        {
            if (ShotList.IsValid())
                ShotList->SetIsOpen(false);
            Controller->SelectReplayShot(0);
            Controller->InjectReplayShot();
        }
        if (Stage == 6)
        {
            return true;
        }
        ++Stage;
        ReadyAt = GetCurrentRunTime() + (Stage == 5 ? 4.0 : 2.0);
        return false;
    }

private:
    FAutomationTestBase* Test;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> ShotList;
    int32 Stage = 0;
    double ReadyAt = 0.0;
    FString Filename;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReplaySelectorVisualTest, "UOpenGolf.Visual.ReplaySelector",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FReplaySelectorVisualTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCaptureReplaySelector>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
