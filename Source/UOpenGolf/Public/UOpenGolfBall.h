// SPDX-License-Identifier: MIT
#pragma once

#include "OpenFairwayBall.h"
#include "UOpenGolfBall.generated.h"

/** Game-owned presentation; the physics plugin stays independent of project assets. */
UCLASS()
class UOPENGOLF_API AUOpenGolfBall : public AOpenFairwayBall
{
    GENERATED_BODY()
public:
    AUOpenGolfBall();
};
