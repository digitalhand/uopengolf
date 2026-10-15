// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/GameModeBase.h"
#include "UOpenGolfGameMode.generated.h"

/** Minimal entry point; future shot orchestration belongs in the game layer. */
UCLASS()
class UOPENGOLF_API AUOpenGolfGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AUOpenGolfGameMode();
};
