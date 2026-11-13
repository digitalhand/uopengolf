// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"

/** Text for an update screen. */
namespace GameUpdate
{
    /** A byte count for people: "820 KB", "587 MB", "1.2 GB". */
    OPENGAMEUPDATE_API FString FormatSize(int64 Bytes);

    /** "412 MB of 587 MB  ·  24 MB/s"; parts that are unknown (zero or less) are left out. */
    OPENGAMEUPDATE_API FString ProgressLine(int64 Received, int64 Total, double BytesPerSecond);
}
