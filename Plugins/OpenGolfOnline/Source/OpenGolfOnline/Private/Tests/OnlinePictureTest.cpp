// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "OpenGolfOnlineSubsystem.h"
#include "OnlinePicture.h"
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Interfaces/IHttpRequest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfOnlinePictureTest, "OpenGolf.Online.Picture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace OnlinePictureTestDetail
{
    /** A PNG whose left, middle and right thirds are red, green and blue. */
    TArray<uint8> ThirdsPng(int32 Width, int32 Height)
    {
        TArray<FColor> Pixels;
        Pixels.SetNum(Width * Height);
        for (int32 Y = 0; Y < Height; ++Y)
            for (int32 X = 0; X < Width; ++X)
                Pixels[Y * Width + X] = X * 3 < Width ? FColor::Red : X * 3 < 2 * Width ? FColor::Green : FColor::Blue;
        TArray64<uint8> Png;
        FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
        return TArray<uint8>(Png.GetData(), static_cast<int32>(Png.Num()));
    }

    FColor PixelAt(const FImage& Image, int32 X, int32 Y) { return Image.AsBGRA8()[int64(Y) * Image.SizeX + X]; }
} // namespace OnlinePictureTestDetail

bool FOpenGolfOnlinePictureTest::RunTest(const FString&)
{
    using namespace OpenGolfOnline;
    using namespace OnlinePictureTestDetail;

    // Addresses: https only, without spaces or control characters, and not too long.
    TestTrue(TEXT("A gravatar address is allowed"),
        Picture::IsAllowedUrl(TEXT("https://s.gravatar.com/avatar/abc?s=480&r=pg&d=https%3A%2F%2Fcdn.auth0.com%2Favatars%2Fpg.png")));
    TestFalse(TEXT("Plain http is refused"), Picture::IsAllowedUrl(TEXT("http://example.test/me.png")));
    TestFalse(TEXT("A scheme alone is refused"), Picture::IsAllowedUrl(TEXT("https://")));
    TestFalse(TEXT("An empty address is refused"), Picture::IsAllowedUrl(TEXT("")));
    TestFalse(TEXT("A script address is refused"), Picture::IsAllowedUrl(TEXT("javascript:alert(1)")));
    TestFalse(TEXT("A space is refused"), Picture::IsAllowedUrl(TEXT("https://example.test/a b.png")));
    TestFalse(TEXT("A line break is refused"), Picture::IsAllowedUrl(TEXT("https://example.test/a.png\n")));
    TestFalse(TEXT("An overlong address is refused"), Picture::IsAllowedUrl(TEXT("https://example.test/") + FString::ChrN(Picture::MaxUrlLength, TEXT('a'))));

    // Decoding: the centre square, resized to Side.
    const TArray<uint8> Wide = ThirdsPng(300, 200);
    FImage Image;
    if (TestTrue(TEXT("A PNG decodes"), Picture::Decode(Wide, Image)))
    {
        TestEqual(TEXT("Decoded width"), Image.SizeX, Picture::Side);
        TestEqual(TEXT("Decoded height"), Image.SizeY, Picture::Side);
        TestEqual(TEXT("Decoded as BGRA8"), static_cast<int32>(Image.Format), static_cast<int32>(ERawImageFormat::BGRA8));
        const FColor Centre = PixelAt(Image, Picture::Side / 2, Picture::Side / 2);
        const FColor Left = PixelAt(Image, 4, Picture::Side / 2);
        const FColor Right = PixelAt(Image, Picture::Side - 5, Picture::Side / 2);
        TestTrue(TEXT("The centre stays in the middle"), Centre.G > 200 && Centre.R < 60 && Centre.B < 60);
        TestTrue(TEXT("The crop keeps some of the left third"), Left.R > 200 && Left.G < 60);
        TestTrue(TEXT("The crop keeps some of the right third"), Right.B > 200 && Right.G < 60);
        UTexture2D* Texture = Picture::MakeTexture(Image);
        if (TestNotNull(TEXT("A texture is made"), Texture))
        {
            TestEqual(TEXT("Texture width"), Texture->GetSizeX(), Picture::Side);
            TestEqual(TEXT("A UI texture"), static_cast<int32>(Texture->LODGroup), static_cast<int32>(TEXTUREGROUP_UI));
        }
    }
    {
        FImage Orange(64, 32, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
        for (FColor& Pixel : Orange.AsBGRA8()) Pixel = FColor(255, 140, 0);
        TArray64<uint8> Jpeg;
        FImage Decoded;
        TestTrue(TEXT("A JPEG is encoded for the test"), FImageUtils::CompressImage(Jpeg, TEXT("jpg"), Orange));
        TestTrue(TEXT("A JPEG decodes"), Picture::Decode(TArray<uint8>(Jpeg.GetData(), static_cast<int32>(Jpeg.Num())), Decoded) &&
            Decoded.SizeX == Picture::Side);
    }
    FImage Ignored;
    TestFalse(TEXT("An empty reply is refused"), Picture::Decode({}, Ignored));
    TestFalse(TEXT("Text is refused"), Picture::Decode(TArray<uint8>({'<', 'h', 't', 'm', 'l', '>', 0, 0, 0, 0}), Ignored));
    {
        TArray<uint8> Big = ThirdsPng(8, 8);
        Big.SetNumZeroed(static_cast<int32>(Picture::MaxBytes) + 1);
        TestFalse(TEXT("A reply over the size limit is refused"), Picture::Decode(Big, Ignored));
    }
    TestFalse(TEXT("An image wider than the limit is refused"), Picture::Decode(ThirdsPng(Picture::MaxSourceSide + 1, 1), Ignored));

    // The download carries no credentials.
    {
        const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Picture::MakeRequest(TEXT("https://example.test/me.png"));
        TestEqual(TEXT("The download is a GET"), Request->GetVerb(), FString(TEXT("GET")));
        TestEqual(TEXT("The download asks for the address given"), Request->GetURL(), FString(TEXT("https://example.test/me.png")));
        TestTrue(TEXT("The download sends no Authorization header"), Request->GetHeader(TEXT("Authorization")).IsEmpty());
        TestTrue(TEXT("The download sends no cookies"), Request->GetHeader(TEXT("Cookie")).IsEmpty());
    }

    // The session keeps the picture the login service names, for this account only.
    FConfig Config;
    Config.Authority = TEXT("https://login.example.test/"); Config.ClientId = TEXT("test-client");
    Config.Audience = TEXT("https://api.example.test"); Config.ApiOrigin = TEXT("https://api.example.test");
    Config.ApplicationName = TEXT("Test golf"); Config.StorageName = TEXT("OpenGolfOnlineTests"); Config.CallbackPorts = {47801};
    struct FPending { FString Url; FHttpCallback Done; };
    TArray<FPending> Pending;
    auto* Session = NewObject<UOpenGolfOnlineSubsystem>(NewObject<UGameInstance>());
    const FString Root = FPaths::ProjectSavedDir() / TEXT("Automation") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Session->SetupForTests(Config, [&Pending](const FString& Url, const FString&, const FString&, const FString&, FHttpCallback Done)
    {
        Pending.Add({Url, MoveTemp(Done)});
        auto Cancel = MakeShared<FRequestCancellation, ESPMode::ThreadSafe>(); Cancel->Finished.store(true); return Cancel;
    }, Root);
    struct FFetch { FString Url; TFunction<void(int32, TArray<uint8>)> Reply; };
    TArray<FFetch> Fetches;
    Session->SetPictureFetcherForTests([&Fetches](const FString& Url, TFunction<void(int32, TArray<uint8>)> Reply) { Fetches.Add({Url, MoveTemp(Reply)}); });
    int32 Changes = 0;
    Session->OnPictureChanged.AddLambda([&Changes] { ++Changes; });
    auto Respond = [&](int32 Status, const FString& Body)
    {
        if (Pending.IsEmpty()) { AddError(TEXT("Expected a queued request")); return; }
        auto Request = MoveTemp(Pending[0]); Pending.RemoveAt(0); Request.Done({Status, Body, true});
    };
    auto UserInfo = [](const FString& Subject, const FString& PictureUrl)
    {
        FString Body = TEXT("{\"sub\":\"") + Subject + TEXT("\",\"name\":\"Pat Golfer\"");
        if (!PictureUrl.IsEmpty()) Body += TEXT(",\"picture\":\"") + PictureUrl + TEXT("\"");
        return Body + TEXT("}");
    };
    const FString Tokens = TEXT("{\"access_token\":\"header.e30.signature\",\"refresh_token\":\"refresh-a\",\"token_type\":\"Bearer\",\"expires_in\":3600}");
    auto Login = [&](const FString& Subject, const FString& PictureUrl)
    {
        Pending.Empty();
        Session->KeepQrSignInReady();
        Respond(200, TEXT("{\"device_code\":\"private-code\",\"user_code\":\"USER-CODE\",\"verification_uri\":\"https://login.example.test/activate\","
            "\"verification_uri_complete\":\"https://login.example.test/activate?user_code=USER-CODE\",\"expires_in\":600,\"interval\":5}"));
        Session->PollAt = 0; Session->Tick(0);
        Respond(200, Tokens);
        Respond(200, UserInfo(Subject, PictureUrl));
        Respond(200, TEXT("{\"id\":\"player\",\"display_name\":\"Golfer\"}"));
    };
    auto RefreshWith = [&](const FString& Subject, const FString& PictureUrl)
    {
        Pending.Empty();
        Session->Refresh([](bool) {});
        Respond(200, Tokens);
        Respond(200, UserInfo(Subject, PictureUrl));
    };
    const FString First = TEXT("https://cdn.example.test/one.png");
    const FString Second = TEXT("https://cdn.example.test/two.png");

    TestTrue(TEXT("Signed out, there is no display name"), Session->GetDisplayName().IsEmpty());
    Login(TEXT("auth0|one"), First);
    if (!TestEqual(TEXT("Sign-in fetches the account picture once"), Fetches.Num(), 1)) { Session->bStopped = true; return false; }
    TestEqual(TEXT("The picture comes from the login service's address"), Fetches[0].Url, First);
    TestNull(TEXT("No picture until it downloads"), Session->GetPicture());
    TestEqual(TEXT("The display name comes from the profile"), Session->GetDisplayName(), FString(TEXT("Golfer")));
    Fetches[0].Reply(200, Wide);
    if (TestNotNull(TEXT("The downloaded picture is kept"), Session->GetPicture()))
        TestEqual(TEXT("The picture is small"), Session->GetPicture()->GetSizeX(), Picture::Side);
    TestEqual(TEXT("The new picture is announced"), Changes, 1);

    RefreshWith(TEXT("auth0|one"), First);
    TestEqual(TEXT("A token refresh with the same picture downloads nothing"), Fetches.Num(), 1);
    TestNotNull(TEXT("A token refresh keeps the picture"), Session->GetPicture());

    RefreshWith(TEXT("auth0|one"), Second);
    TestNull(TEXT("A changed address drops the old picture"), Session->GetPicture());
    TestEqual(TEXT("A changed address downloads again"), Fetches.Num(), 2);
    Fetches[1].Reply(404, {});
    TestNull(TEXT("A failed download shows no picture"), Session->GetPicture());
    RefreshWith(TEXT("auth0|one"), Second);
    TestEqual(TEXT("A failed picture is fetched again at the next refresh"), Fetches.Num(), 3);
    Fetches[2].Reply(200, TArray<uint8>({'n', 'o', 't', ' ', 'a', 'n', ' ', 'i', 'm', 'a', 'g', 'e'}));
    TestNull(TEXT("A reply that is not an image shows no picture"), Session->GetPicture());

    RefreshWith(TEXT("auth0|one"), TEXT("http://cdn.example.test/insecure.png"));
    TestEqual(TEXT("A plain http picture is not fetched"), Fetches.Num(), 3);

    RefreshWith(TEXT("auth0|one"), First);
    RefreshWith(TEXT("auth0|one"), Second);
    TestEqual(TEXT("Each new address downloads"), Fetches.Num(), 5);
    Fetches[3].Reply(200, Wide);
    TestNull(TEXT("A reply for an earlier address is ignored"), Session->GetPicture());
    Fetches[4].Reply(200, Wide);
    TestNotNull(TEXT("The latest address's picture is kept"), Session->GetPicture());

    Login(TEXT("auth0|two"), FString());
    TestNull(TEXT("Another account never shows the previous account's picture"), Session->GetPicture());
    TestEqual(TEXT("An account without a picture downloads nothing"), Fetches.Num(), 5);

    Login(TEXT("auth0|one"), First);
    Fetches.Last().Reply(200, Wide);
    TestNotNull(TEXT("Signing in again shows the picture"), Session->GetPicture());
    Pending.Empty();
    Session->SignOut();
    TestNull(TEXT("Signing out forgets the picture"), Session->GetPicture());
    TestTrue(TEXT("Signing out forgets the display name"), Session->GetDisplayName().IsEmpty());

    Pending.Empty();
    Session->bStopped = true;
    IFileManager::Get().DeleteDirectory(*Root, false, true);
    return true;
}
#endif
