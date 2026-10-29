// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/HUD.h"
#include "UOpenGolfHUD.generated.h"

class SWidget;
class UUserWidget;

/** Range HUD: shot tiles, wind, launch-monitor status and the optional wind debug overlay. */
UCLASS()
class UOPENGOLF_API AUOpenGolfHUD : public AHUD
{
    GENERATED_BODY()

public:
    /** Draws the uopengolf.WindDebug overlay when it is on. */
    virtual void DrawHUD() override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY()
    TObjectPtr<UUserWidget> WindWidget;

    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|UI")
    TSoftClassPtr<UUserWidget> WindWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/UOpenGolf/UI/WBP_WindIndicator.WBP_WindIndicator_C")));

    TSharedPtr<SWidget> Widget;
};
