// SPDX-License-Identifier: MIT
#include "UOpenGolfHUD.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "RangeHud.h"
#include "UOpenGolfPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "OpenGolfWindIndicator.h"

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
        // Below the test-shot panel (Z-order 10), which hides this HUD while it is open.
        Viewport->AddViewportWidgetContent(Widget.ToSharedRef(), 5);
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
    WindWidget = nullptr;
    Super::EndPlay(EndPlayReason);
}
