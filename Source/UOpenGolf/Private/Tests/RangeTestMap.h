// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

FString GetRangeTestMap();

class UOpenGolfTerrainDefinition;
bool FindRangeBunkerCenter(const UOpenGolfTerrainDefinition& Definition, FVector2D& Center);
