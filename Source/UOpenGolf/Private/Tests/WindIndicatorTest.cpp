// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Misc/AutomationTest.h"
#include "OpenGolfWindIndicator.h"
#include "WindDisplay.h"
#include "WindIndicator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWindDisplayTest, "UOpenGolf.Range.WindIndicator.Direction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWindDisplayTest::RunTest(const FString& Parameters)
{
    const auto Check = [this](const TCHAR* Label, float From, float Yaw, FVector2D Expected)
    {
        const auto State = WindDisplay::FromSettings(10, From, Yaw);
        TestTrue(Label, State.Direction.Equals(Expected, 1e-5));
        TestEqual(TEXT("Speed stays in mph"), State.SpeedMph, 10.0f, 1e-4f);
    };
    Check(TEXT("Headwind points down"), 0, 0, FVector2D(0, 1));
    Check(TEXT("Wind from right points left"), 90, 0, FVector2D(-1, 0));
    Check(TEXT("Tailwind points up"), 180, 0, FVector2D(0, -1));
    Check(TEXT("Wind from left points right"), 270, 0, FVector2D(1, 0));
    Check(TEXT("Diagonal headwind"), 45, 0, FVector2D(-1, 1).GetSafeNormal());
    Check(TEXT("Camera facing right turns right wind into headwind"), 90, 90, FVector2D(0, 1));
    Check(TEXT("Camera yaw wraps"), 90, 450, FVector2D(0, 1));
    Check(TEXT("Wind direction wraps"), -90, 0, FVector2D(1, 0));
    const auto Calm = WindDisplay::FromSettings(0, 90, 45);
    TestEqual(TEXT("Calm label"), Calm.SpeedLabel(), FString(TEXT("0 MPH")));
    TestEqual(TEXT("Calm has a stable downward arrow"), Calm.Direction, FVector2D(0, 1));
    TestEqual(TEXT("Speed rounds down"), WindDisplay::FromSettings(10.4f, 0, 0).SpeedLabel(), FString(TEXT("10 MPH")));
    TestEqual(TEXT("Speed rounds up"), WindDisplay::FromSettings(10.6f, 0, 0).SpeedLabel(), FString(TEXT("11 MPH")));
    TestEqual(TEXT("Negative speed is calm"), WindDisplay::FromSettings(-5, 0, 0).SpeedMph, 0.0f);
    TestEqual(TEXT("No ball or range is calm"), WindDisplay::Read(nullptr, nullptr, 0).SpeedMph, 0.0f);
    const auto Velocity = WindDisplay::FromVelocity(FVector(0, -4.4704, 0), 0);
    TestTrue(TEXT("World velocity matches settings"), Velocity.Direction.Equals(FVector2D(-1, 0)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWindDesignerTest, "UOpenGolf.Range.WindIndicator.Designer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWindDesignerTest::RunTest(const FString& Parameters)
{
    UOpenGolfWindIndicator* Indicator = NewObject<UOpenGolfWindIndicator>();
    Indicator->SetDesignerFlags(EWidgetDesignFlags::Designing);
    const TSharedRef<SWindIndicator> Slate = StaticCastSharedRef<SWindIndicator>(Indicator->TakeWidget());
    TestNull(TEXT("Designer requires no world"), Indicator->GetWorld());
    TestTrue(TEXT("Default preview points left"), Slate->GetWind().Direction.Equals(FVector2D(-1, 0), 1e-5));
    Indicator->WindSpeedMph = 20;
    Indicator->WindFromDeg = 180;
    Indicator->SynchronizeProperties();
    TestEqual(TEXT("Designer property edit updates text"), Slate->GetWind().SpeedLabel(), FString(TEXT("20 MPH")));
    TestTrue(TEXT("Designer property edit updates arrow"), Slate->GetWind().Direction.Equals(FVector2D(0, -1), 1e-5));
    Indicator->SetWind(0, 270, 0);
    TestEqual(TEXT("Blueprint setter updates text"), Slate->GetWind().SpeedLabel(), FString(TEXT("0 MPH")));
    Indicator->ReleaseSlateResources(true);
    const auto Rebuilt = StaticCastSharedRef<SWindIndicator>(Indicator->TakeWidget());
    TestEqual(TEXT("Rebuilt preview retains values"), Rebuilt->GetWind().SpeedLabel(), FString(TEXT("0 MPH")));

    UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UOpenGolf/UI/WBP_WindIndicator.WBP_WindIndicator_C"));
    if (!TestNotNull(TEXT("Saved Designer asset loads"), WidgetClass))
        return false;
    UUserWidget* Preview = NewObject<UUserWidget>(GetTransientPackage(), WidgetClass);
    Preview->SetDesignerFlags(EWidgetDesignFlags::Designing);
    if (!TestTrue(TEXT("Designer asset initializes without Play"), Preview->Initialize()))
        return false;
    const auto* Child = Cast<UOpenGolfWindIndicator>(Preview->WidgetTree->FindWidget(TEXT("WindIndicator")));
    if (!TestNotNull(TEXT("Designer asset contains native indicator"), Child))
        return false;
    const auto* Slot = Cast<UCanvasPanelSlot>(Child->Slot);
    if (!TestNotNull(TEXT("Indicator uses a Canvas slot"), Slot))
        return false;
    TestEqual(TEXT("Top center anchor"), Slot->GetAnchors().Minimum, FVector2D(0.5, 0));
    TestEqual(TEXT("Centered alignment"), Slot->GetAlignment(), FVector2D(0.5, 0));
    TestEqual(TEXT("Top margin"), Slot->GetPosition(), FVector2D(0, 20));
    TestTrue(TEXT("Slot follows desired size"), Slot->GetAutoSize());
    return true;
}
#endif
