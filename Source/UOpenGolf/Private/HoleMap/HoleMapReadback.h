// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

class UTextureRenderTarget2D;

/**
 * Reads a render target back to the CPU without stalling the game thread. Request queues a GPU copy behind the
 * rendering already queued (such as a scene capture); Poll hands over the pixels once the copy has landed, usually a
 * frame or two later. Both run on the game thread; the copy and the mapping of its memory run on the render thread.
 */
class FHoleMapReadback
{
public:
    enum class EStatus : uint8
    {
        /** Nothing requested, or the pixels were handed over. */
        Idle,
        Pending,
        Ready,
        /** The target is missing or its format is not 8-bit RGBA or BGRA; read it with ReadPixels instead. */
        Failed,
    };

    ~FHoleMapReadback();

    /** Queues a copy of Target; any earlier request is dropped. */
    void Request(UTextureRenderTarget2D& Target);

    /** Checks the copy; on Ready, OutPixels holds the target's pixels row by row and the request is finished. */
    EStatus Poll(TArray<FColor>& OutPixels);

private:
    struct FState;
    /** Drops the request; the render thread lets go of it after any command still queued for it. */
    void Release();

    TSharedPtr<FState, ESPMode::ThreadSafe> State;
};
