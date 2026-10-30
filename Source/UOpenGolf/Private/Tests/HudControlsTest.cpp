// SPDX-License-Identifier: MIT
// The HUD's slider and switch: where a click sets the slider, and the switch's click.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/HudSlider.h"
#include "UI/HudSwitch.h"

namespace HudControlsTestDetail
{
    FPointerEvent LeftClickAt(const FVector2D& Position)
    {
        return FPointerEvent(0, Position, Position, TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
    }
} // namespace HudControlsTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHudControlsTest, "UOpenGolf.Range.HudControls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHudControlsTest::RunTest(const FString& Parameters)
{
    using namespace HudControlsTestDetail;
    constexpr float Width = SHudSlider::Width;
    constexpr float Start = SHudSlider::ThumbSize * 0.5f;
    constexpr float Travel = Width - SHudSlider::ThumbSize;
    TestEqual(TEXT("The thumb's left stop is the minimum"), SHudSlider::ValueAt(Start, Width, 0, 30, 1), 0.0f);
    TestEqual(TEXT("The thumb's right stop is the maximum"), SHudSlider::ValueAt(Start + Travel, Width, 0, 30, 1), 30.0f);
    TestEqual(TEXT("Left of the track is the minimum"), SHudSlider::ValueAt(-50, Width, 0, 30, 1), 0.0f);
    TestEqual(TEXT("Right of the track is the maximum"), SHudSlider::ValueAt(500, Width, 0, 30, 1), 30.0f);
    TestEqual(TEXT("The middle is half way"), SHudSlider::ValueAt(Start + Travel * 0.5f, Width, 0, 30, 1), 15.0f);
    TestEqual(TEXT("Speed snaps to whole steps"), SHudSlider::ValueAt(Start + Travel * 0.51f, Width, 0, 30, 1), 15.0f);
    TestEqual(TEXT("Direction snaps to its step"), SHudSlider::ValueAt(Start + Travel * 0.27f, Width, 0, 355, 5), 95.0f);
    TestEqual(TEXT("No step slides smoothly"), SHudSlider::ValueAt(Start + Travel * 0.25f, Width, 0, 1, 0), 0.25f, 1e-4f);

    // A click on the slider sets the value under it.
    float Speed = 0.0f;
    const TSharedRef<SHudSlider> Slider = SNew(SHudSlider).MinValue(0).MaxValue(30).Step(1)
        .Value_Lambda([&Speed] { return Speed; }).OnValueChanged_Lambda([&Speed](float Value) { Speed = Value; });
    const FGeometry SliderGeometry = FGeometry::MakeRoot(FVector2D(Width, SHudSlider::Height), FSlateLayoutTransform());
    TestTrue(TEXT("The slider takes the click"), Slider->OnMouseButtonDown(SliderGeometry, LeftClickAt(FVector2D(Start + Travel * 0.4f, 10))).IsEventHandled());
    TestEqual(TEXT("The click sets the speed under it"), Speed, 12.0f);
    Slider->SetEnabled(false);
    TestFalse(TEXT("A disabled slider leaves the click"), Slider->OnMouseButtonDown(SliderGeometry, LeftClickAt(FVector2D(Start, 10))).IsEventHandled());
    TestEqual(TEXT("A disabled slider keeps its value"), Speed, 12.0f);

    // The switch toggles on a click released over it, not on one dragged off it.
    bool bOn = false;
    const TSharedRef<SHudSwitch> Switch = SNew(SHudSwitch).IsOn_Lambda([&bOn] { return bOn; }).OnToggled_Lambda([&bOn] { bOn = !bOn; });
    const FGeometry SwitchGeometry = FGeometry::MakeRoot(FVector2D(SHudSwitch::Width, SHudSwitch::Height), FSlateLayoutTransform());
    const FVector2D Inside(SHudSwitch::Width * 0.5f, SHudSwitch::Height * 0.5f);
    Switch->OnMouseButtonDown(SwitchGeometry, LeftClickAt(Inside));
    Switch->OnMouseButtonUp(SwitchGeometry, LeftClickAt(Inside));
    TestTrue(TEXT("A click turns the switch on"), bOn);
    Switch->OnMouseButtonDown(SwitchGeometry, LeftClickAt(Inside));
    Switch->OnMouseButtonUp(SwitchGeometry, LeftClickAt(FVector2D(200, 200)));
    TestTrue(TEXT("Releasing off the switch leaves it"), bOn);
    return true;
}

#endif
