// SPDX-License-Identifier: MIT
#include "UOpenGolfHUD.h"
#include "OpenFairwayBall.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UI/HudLayers.h"
#include "UI/PlayModeBar.h"
#include "UI/RangeHud.h"
#include "UI/WindControl.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "OpenGolfFlagstickComponent.h"
#include "OpenGolfWindIndicator.h"
#include "OpenGolfWindSubsystem.h"
#include "UI/WindDebug.h"

namespace
{
    TAutoConsoleVariable<int32> CVarWindDebug(TEXT("uopengolf.WindDebug"), 0,
        TEXT("1 shows the wind against the Beaufort scale, the wind at grass, flag and crown height, and the flag's measured droop and flap rate."),
        ECVF_Default);
} // namespace

void AUOpenGolfHUD::BeginPlay()
{
    Super::BeginPlay();
    if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
    {
        AUOpenGolfPlayerController* Controller = Cast<AUOpenGolfPlayerController>(GetOwningPlayerController());
        UOpenGolfWindIndicator* Indicator = nullptr;
        TSharedPtr<SWidget> WindContent;
        if (UClass* WindClass = WindWidgetClass.LoadSynchronous())
        {
            WindWidget = CreateWidget<UUserWidget>(Controller, WindClass);
            if (WindWidget)
            {
                WindContent = WindWidget->TakeWidget();
                Indicator = Cast<UOpenGolfWindIndicator>(WindWidget->WidgetTree->FindWidget(TEXT("WindIndicator")));
            }
        }
        ensureMsgf(Indicator, TEXT("The range wind Widget Blueprint must contain WindIndicator."));
        Widget = SNew(SRangeHud).Controller(Controller).WindIndicator(Indicator).WindContent(WindContent);
        // Below the test-shot panel, which hides this HUD while it is open.
        Viewport->AddViewportWidgetContent(Widget.ToSharedRef(), HudLayers::ShotHud);
        // Their controls take clicks, so they sit over the shot HUD but under the test-shot panel.
        PlayBar = SNew(SPlayModeBar).Controller(Controller);
        Viewport->AddViewportWidgetContent(PlayBar.ToSharedRef(), HudLayers::Controls);
        WindControl = SNew(SWindControl).Controller(Controller).WindIndicator(Indicator);
        Viewport->AddViewportWidgetContent(WindControl.ToSharedRef(), HudLayers::Controls);
    }
}

void AUOpenGolfHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Widget.IsValid())
    {
        if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
        {
            Viewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
        }
        Widget.Reset();
    }
    for (TSharedPtr<SWidget>* Control : {&PlayBar, &WindControl})
    {
        if (!Control->IsValid())
            continue;
        if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
            Viewport->RemoveViewportWidgetContent(Control->ToSharedRef());
        Control->Reset();
    }
    WindWidget = nullptr;
    Super::EndPlay(EndPlayReason);
}

void AUOpenGolfHUD::DrawHUD()
{
    Super::DrawHUD();
    const auto* PlayMode = GetWorld()->GetAuthGameMode<AUOpenGolfGameMode>();
    const auto* Player = Cast<AUOpenGolfPlayerController>(GetOwningPlayerController());
    if (Canvas && GEngine && bShowHUD && Player && !Player->IsReplayPanelOpen() &&
        PlayMode && PlayMode->GetPlayMode() == EOpenGolfPlayMode::Hole &&
        PlayMode->GetBall() && PlayMode->GetRange() && PlayMode->GetRange()->GetFlagstick())
    {
        const auto* Flag = PlayMode->GetRange()->GetFlagstick();
        FVector2D Screen;
        const FVector Anchor = Flag->GetComponentTransform().TransformPosition(FVector(0, 0, UOpenGolfFlagstickComponent::PoleHeightCm + 30));
        if (Player->ProjectWorldLocationToScreen(Anchor, Screen) && Screen.X >= 0 && Screen.X < Canvas->ClipX && Screen.Y >= 0 && Screen.Y < Canvas->ClipY)
        {
            FOpenFairwayShotResult Result;
            PlayMode->GetBall()->GetShotResult(Result);
            const FString Label = PlayMode->GetBall()->IsHoled() ? TEXT("In") :
                OpenGolfPlay::DistanceToPinLabel(PlayMode->GetDistanceToCupM(), Result.Surface == EOpenFairwaySurface::Green);
            float Width, Height;
            const float Scale = FMath::Max(1.0f, Canvas->ClipY / 1080.0f);
            GetTextSize(Label, Width, Height, GEngine->GetMediumFont(), Scale);
            DrawRect(FLinearColor(0, 0, 0, 0.75f), Screen.X - Width / 2 - 6, Screen.Y - Height - 4, Width + 12, Height + 8);
            DrawText(Label, FLinearColor::White, Screen.X - Width / 2, Screen.Y - Height, GEngine->GetMediumFont(), Scale);
        }
    }
    if (CVarWindDebug.GetValueOnGameThread() == 0 || !Canvas || !GEngine)
        return;
    const UWorld* World = GetWorld();
    const UOpenGolfWindSubsystem* Wind = World ? World->GetSubsystem<UOpenGolfWindSubsystem>() : nullptr;
    if (!Wind)
        return;
    WindDebug::FInput Input;
    Input.WindMps = Wind->GetWindMps();
    Input.ReferenceHeightM = Wind->GetReferenceHeightM();
    Input.RoughnessM = Wind->GetRoughnessM();
    const AUOpenGolfGameMode* Mode = World->GetAuthGameMode<AUOpenGolfGameMode>();
    const AUOpenGolfRange* Range = Mode ? Mode->GetRange() : nullptr;
    if (const UOpenGolfFlagstickComponent* Flag = Range ? Range->GetFlagstick() : nullptr)
    {
        Input.bHasFlag = true;
        Input.FlagDroopDeg = Flag->GetMotion().DroopDeg;
        Input.FlagFlapHz = Flag->GetMotion().FlapHz;
    }
    // Left edge, below the top tiles; the canvas draws under the Slate HUD.
    UFont* Font = GEngine->GetMediumFont();
    const float Scale = FMath::Max(1.0f, Canvas->ClipY / 1080.0f);
    float Y = Canvas->ClipY * 0.3f;
    for (const FString& Line : WindDebug::Lines(Input))
    {
        DrawText(Line, FLinearColor::White, 24.0f, Y, Font, Scale);
        Y += Font->GetMaxCharHeight() * Scale * 1.2f;
    }
}
