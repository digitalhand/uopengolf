// SPDX-License-Identifier: MIT
#include "HoleMap/HoleMapCompositor.h"
#include "Async/ParallelFor.h"
#include "Engine/Texture2D.h"
#include "HoleMap/HoleMapLook.h"
#include "TextureResource.h"
#include "UObject/Package.h"

namespace HoleMapCompositorDetail
{
    using HoleMapCompositor::FPicture;

    double DecodeSRGB(double Encoded)
    {
        return Encoded <= 0.04045 ? Encoded / 12.92 : FMath::Pow((Encoded + 0.055) / 1.055, 2.4);
    }

    /** Linear light of each 8-bit sRGB value, exact enough that converting back returns the same byte. */
    const TStaticArray<double, 256>& LinearTable()
    {
        static const TStaticArray<double, 256> Table = []
        {
            TStaticArray<double, 256> Values;
            for (int32 Value = 0; Value < 256; ++Value)
                Values[Value] = DecodeSRGB(Value / 255.0);
            return Values;
        }();
        return Table;
    }

    uint8 EncodeSRGB(double Linear)
    {
        const double Clamped = FMath::Clamp(Linear, 0.0, 1.0);
        const double Encoded = Clamped <= 0.0031308 ? Clamped * 12.92 : 1.055 * FMath::Pow(Clamped, 1 / 2.4) - 0.055;
        return uint8(FMath::Clamp(FMath::RoundToInt32(Encoded * 255), 0, 255));
    }

    /** World XY at the centre of pixel (X, Y) of a picture covering Frame. */
    FVector2D PixelToWorld(const FHoleMapFrame& Frame, const FPicture& Image, int32 X, int32 Y)
    {
        return Frame.ToWorld(FVector2D((X + 0.5) / Image.Width, (Y + 0.5) / Image.Height));
    }

    uint8 Channel(double Value) { return uint8(FMath::Clamp(FMath::RoundToInt32(Value), 0, 255)); }

    FColor Scaled(FColor Color, double Factor)
    {
        return FColor(Channel(Color.R * Factor), Channel(Color.G * Factor), Channel(Color.B * Factor));
    }

    /** A new opaque picture with PerPixel(X, Y, Color) applied to every pixel, a row per task. */
    template <typename FPerPixel>
    FPicture Map(const FPicture& Image, FPerPixel&& PerPixel)
    {
        FPicture Out;
        Out.Width = Image.Width;
        Out.Height = Image.Height;
        Out.Pixels.SetNumUninitialized(Image.Pixels.Num());
        ParallelFor(Image.Height, [&](int32 Y)
        {
            for (int32 X = 0; X < Image.Width; ++X)
            {
                const int32 Index = Y * Image.Width + X;
                FColor Color = PerPixel(X, Y, Image.Pixels[Index]);
                Color.A = 255;
                Out.Pixels[Index] = Color;
            }
        });
        return Out;
    }

    /** The next mip: each pixel the linear-light average of up to four below it. */
    FPicture HalfSize(const FPicture& Source)
    {
        FPicture Out;
        Out.Width = FMath::Max(1, Source.Width / 2);
        Out.Height = FMath::Max(1, Source.Height / 2);
        Out.Pixels.SetNumUninitialized(Out.Width * Out.Height);
        const TStaticArray<double, 256>& Linear = LinearTable();
        ParallelFor(Out.Height, [&](int32 Y)
        {
            for (int32 X = 0; X < Out.Width; ++X)
            {
                double R = 0, G = 0, B = 0, A = 0;
                for (int32 DY = 0; DY < 2; ++DY)
                {
                    for (int32 DX = 0; DX < 2; ++DX)
                    {
                        const int32 SX = FMath::Min(X * 2 + DX, Source.Width - 1), SY = FMath::Min(Y * 2 + DY, Source.Height - 1);
                        const FColor& Color = Source.Pixels[SY * Source.Width + SX];
                        R += Linear[Color.R];
                        G += Linear[Color.G];
                        B += Linear[Color.B];
                        A += Color.A;
                    }
                }
                Out.Pixels[Y * Out.Width + X] = FColor(EncodeSRGB(R / 4), EncodeSRGB(G / 4), EncodeSRGB(B / 4), Channel(A / 4));
            }
        });
        return Out;
    }
} // namespace HoleMapCompositorDetail

HoleMapCompositor::FPicture HoleMapCompositor::WithRelief(const FPicture& Image, const FHoleMapFrame& Frame, const FHoleMapField& Heights)
{
    if (!Heights.IsValid())
        return Image;
    double SinAltitude = 0, CosAltitude = 0;
    FMath::SinCos(&SinAltitude, &CosAltitude, FMath::DegreesToRadians(HoleMapLook::LightAltitudeDeg));
    const FVector2D TowardLight = (Frame.Up - Frame.Right()).GetSafeNormal();
    const FVector Light(TowardLight * CosAltitude, SinAltitude);
    return HoleMapCompositorDetail::Map(Image, [&](int32 X, int32 Y, FColor Color)
    {
        const FVector2D Slope = Heights.Gradient(HoleMapCompositorDetail::PixelToWorld(Frame, Image, X, Y)) * HoleMapLook::ReliefExaggeration;
        const FVector Normal = FVector(-Slope.X, -Slope.Y, 1).GetSafeNormal();
        // Relative to flat ground, which is lit at exactly SinAltitude: flat ground keeps its colour.
        const double Shade = FMath::Max(0.0, Normal | Light) / SinAltitude;
        return HoleMapCompositorDetail::Scaled(Color, 1 + HoleMapLook::ReliefStrength * (Shade - 1));
    });
}

double HoleMapCompositor::ContourInterval(double SpanCm)
{
    for (const double Interval : HoleMapLook::ContourIntervalsCm)
        if (SpanCm / Interval <= HoleMapLook::MaxContours)
            return Interval;
    return HoleMapLook::ContourIntervalsCm[UE_ARRAY_COUNT(HoleMapLook::ContourIntervalsCm) - 1];
}

HoleMapCompositor::FPicture HoleMapCompositor::WithContours(const FPicture& Image, const FHoleMapFrame& Frame, const FHoleMapField& Heights)
{
    if (!Heights.IsValid())
        return Image;
    const double Interval = ContourInterval(Heights.GetMax() - Heights.GetMin());
    // Which interval each pixel's ground falls in; a contour runs wherever that changes.
    TArray<int32> Bands;
    Bands.SetNumUninitialized(Image.Pixels.Num());
    ParallelFor(Image.Height, [&](int32 Y)
    {
        for (int32 X = 0; X < Image.Width; ++X)
            Bands[Y * Image.Width + X] = FMath::FloorToInt32(Heights.At(HoleMapCompositorDetail::PixelToWorld(Frame, Image, X, Y)) / Interval);
    });
    const int32 Thickness = FMath::Max(1, FMath::RoundToInt32(double(Image.Height) / HoleMapLook::ContourRowsPerWidth));
    return HoleMapCompositorDetail::Map(Image, [&](int32 X, int32 Y, FColor Color)
    {
        const int32 Here = Bands[Y * Image.Width + X];
        // The contour crossed between here and Thickness pixels right or down: the higher band's lower edge.
        TOptional<int32> Crossed;
        const auto Check = [&](int32 NX, int32 NY)
        {
            if (NX < Image.Width && NY < Image.Height && Bands[NY * Image.Width + NX] != Here)
                Crossed = FMath::Max(Here, Bands[NY * Image.Width + NX]);
        };
        Check(X + Thickness, Y);
        Check(X, Y + Thickness);
        if (!Crossed)
            return Color;
        const bool bIndex = (*Crossed % HoleMapLook::IndexContourEvery + HoleMapLook::IndexContourEvery) % HoleMapLook::IndexContourEvery == 0;
        return HoleMapCompositorDetail::Scaled(Color, 1 - (bIndex ? HoleMapLook::IndexContourDarkening : HoleMapLook::ContourDarkening));
    });
}

HoleMapCompositor::FPicture HoleMapCompositor::WithSurroundDimmed(const FPicture& Image, const FHoleMapFrame& Frame, const FHoleMapField& DistanceFromPlay)
{
    if (!DistanceFromPlay.IsValid())
        return Image;
    return HoleMapCompositorDetail::Map(Image, [&](int32 X, int32 Y, FColor Color)
    {
        const double Dim = FMath::SmoothStep(HoleMapLook::DimStartCm, HoleMapLook::DimFullCm, DistanceFromPlay.At(HoleMapCompositorDetail::PixelToWorld(Frame, Image, X, Y)));
        if (Dim <= 0)
            return Color;
        const double Luma = 0.2126 * Color.R + 0.7152 * Color.G + 0.0722 * Color.B;
        const double Saturation = FMath::Lerp(1.0, HoleMapLook::DimSaturation, Dim);
        const double Brightness = FMath::Lerp(1.0, HoleMapLook::DimBrightness, Dim);
        const auto Dimmed = [&](uint8 Value) { return HoleMapCompositorDetail::Channel((Luma + (Value - Luma) * Saturation) * Brightness); };
        return FColor(Dimmed(Color.R), Dimmed(Color.G), Dimmed(Color.B));
    });
}

TArray<HoleMapCompositor::FPicture> HoleMapCompositor::BuildMips(const FPicture& Image)
{
    TArray<FPicture> Chain;
    if (Image.Pixels.IsEmpty())
        return Chain;
    Chain.Add(Image);
    while (Chain.Last().Width > 1 || Chain.Last().Height > 1)
    {
        FPicture Next = HoleMapCompositorDetail::HalfSize(Chain.Last());
        Chain.Add(MoveTemp(Next));
    }
    return Chain;
}

UTexture2D* HoleMapCompositor::MakeTexture(const TArray<FPicture>& Mips, FName Name)
{
    if (Mips.IsEmpty() || Mips[0].Pixels.IsEmpty())
        return nullptr;
    const FName Unique = MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), Name);
    // Without image data it allocates the top mip but does not upload it yet: the upload below carries every mip.
    UTexture2D* Texture = UTexture2D::CreateTransient(Mips[0].Width, Mips[0].Height, PF_B8G8R8A8, Unique);
    if (!Texture)
        return nullptr;
    for (int32 Level = 0; Level < Mips.Num(); ++Level)
    {
        const FPicture& Image = Mips[Level];
        TIndirectArray<FTexture2DMipMap>& Levels = Texture->GetPlatformData()->Mips;
        FTexture2DMipMap* Mip = Level < Levels.Num() ? &Levels[Level] : new FTexture2DMipMap(Image.Width, Image.Height, 1);
        Mip->BulkData.Lock(LOCK_READ_WRITE);
        void* Data = Mip->BulkData.Realloc(Image.Pixels.Num() * sizeof(FColor));
        FMemory::Memcpy(Data, Image.Pixels.GetData(), Image.Pixels.Num() * sizeof(FColor));
        Mip->BulkData.Unlock();
        if (Level >= Levels.Num())
            Levels.Add(Mip);
    }
    Texture->SRGB = true;
    Texture->Filter = TF_Trilinear;
    Texture->NeverStream = true;
    Texture->UpdateResource();
    return Texture;
}
