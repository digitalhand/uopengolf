// SPDX-License-Identifier: MIT
#include "GameUpdateFormat.h"

namespace GameUpdate
{
    FString FormatSize(int64 Bytes)
    {
        const double Value = double(FMath::Max<int64>(Bytes, 0));
        if (Value >= 1e9) return FString::Printf(TEXT("%.1f GB"), Value / 1e9);
        if (Value >= 1e6) return FString::Printf(TEXT("%.0f MB"), Value / 1e6);
        return FString::Printf(TEXT("%.0f KB"), FMath::CeilToDouble(Value / 1e3));
    }

    FString ProgressLine(int64 Received, int64 Total, double BytesPerSecond)
    {
        FString Line = FormatSize(Received);
        if (Total > 0) Line += TEXT(" of ") + FormatSize(Total);
        if (BytesPerSecond > 0) Line += TEXT("  ·  ") + FormatSize(int64(BytesPerSecond)) + TEXT("/s");
        return Line;
    }
}
