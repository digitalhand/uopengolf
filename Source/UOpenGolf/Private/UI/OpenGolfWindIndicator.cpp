// SPDX-License-Identifier: MIT
#include "OpenGolfWindIndicator.h"

#include "UI/WindIndicator.h"

TSharedRef<SWidget> UOpenGolfWindIndicator::RebuildWidget()
{
    Indicator = SNew(SWindIndicator);
    Indicator->SetWind(WindDisplay::FromSettings(WindSpeedMph, WindFromDeg, ViewYawDeg));
    return Indicator.ToSharedRef();
}

void UOpenGolfWindIndicator::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    SetWindState(WindDisplay::FromSettings(WindSpeedMph, WindFromDeg, ViewYawDeg));
}

void UOpenGolfWindIndicator::SetWind(float SpeedMph, float FromDeg, float CameraYawDeg)
{
    WindSpeedMph = SpeedMph;
    WindFromDeg = FromDeg;
    ViewYawDeg = CameraYawDeg;
    SynchronizeProperties();
}

void UOpenGolfWindIndicator::SetWindState(const WindDisplay::FState& State)
{
    if (Indicator.IsValid())
        Indicator->SetWind(State);
}

void UOpenGolfWindIndicator::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    Indicator.Reset();
}
