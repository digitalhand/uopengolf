// SPDX-License-Identifier: MIT
#include "UOpenGolfHUD.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "RangeHud.h"
#include "UOpenGolfPlayerController.h"

void AUOpenGolfHUD::BeginPlay()
{
    Super::BeginPlay();
    if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
    {
        Widget = SNew(SRangeHud).Controller(Cast<AUOpenGolfPlayerController>(GetOwningPlayerController()));
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
    Super::EndPlay(EndPlayReason);
}
