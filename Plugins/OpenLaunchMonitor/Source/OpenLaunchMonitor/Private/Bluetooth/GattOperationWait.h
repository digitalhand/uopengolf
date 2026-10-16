// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

namespace GattOperationWait
{
    enum class EResult { Completed, Cancelled, TimedOut };

    /** No callbacks outlive this call. The clock and sleep are replaceable by deterministic tests. */
    inline EResult Wait(TFunctionRef<bool()> Completed, TFunctionRef<bool()> Cancelled,
        TFunctionRef<double()> Now, TFunctionRef<void(double)> Sleep, double Deadline)
    {
        for (;;)
        {
            if (Cancelled()) return EResult::Cancelled;
            const double Remaining = Deadline - Now();
            if (Remaining <= 0) return EResult::TimedOut;
            if (Completed()) return EResult::Completed;
            Sleep(FMath::Min(0.025, Remaining));
        }
    }
}
