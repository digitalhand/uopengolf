// SPDX-License-Identifier: MIT
#include "UOpenGolfGameMode.h"

#include "UOpenGolfHUD.h"
#include "UOpenGolfPlayerController.h"

AUOpenGolfGameMode::AUOpenGolfGameMode()
{
    HUDClass = AUOpenGolfHUD::StaticClass();
    PlayerControllerClass = AUOpenGolfPlayerController::StaticClass();
    DefaultPawnClass = nullptr;
}
