// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/HUD.h"
#include "UOpenGolfHUD.generated.h"

class SWidget;
class UUserWidget;

/** Range HUD: shot tiles, wind and launch-monitor status. */
UCLASS()
class UOPENGOLF_API AUOpenGolfHUD : public AHUD
{
    GENERATED_BODY()

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
