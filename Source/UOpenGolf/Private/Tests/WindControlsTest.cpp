// SPDX-License-Identifier: MIT
// The wind slide-out during Play: the HUD's wind setting reaching the range and ball, wind from the console reaching the
// HUD, Escape, and which widgets take clicks in each viewport layer.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "Play/OpenGolfWindSetting.h"
#include "Tests/AutomationEditorCommon.h"
#include "Types/SlateAttributeMetaData.h"
#include "UI/PlayModeBar.h"
#include "UI/ReplayShotPanel.h"
#include "UI/WindControl.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"

namespace WindControlsTestDetail
{
    /**
     * The widgets a click lands on, front-most first in each branch: Slate gives a click to a widget whose own
     * visibility is hit-testable, so these are the only places a layer can catch one.
     */
    void CollectClickTargets(const TSharedRef<SWidget>& Widget, TArray<FString>& OutTypes)
    {
        FSlateAttributeMetaData::UpdateAllAttributes(*Widget, FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);
        const EVisibility Visibility = Widget->GetVisibility();
        if (!Visibility.IsVisible())
            return;
        if (Visibility.IsHitTestVisible())
        {
            OutTypes.Add(Widget->GetTypeAsString());
            return;
        }
        if (!Visibility.AreChildrenHitTestVisible())
            return;
        FChildren* Children = Widget->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
            CollectClickTargets(Children->GetChildAt(Index), OutTypes);
    }

    /** The widget types a click can land on, in paint order. */
    FString ClickTargets(const TSharedRef<SWidget>& Widget)
    {
        TArray<FString> Types;
        CollectClickTargets(Widget, Types);
        return FString::Join(Types, TEXT(", "));
    }

    IConsoleVariable* WindVar(const TCHAR* Name)
    {
        return IConsoleManager::Get().FindConsoleVariable(Name);
    }
} // namespace WindControlsTestDetail

class FVerifyWindControls : public IAutomationLatentCommand
{
public:
    explicit FVerifyWindControls(FAutomationTestBase* InTest) : Test(InTest) {}

    virtual bool Update() override
    {
        using namespace WindControlsTestDetail;
        if (GetCurrentRunTime() > 60.0)
        {
            Test->AddError(FString::Printf(TEXT("Timed out at stage %d."), Stage));
            RestoreConsole();
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        auto* Controller = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
        AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
        AUOpenGolfRange* Range = Mode ? Mode->GetRange() : nullptr;
        if (!Ball || !Range || !Controller || !Controller->GetHUD())
            return false;
        const OpenGolfPlay::FOpenGolfWindSetting& Wind = Controller->GetWind();

        switch (Stage)
        {
        case 0:
        {
            Test->TestEqual(TEXT("The HUD starts with the map's wind on or off"), Wind.bOn, Range->WindSpeedMph > 0.0f);
            Test->TestEqual(TEXT("The HUD starts with the map's direction"), Wind.FromDeg, Range->WindFromDeg);
            // The test-shot layer spans the viewport over the play controls: only its button may take clicks.
            Test->TestEqual(TEXT("The closed test-shot layer only takes clicks on its button"),
                ClickTargets(SNew(SReplayShotPanel).Controller(Controller)), FString(TEXT("SButton")));
            Test->TestEqual(TEXT("The range's play bar only takes clicks on the flag button"),
                ClickTargets(SNew(SPlayModeBar).Controller(Controller)), FString(TEXT("SHudIconButton")));
            const TSharedRef<SWindControl> Control = SNew(SWindControl).Controller(Controller);
            Test->TestEqual(TEXT("Closed, the wind controls only take clicks on the indicator"), ClickTargets(Control), FString(TEXT("SBorder")));

            Controller->ToggleWindPanel();
            Test->TestTrue(TEXT("The indicator's click (or W) opens the wind controls"), Controller->IsWindPanelOpen());
            Control->Tick(FGeometry(), 0.0, SWindControl::SlideSeconds);
            Test->TestEqual(TEXT("The panel slides all the way out"), Control->GetOpenness(), 1.0f);
            Test->TestEqual(TEXT("Open, the panel takes its own clicks too"), ClickTargets(Control), FString(TEXT("SBorder, SBorder")));

            const bool bMapCalm = !Wind.bOn;
            Controller->SetWindOn(true);
            if (bMapCalm)
                Test->TestEqual(TEXT("Switched on, the wind blows at the default speed"), Range->WindSpeedMph, OpenGolfPlay::DefaultWindSettingMph);
            Controller->SetWindSpeedMph(45.0f);
            Test->TestEqual(TEXT("The slider stops at 30 mph"), Range->WindSpeedMph, OpenGolfPlay::MaxWindSettingMph);
            Controller->SetWindFromDeg(93.0f);
            Test->TestEqual(TEXT("The direction reaches the range in 5° steps"), Range->WindFromDeg, 95.0f);
            ++Stage;
            return false;
        }
        case 1:
            Test->TestEqual(TEXT("The ball flies in the HUD's speed"), Ball->WindSpeedMph, OpenGolfPlay::MaxWindSettingMph);
            Test->TestEqual(TEXT("The ball flies in the HUD's direction"), Ball->WindFromDeg, 95.0f);
            Controller->SetWindOn(false);
            Test->TestEqual(TEXT("Off is calm on the range"), Range->WindSpeedMph, 0.0f);
            Test->TestEqual(TEXT("Off keeps the speed"), Wind.SpeedMph, OpenGolfPlay::MaxWindSettingMph);
            ++Stage;
            return false;
        case 2:
            Test->TestTrue(TEXT("Off calms the ball"), Ball->GetWindVelocityMps().IsNearlyZero());
            Controller->SetWindOn(true);
            Test->TestEqual(TEXT("On again blows at the kept speed"), Range->WindSpeedMph, OpenGolfPlay::MaxWindSettingMph);
            ConsoleSpeed = WindVar(TEXT("uopengolf.WindMph"))->GetFloat();
            ConsoleDirection = WindVar(TEXT("uopengolf.WindFromDeg"))->GetFloat();
            WindVar(TEXT("uopengolf.WindMph"))->Set(12.0f, ECVF_SetByConsole);
            WindVar(TEXT("uopengolf.WindFromDeg"))->Set(180.0f, ECVF_SetByConsole);
            bConsoleSet = true;
            ++Stage;
            return false;
        case 3:
            // The range takes the console wind on its tick and the HUD follows on the controller's.
            if (Wind.SpeedMph != 12.0f)
                return false;
            Test->TestTrue(TEXT("Console wind turns the HUD's wind on"), Wind.bOn);
            Test->TestEqual(TEXT("The HUD takes the console direction"), Wind.FromDeg, 180.0f);
            RestoreConsole();
            Controller->HandleEscape();
            Test->TestFalse(TEXT("Escape closes the wind controls"), Controller->IsWindPanelOpen());
            Test->TestNotNull(TEXT("Escape with the wind controls open keeps playing"), GEditor->PlayWorld.Get());
            {
                Controller->SetReplayPanelOpen(true);
                const TSharedRef<SWindControl> Control = SNew(SWindControl).Controller(Controller);
                FSlateAttributeMetaData::UpdateAllAttributes(*Control, FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);
                Test->TestEqual(TEXT("The wind controls hide with the test-shot panel"), Control->GetVisibility(), EVisibility::Collapsed);
                Controller->SetReplayPanelOpen(false);
            }
            return true;
        default:
            return true;
        }
    }

private:
    void RestoreConsole()
    {
        if (!bConsoleSet)
            return;
        WindControlsTestDetail::WindVar(TEXT("uopengolf.WindMph"))->Set(ConsoleSpeed, ECVF_SetByConsole);
        WindControlsTestDetail::WindVar(TEXT("uopengolf.WindFromDeg"))->Set(ConsoleDirection, ECVF_SetByConsole);
        bConsoleSet = false;
    }

    FAutomationTestBase* Test;
    int32 Stage = 0;
    float ConsoleSpeed = 0.0f;
    float ConsoleDirection = 0.0f;
    bool bConsoleSet = false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWindControlsTest, "UOpenGolf.Range.WindControls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWindControlsTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FVerifyWindControls>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
