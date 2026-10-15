// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/HUD.h"
#include "UOpenGolfHUD.generated.h"

/** Hello-world presentation, available without imported assets or plugins. */
UCLASS()
class UOPENGOLF_API AUOpenGolfHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

    UFUNCTION(BlueprintPure, Category = "UOpenGolf")
    FText GetGreeting() const;
};
