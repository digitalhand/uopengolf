// SPDX-License-Identifier: MIT
#include "HoleMap/HoleMapReadback.h"

#include "Engine/TextureRenderTarget2D.h"
#include "RHICommandList.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "TextureResource.h"

#include <atomic>

struct FHoleMapReadback::FState
{
    /** Render thread only. */
    TUniquePtr<FRHIGPUTextureReadback> Copy;
    EPixelFormat Format = PF_Unknown;
    FIntPoint Size = FIntPoint::ZeroValue;
    /** Written on the render thread before Status becomes Ready or Failed; read on the game thread after. */
    TArray<FColor> Pixels;
    std::atomic<EStatus> Status{EStatus::Pending};
    std::atomic<bool> bCheckQueued{false};
};

namespace HoleMapReadbackDetail
{
    /** Copies the mapped rows into FColor order (B, G, R, A in memory); false for any other format. */
    bool CopyRows(const uint8* Data, int32 RowPitchPixels, EPixelFormat Format, FIntPoint Size, TArray<FColor>& Out)
    {
        if ((Format != PF_B8G8R8A8 && Format != PF_R8G8B8A8) || !Data || RowPitchPixels < Size.X)
            return false;
        Out.SetNumUninitialized(Size.X * Size.Y);
        for (int32 Y = 0; Y < Size.Y; ++Y)
        {
            const uint8* Row = Data + int64(Y) * RowPitchPixels * 4;
            FColor* Target = Out.GetData() + int64(Y) * Size.X;
            if (Format == PF_B8G8R8A8)
            {
                FMemory::Memcpy(Target, Row, Size.X * sizeof(FColor));
                continue;
            }
            for (int32 X = 0; X < Size.X; ++X)
                Target[X] = FColor(Row[X * 4], Row[X * 4 + 1], Row[X * 4 + 2], Row[X * 4 + 3]);
        }
        return true;
    }
} // namespace HoleMapReadbackDetail

FHoleMapReadback::~FHoleMapReadback()
{
    Release();
}

void FHoleMapReadback::Release()
{
    if (!State)
        return;
    // The copy's GPU resources are let go of on the render thread, after any command still queued for them.
    ENQUEUE_RENDER_COMMAND(ReleaseHoleMapReadback)([Released = MoveTemp(State)](FRHICommandListImmediate&) {});
    State.Reset();
}

void FHoleMapReadback::Request(UTextureRenderTarget2D& Target)
{
    Release();
    State = MakeShared<FState, ESPMode::ThreadSafe>();
    State->Size = FIntPoint(Target.SizeX, Target.SizeY);
    FTextureRenderTargetResource* Resource = Target.GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        State->Status = EStatus::Failed;
        return;
    }
    ENQUEUE_RENDER_COMMAND(CopyHoleMapReadback)([Shared = State, Resource](FRHICommandListImmediate& RHICmdList)
    {
        FRHITexture* Texture = Resource->GetShaderResourceTexture();
        if (!Texture || Texture->GetSizeXY() != Shared->Size)
        {
            Shared->Status = EStatus::Failed;
            return;
        }
        Shared->Format = Texture->GetFormat();
        Shared->Copy = MakeUnique<FRHIGPUTextureReadback>(TEXT("HoleMapReadback"));
        RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::SRVMask, ERHIAccess::CopySrc));
        Shared->Copy->EnqueueCopy(RHICmdList, Texture, FIntVector::ZeroValue, 0, FIntVector::ZeroValue);
        RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::CopySrc, ERHIAccess::SRVMask));
    });
}

FHoleMapReadback::EStatus FHoleMapReadback::Poll(TArray<FColor>& OutPixels)
{
    if (!State)
        return EStatus::Idle;
    const EStatus Status = State->Status;
    if (Status == EStatus::Ready)
        OutPixels = MoveTemp(State->Pixels);
    if (Status != EStatus::Pending)
    {
        Release();
        return Status;
    }
    // One check at a time: it maps the copy once the GPU has written it.
    if (!State->bCheckQueued.exchange(true))
    {
        ENQUEUE_RENDER_COMMAND(CheckHoleMapReadback)([Shared = State](FRHICommandListImmediate&)
        {
            Shared->bCheckQueued = false;
            if (Shared->Status != EStatus::Pending || !Shared->Copy || !Shared->Copy->IsReady())
                return;
            int32 RowPitchPixels = 0;
            const uint8* Data = static_cast<const uint8*>(Shared->Copy->Lock(RowPitchPixels));
            const bool bCopied = HoleMapReadbackDetail::CopyRows(Data, RowPitchPixels, Shared->Format, Shared->Size, Shared->Pixels);
            if (Data)
                Shared->Copy->Unlock();
            Shared->Copy.Reset();
            Shared->Status = bCopied ? EStatus::Ready : EStatus::Failed;
        });
    }
    return EStatus::Pending;
}
