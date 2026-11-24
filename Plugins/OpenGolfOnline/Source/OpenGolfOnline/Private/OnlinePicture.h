// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"

struct FImage;
class UTexture2D;

/**
 * The account picture named by the login service's profile (the OpenID "picture" claim). It is fetched without
 * credentials, from whichever https host the login service names, and shrunk to a small UI image.
 */
namespace OpenGolfOnline::Picture
{
    constexpr int32 MaxUrlLength = 2048;
    /** A larger reply is refused before decoding. */
    constexpr int64 MaxBytes = 1024 * 1024;
    /** A larger image is refused before its pixels are decoded. */
    constexpr int32 MaxSourceSide = 2048;
    /** The decoded picture is this square, in pixels. */
    constexpr int32 Side = 128;
    constexpr float TimeoutSeconds = 15.0f;

    /** An https address without spaces or control characters, at most MaxUrlLength long. */
    bool IsAllowedUrl(const FString& Url);
    /**
     * PNG or JPEG only, at most MaxBytes and MaxSourceSide on each side. The centre square is resized to Side and
     * returned as sRGB BGRA8.
     */
    bool Decode(TConstArrayView<uint8> Bytes, FImage& OutImage);
    /** A transient UI texture of a decoded picture; null for an empty image. */
    UTexture2D* MakeTexture(const FImage& Image);
    /** A GET for Url that carries no credentials: no Authorization header and no cookies. It is not sent. */
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MakeRequest(const FString& Url);
} // namespace OpenGolfOnline::Picture
