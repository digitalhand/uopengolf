// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/HUD.h"
#include "UOpenGolfHUD.generated.h"

class SWidget;

/** Range HUD: shot tiles and a launch-monitor status box, built in Slate without imported assets. */
UCLASS()
class UOPENGOLF_API AUOpenGolfHUD : public AHUD
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TSharedPtr<SWidget> Widget;
};
