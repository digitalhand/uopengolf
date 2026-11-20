// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/HUD.h"
#include "UOpenGolfHUD.generated.h"

class SWidget;
class UUserWidget;

/** Range HUD: shot tiles, course header and settings, play toolbar, wind controls, and the optional wind debug overlay. */
UCLASS()
class UOPENGOLF_API AUOpenGolfHUD : public AHUD
{
    GENERATED_BODY()

public:
    /** Draws the uopengolf.WindDebug overlay when it is on. */
    virtual void DrawHUD() override;
    void RefreshTheme();

    /** Only this C++ path refers to the wind Widget Blueprint, so Config/DefaultGame.ini always cooks its folder. */
    static constexpr const TCHAR* WindWidgetPath = TEXT("/Game/UOpenGolf/UI/WBP_WindIndicator.WBP_WindIndicator_C");

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY()
    TObjectPtr<UUserWidget> WindWidget;

    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|UI")
    TSoftClassPtr<UUserWidget> WindWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(WindWidgetPath));

    TSharedPtr<SWidget> Widget;
    /** Play mode, ball placement and hole score; see SPlayModeBar. */
    TSharedPtr<SWidget> PlayBar;
    /** The wind slide-out on the wind indicator; see SWindControl. */
    TSharedPtr<SWidget> WindControl;
};
