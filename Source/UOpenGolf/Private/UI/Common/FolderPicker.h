// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"

/** The operating system's folder chooser. Windows only for now; elsewhere a path is typed. */
namespace FolderPicker
{
    bool Supported();
    /** Shows the chooser over the game window, opened at Start; blocks until it closes. False when cancelled. */
    bool Pick(const FString& Title, const FString& Start, FString& OutFolder);
}
