// SPDX-License-Identifier: MIT
#include "UI/DisplayOptions.h"

namespace DisplayOptions
{
    namespace
    {
        /** Common display sizes; a list entry must fit the display, so no entry can be too large for anyone. */
        const FIntPoint StandardSizes[] = {
            {1280, 720}, {1280, 800}, {1366, 768}, {1440, 900}, {1600, 900}, {1680, 1050}, {1920, 1080}, {1920, 1200},
            {2560, 1080}, {2560, 1440}, {2560, 1600}, {3440, 1440}, {3840, 1600}, {3840, 2160}, {5120, 1440}, {5120, 2160}};
        /** Relative aspect difference accepted as the same shape (1366x768 against 16:9 is 0.05%). */
        constexpr float AspectTolerance = 0.02f;

        bool SameAspect(FIntPoint A, FIntPoint B)
        {
            if (A.Y <= 0 || B.Y <= 0) return false;
            const float RatioA = float(A.X) / float(A.Y), RatioB = float(B.X) / float(B.Y);
            return FMath::Abs(RatioA - RatioB) <= AspectTolerance * RatioB;
        }
        bool FitsInside(FIntPoint Size, FIntPoint Bounds) { return Size.X > 0 && Size.Y > 0 && Size.X < Bounds.X && Size.Y < Bounds.Y; }
        bool NoLargerThan(FIntPoint Size, FIntPoint Bounds) { return Size.X > 0 && Size.Y > 0 && Size.X <= Bounds.X && Size.Y <= Bounds.Y; }
        TArray<FIntPoint> SortedUnique(TArray<FIntPoint> Sizes)
        {
            Sizes.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
            TArray<FIntPoint> Unique;
            for (const FIntPoint& Size : Sizes) Unique.AddUnique(Size);
            return Unique;
        }
    }

    FModeChoice Modes(bool bBorderlessIsFullscreen)
    {
        if (bBorderlessIsFullscreen) return {{TEXT("Fullscreen"), TEXT("Windowed")}, {Fullscreen, Windowed}};
        return {{TEXT("Fullscreen"), TEXT("Borderless"), TEXT("Windowed")}, {Fullscreen, Borderless, Windowed}};
    }
    int32 ShownMode(int32 EngineMode, bool bBorderlessIsFullscreen)
    {
        return (bBorderlessIsFullscreen && EngineMode == Borderless) ? Fullscreen : EngineMode;
    }
    int32 EngineMode(int32 UiMode, bool bBorderlessIsFullscreen)
    {
        return (bBorderlessIsFullscreen && UiMode == Fullscreen) ? Borderless : UiMode;
    }
    bool DesktopSized(int32 UiMode, const FFacts& Facts)
    {
        return UiMode == Borderless || (UiMode == Fullscreen && (Facts.bBorderlessIsFullscreen || Facts.RhiModes.IsEmpty()));
    }
    TArray<FIntPoint> Resolutions(int32 UiMode, const FFacts& Facts, FIntPoint Current)
    {
        TArray<FIntPoint> Offered;
        if (UiMode == Windowed)
        {
            for (const FIntPoint& Size : StandardSizes) if (FitsInside(Size, Facts.WorkArea)) Offered.Add(Size);
            if (FitsInside(Current, Facts.WorkArea)) Offered.Add(Current);
        }
        else if (DesktopSized(UiMode, Facts))
        {
            if (Facts.bRenderResolution)
                for (const FIntPoint& Size : StandardSizes) if (SameAspect(Size, Facts.Desktop) && NoLargerThan(Size, Facts.Desktop)) Offered.Add(Size);
            if (Facts.Desktop.X > 0 && Facts.Desktop.Y > 0) Offered.Add(Facts.Desktop);
        }
        else
        {
            Offered = Facts.RhiModes;
            if (NoLargerThan(Current, Facts.Desktop)) Offered.Add(Current);
        }
        return SortedUnique(Offered);
    }
    FIntPoint Choose(const TArray<FIntPoint>& Offered, FIntPoint Current)
    {
        if (Offered.IsEmpty() || Offered.Contains(Current)) return Current;
        FIntPoint Largest = Offered[0];
        for (const FIntPoint& Size : Offered) if (Size.X * Size.Y > Largest.X * Largest.Y) Largest = Size;
        return Largest;
    }
    FRequest Plan(int32 UiMode, FIntPoint Resolution, const FFacts& Facts)
    {
        FRequest Request;
        Request.EngineMode = EngineMode(UiMode, Facts.bBorderlessIsFullscreen);
        if (DesktopSized(UiMode, Facts)) { Request.ScreenResolution = Facts.Desktop; Request.RenderResolution = Resolution; }
        else Request.ScreenResolution = Resolution;
        return Request;
    }
}
