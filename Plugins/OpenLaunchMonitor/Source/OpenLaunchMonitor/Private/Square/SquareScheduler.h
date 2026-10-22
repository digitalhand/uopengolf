// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

/** Runs delayed work on the game thread. Tests substitute a manually advanced clock. */
class ISquareScheduler
{
public:
    virtual ~ISquareScheduler() = default;
    virtual void Schedule(float DelaySeconds, TFunction<void()> Work) = 0;
};

/** Core-ticker scheduler; works in editor, game and headless runs without a world. */
class FTickerSquareScheduler final : public ISquareScheduler
{
public:
    virtual ~FTickerSquareScheduler() override
    {
        for (const FTSTicker::FDelegateHandle& Handle : Handles)
        {
            FTSTicker::RemoveTicker(Handle);
        }
    }

    virtual void Schedule(float DelaySeconds, TFunction<void()> Work) override
    {
        TSharedRef<FTSTicker::FDelegateHandle> Self = MakeShared<FTSTicker::FDelegateHandle>();
        *Self = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
                                                         [this, Self, Work = MoveTemp(Work)](float)
                                                         {
                                                             Handles.Remove(*Self);
                                                             Work();
                                                             return false;
                                                         }),
            FMath::Max(DelaySeconds, 0.0f));
        Handles.Add(*Self);
    }

private:
    TSet<FTSTicker::FDelegateHandle> Handles;
};
