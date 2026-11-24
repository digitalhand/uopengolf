// SPDX-License-Identifier: MIT
#include "OnlinePicture.h"
#include "Engine/Texture2D.h"
#include "HttpModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageCore.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

namespace OpenGolfOnline::Picture
{
    bool IsAllowedUrl(const FString& Url)
    {
        if (Url.Len() > MaxUrlLength || !Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase) || Url.Len() <= 8)
            return false;
        for (const TCHAR C : Url)
            if (C <= TEXT(' ') || C == 0x7F)
                return false;
        return true;
    }

    bool Decode(TConstArrayView<uint8> Bytes, FImage& OutImage)
    {
        if (Bytes.IsEmpty() || Bytes.Num() > MaxBytes)
            return false;
        IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
        const EImageFormat Format = Module.DetectImageFormat(Bytes.GetData(), Bytes.Num());
        if (Format != EImageFormat::PNG && Format != EImageFormat::JPEG)
            return false;
        const TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(Format);
        // The header gives the size, so an oversized image is refused before its pixels are decoded.
        if (!Wrapper || !Wrapper->SetCompressed(Bytes.GetData(), Bytes.Num()))
            return false;
        const int64 Width = Wrapper->GetWidth();
        const int64 Height = Wrapper->GetHeight();
        if (Width < 1 || Height < 1 || Width > MaxSourceSide || Height > MaxSourceSide)
            return false;
        // The module's decoder (IImageWrapper::GetRawImage is not exported); the format was checked above.
        FImage Source;
        if (!Module.DecompressImage(Bytes.GetData(), Bytes.Num(), Source) || Source.SizeX != Width || Source.SizeY != Height)
            return false;
        Source.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);

        // The centre square, so a portrait or landscape photo is not squashed into the circle.
        const int32 Square = FMath::Min(Source.SizeX, Source.SizeY);
        const int32 Left = (Source.SizeX - Square) / 2;
        const int32 Top = (Source.SizeY - Square) / 2;
        FImage Cropped(Square, Square, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
        const TArrayView64<const FColor> From = Source.AsBGRA8();
        const TArrayView64<FColor> To = Cropped.AsBGRA8();
        for (int32 Row = 0; Row < Square; ++Row)
            FMemory::Memcpy(&To[int64(Row) * Square], &From[int64(Top + Row) * Source.SizeX + Left], Square * sizeof(FColor));

        FImageCore::ResizeImageAllocDest(Cropped, OutImage, Side, Side, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
        return OutImage.SizeX == Side && OutImage.SizeY == Side;
    }

    UTexture2D* MakeTexture(const FImage& Image)
    {
        if (Image.SizeX < 1 || Image.SizeY < 1 || Image.Format != ERawImageFormat::BGRA8)
            return nullptr;
        const FName Name = MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("AccountPicture"));
        UTexture2D* Texture = UTexture2D::CreateTransient(Image.SizeX, Image.SizeY, PF_B8G8R8A8, Name, Image.RawData);
        if (!Texture)
            return nullptr;
        Texture->SRGB = true;
        Texture->LODGroup = TEXTUREGROUP_UI;
        Texture->NeverStream = true;
        Texture->UpdateResource();
        return Texture;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MakeRequest(const FString& Url)
    {
        const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
        Request->SetURL(Url);
        Request->SetVerb(TEXT("GET"));
        Request->SetHeader(TEXT("Accept"), TEXT("image/png, image/jpeg"));
        Request->SetTimeout(TimeoutSeconds);
        return Request;
    }
} // namespace OpenGolfOnline::Picture
