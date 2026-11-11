// SPDX-License-Identifier: MIT
#include "OpenGolfGameUserSettings.h"

void UOpenGolfGameUserSettings::SetRenderResolution(FIntPoint Size)
{
    DesiredScreenWidth = Size.X;
    DesiredScreenHeight = Size.Y;
    bUseDesiredScreenHeight = true;
    UpdateResolutionQuality();
}

void UOpenGolfGameUserSettings::ClearRenderResolution()
{
    bUseDesiredScreenHeight = false;
}

FIntPoint UOpenGolfGameUserSettings::GetRenderResolution() const
{
    return bUseDesiredScreenHeight ? FIntPoint(DesiredScreenWidth, DesiredScreenHeight) : GetScreenResolution();
}
