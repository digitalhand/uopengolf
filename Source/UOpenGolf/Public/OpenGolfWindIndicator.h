// SPDX-License-Identifier: MIT
#pragma once

#include "Components/Widget.h"
#include "OpenGolfWindIndicator.generated.h"

class SWindIndicator;
namespace WindDisplay
{
    struct FState;
}

/** Wind readout shared by the range HUD and the UI Designer. */
UCLASS(meta = (DisplayName = "Open Golf Wind Indicator"))
class UOPENGOLF_API UOpenGolfWindIndicator : public UWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind", meta = (ClampMin = "0"))
    float WindSpeedMph = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
    float WindFromDeg = 90;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
    float ViewYawDeg = 0;

    UFUNCTION(BlueprintCallable, Category = "Wind")
    void SetWind(float SpeedMph, float FromDeg, float CameraYawDeg);

    void SetWindState(const WindDisplay::FState& State);
    virtual void SynchronizeProperties() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
    virtual const FText GetPaletteCategory() override { return FText::FromString(TEXT("Open Golf")); }
#endif

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    TSharedPtr<SWindIndicator> Indicator;
};
