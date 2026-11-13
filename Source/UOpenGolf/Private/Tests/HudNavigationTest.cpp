// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/Common/HudIconButton.h"
#include "UI/Common/HudSlider.h"
#include "UI/Common/HudSwitch.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHudNavigationTest, "UOpenGolf.Menu.HudNavigation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHudNavigationTest::RunTest(const FString&)
{
    const auto Key=[](FKey K){return FKeyEvent(K,FModifierKeysState(),0,false,0,0);};
    int32 Clicks=0;
    auto Button=SNew(SHudIconButton).OnClicked_Lambda([&]{++Clicks;});
    auto Switch=SNew(SHudSwitch).OnToggled_Lambda([&]{++Clicks;});
    TestTrue(TEXT("HUD button accepts keyboard focus"),Button->SupportsKeyboardFocus());
    TestTrue(TEXT("HUD switch accepts keyboard focus"),Switch->SupportsKeyboardFocus());
    Button->OnKeyDown(FGeometry(),Key(EKeys::Enter));
    Switch->OnKeyDown(FGeometry(),Key(EKeys::Gamepad_FaceButton_Bottom));
    TestEqual(TEXT("Keyboard and controller activate once"),Clicks,2);
    Button->SetEnabled(false);
    Button->OnKeyDown(FGeometry(),Key(EKeys::Enter));
    TestEqual(TEXT("Disabled control cannot activate"),Clicks,2);
    float Value=10;
    auto Slider=SNew(SHudSlider).MinValue(0).MaxValue(20).Step(5)
        .Value_Lambda([&]{return Value;}).OnValueChanged_Lambda([&](float V){Value=V;});
    TestTrue(TEXT("Slider accepts focus"),Slider->SupportsKeyboardFocus());
    Slider->OnKeyDown(FGeometry(),Key(EKeys::Right));
    TestEqual(TEXT("Arrow advances one step"),Value,15.f);
    Slider->OnKeyDown(FGeometry(),Key(EKeys::Gamepad_DPad_Right));
    Slider->OnKeyDown(FGeometry(),Key(EKeys::Gamepad_DPad_Right));
    TestEqual(TEXT("Controller stops at maximum"),Value,20.f);
    Slider->OnKeyDown(FGeometry(),Key(EKeys::Left));
    TestEqual(TEXT("Arrow reverses direction"),Value,15.f);
    return true;
}
#endif
