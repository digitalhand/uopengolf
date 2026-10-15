// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/HUD.h"
#include "UOpenGolfHUD.generated.h"

/** Launch-monitor status and shot result panels, drawn without imported assets. */
UCLASS()
class UOPENGOLF_API AUOpenGolfHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

private:
    struct FLine
    {
        FString Text;
        FLinearColor Color;
    };

    void DrawLaunchMonitorPanel();
    void DrawShotPanel();
    /** Draws lines in a translucent panel; a negative Left aligns the panel to the right edge. */
    void DrawPanel(const TArray<FLine>& Lines, float Left, float Top);
};
