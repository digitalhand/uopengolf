// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/HUD.h"
#include "UOpenGolfHUD.generated.h"

/** Greeting and launch-monitor status panel, drawn without imported assets. */
UCLASS()
class UOPENGOLF_API AUOpenGolfHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

    UFUNCTION(BlueprintPure, Category = "UOpenGolf")
    FText GetGreeting() const;

private:
    void DrawLaunchMonitorPanel();
};
