// SPDX-License-Identifier: MIT
// Rendered: real clicks through Slate's hit test on the play buttons and the wind slide-out, with a capture to review.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "OpenGolfWindIndicator.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/RenderedPlay.h"
#include "UI/HudSlider.h"
#include "UI/WindIndicator.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"

class FClickWindControls : public IAutomationLatentCommand
{
public:
    explicit FClickWindControls(FAutomationTestBase* InTest) : Test(InTest) {}

    virtual bool Update() override
    {
        using namespace RenderedPlay;
        if (GetCurrentRunTime() > 90.0)
        {
            Test->AddError(FString::Printf(TEXT("Timed out at stage %d."), Stage));
            FScreenshotRequest::Reset();
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        auto* Controller = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
        const TSharedPtr<SWindow> Window = World && World->GetGameViewport() ? World->GetGameViewport()->GetWindow() : nullptr;
        if (!Mode || !Mode->GetBall() || !Mode->GetRange() || !Controller || !Controller->GetHUD() || !Window.IsValid())
            return false;
        // Each step waits for the HUD to be drawn, and so hit-testable, as the last click left it.
        if (GetCurrentRunTime() < ReadyAt)
            return false;
        const TSharedRef<SWindow> Root = Window.ToSharedRef();
        auto Find = [&Root](const TCHAR* Layer, const TCHAR* Type) -> TSharedPtr<SWidget>
        {
            const TSharedPtr<SWidget> Found = FindWidget(Root, Layer);
            return Found ? FindWidget(Found.ToSharedRef(), Type) : nullptr;
        };
        // Whether a click at At reaches Widget: the hit test puts it, or a widget inside it, at the front.
        auto Reaches = [&Root](const FVector2D& At, const TSharedPtr<SWidget>& Widget)
        {
            const FWidgetPath Path = PathAt(Root, At);
            return Widget.IsValid() && Path.IsValid() && Path.ContainsWidget(Widget.Get());
        };

        switch (Stage)
        {
        case 0:
            ReadyAt = GetCurrentRunTime() + 2.0;
            ++Stage;
            return false;
        case 1:
        {
            // The flag button sits under the test-shot layer, which once caught every click over it.
            const TSharedPtr<SWidget> Flag = Find(TEXT("SPlayModeBar"), TEXT("SHudIconButton"));
            if (!Test->TestTrue(TEXT("The flag button is drawn"), Flag.IsValid()))
                return true;
            const FVector2D At = PointOn(*Flag);
            Test->TestTrue(TEXT("A click on the flag button reaches it"), Reaches(At, Flag));
            Click(Root, At);
            Test->TestEqual(TEXT("Clicking the flag button plays the hole"), Mode->GetPlayMode(), EOpenGolfPlayMode::Hole);
            break;
        }
        case 2:
        {
            const TSharedPtr<SWidget> Control = FindWidget(Root, TEXT("SWindControl"));
            TSharedPtr<SWidget> Indicator;
            for (TObjectIterator<UOpenGolfWindIndicator> It; It; ++It)
                if (It->GetWorld() == World && It->GetCachedWidget().IsValid())
                    Indicator = It->GetCachedWidget();
            if (!Test->TestTrue(TEXT("The wind indicator and its controls are drawn"), Control.IsValid() && Indicator.IsValid()))
                return true;
            // The indicator's speed panel, left of its arrow.
            const FVector2D At = PointOn(*Indicator, FVector2D(0.3, 0.4));
            Test->TestTrue(TEXT("A click on the wind indicator reaches its controls"), Reaches(At, Control));
            Click(Root, At);
            Test->TestTrue(TEXT("Clicking the wind indicator slides out its controls"), Controller->IsWindPanelOpen());
            break;
        }
        case 3:
        {
            const TSharedPtr<SWidget> Switch = Find(TEXT("SWindControl"), TEXT("SHudSwitch"));
            if (!Test->TestTrue(TEXT("The wind switch is drawn"), Switch.IsValid()))
                return true;
            Test->TestTrue(TEXT("A click on the wind switch reaches it"), Reaches(PointOn(*Switch), Switch));
            const bool bWasOn = Controller->GetWind().bOn;
            Click(Root, PointOn(*Switch));
            Test->TestEqual(TEXT("Clicking the switch turns the wind on or off"), Controller->GetWind().bOn, !bWasOn);
            if (!Controller->GetWind().bOn)
                Click(Root, PointOn(*Switch));
            break;
        }
        case 4:
        {
            const TSharedPtr<SWidget> Slider = Find(TEXT("SWindControl"), TEXT("SHudSlider"));
            if (!Test->TestTrue(TEXT("The speed slider is drawn"), Slider.IsValid()))
                return true;
            const FGeometry& Geometry = Slider->GetTickSpaceGeometry();
            const float Inset = SHudSlider::ThumbSize * 0.5f / Geometry.GetLocalSize().X;
            auto Along = [Inset](float Alpha) { return FVector2D(Inset + (1.0f - 2.0f * Inset) * Alpha, 0.5); };
            Test->TestTrue(TEXT("A press on the speed slider reaches it"), Reaches(PointOn(*Slider, Along(0.2f)), Slider));
            Drag(Root, PointOn(*Slider, Along(0.2f)), PointOn(*Slider, Along(0.7f)));
            Test->TestEqual(TEXT("Sliding sets the wind speed"), Controller->GetWind().SpeedMph, 21.0f);
            Test->TestEqual(TEXT("The slid speed reaches the range"), Mode->GetRange()->WindSpeedMph, 21.0f);
            ReadyAt = GetCurrentRunTime() + 1.0;
            ++Stage;
            return false;
        }
        case 5:
            Filename = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
                FString::Printf(TEXT("Screenshots/WindControls/%s-wind-open.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
            FScreenshotRequest::RequestScreenshot(Filename, true, false, false, FIntRect(), true);
            ++Stage;
            return false;
        default:
            if (!IFileManager::Get().FileExists(*Filename))
                return false;
            Test->AddInfo(FString::Printf(TEXT("Wind controls capture: %s"), *Filename));
            return true;
        }
        ReadyAt = GetCurrentRunTime() + 0.6;
        ++Stage;
        return false;
    }

private:
    FAutomationTestBase* Test;
    int32 Stage = 0;
    double ReadyAt = 0.0;
    FString Filename;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWindControlsVisualTest, "UOpenGolf.Visual.WindControls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FWindControlsVisualTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<RenderedPlay::FStartInWindow>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FClickWindControls>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
