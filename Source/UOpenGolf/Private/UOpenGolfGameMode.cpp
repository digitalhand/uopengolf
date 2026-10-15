// SPDX-License-Identifier: MIT
#include "UOpenGolfGameMode.h"

#include "UOpenGolfHUD.h"

AUOpenGolfGameMode::AUOpenGolfGameMode()
{
    HUDClass = AUOpenGolfHUD::StaticClass();
    DefaultPawnClass = nullptr;
}
