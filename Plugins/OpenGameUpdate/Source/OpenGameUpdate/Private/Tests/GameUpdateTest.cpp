// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Async/TaskGraphInterfaces.h"
#include "Engine/GameInstance.h"
#include "GameUpdateFormat.h"
#include "GameUpdateHash.h"
#include "GameUpdateRelease.h"
#include "GameUpdateSource.h"
#include "GameUpdateSubsystem.h"
#include "GameUpdateVersion.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    FGameUpdateConfig TestConfig()
    {
        FGameUpdateConfig C;
        C.Repository = TEXT("owner/game");
        C.InstallerAsset = TEXT("Game-{version}-Windows-x64-Setup.exe");
        C.InstallerArguments = TEXT("/SILENT /DIR=\"{installdir}\"");
        C.CurrentVersion = TEXT("0.0.8");
        C.UserAgent = TEXT("Game/0.0.8");
        return C;
    }

    /** Answers from memory: a release, and a download that writes Payload. */
    struct FFakeSource final : IGameUpdateSource
    {
        bool bReleaseOk = true;
        FGameUpdateRelease Release;
        FString Payload = TEXT("installer bytes");
        int32 Cancels = 0;
        virtual void FetchLatest(FOnRelease OnDone) override { OnDone(bReleaseOk, Release, bReleaseOk ? FString() : TEXT("offline")); }
        virtual void Download(const FString&, const FString& Path, FOnProgress OnProgress, FOnDone OnDone) override
        {
            FFileHelper::SaveStringToFile(Payload, *Path);
            OnProgress(Payload.Len());
            OnDone(true, {});
        }
        virtual void Cancel() override { ++Cancels; }
    };

    /** Runs game-thread tasks until the subsystem leaves Verifying, for at most five seconds. */
    void WaitForVerify(UGameUpdateSubsystem* Updater)
    {
        const double Deadline = FPlatformTime::Seconds() + 5;
        while (Updater->GetState() == EGameUpdateState::Verifying && FPlatformTime::Seconds() < Deadline)
        {
            FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
            FPlatformProcess::Sleep(0.01f);
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameUpdateVersionTest, "OpenGameUpdate.Version", Flags)
bool FGameUpdateVersionTest::RunTest(const FString&)
{
    using GameUpdate::IsNewer;
    TestTrue(TEXT("Patch"), IsNewer(TEXT("0.0.8"), TEXT("0.0.7")));
    TestTrue(TEXT("Leading v"), IsNewer(TEXT("v0.1.0"), TEXT("0.0.9")));
    TestTrue(TEXT("Numbers, not text"), IsNewer(TEXT("0.0.10"), TEXT("0.0.9")));
    TestFalse(TEXT("Same version"), IsNewer(TEXT("0.0.7"), TEXT("0.0.7")));
    TestFalse(TEXT("Older"), IsNewer(TEXT("0.0.6"), TEXT("0.0.7")));
    TestTrue(TEXT("Release beats its prerelease"), IsNewer(TEXT("1.0.0"), TEXT("1.0.0-rc.1")));
    TestTrue(TEXT("Release beats a dev build"), IsNewer(TEXT("0.0.7"), TEXT("0.0.7-dev.cd5c3b8")));
    TestFalse(TEXT("A dev build of the next version is not behind"), IsNewer(TEXT("0.0.7"), TEXT("0.0.8-dev.cd5c3b8")));
    TestTrue(TEXT("Numeric prerelease parts"), IsNewer(TEXT("1.0.0-rc.10"), TEXT("1.0.0-rc.2")));
    TestTrue(TEXT("Longer prerelease"), IsNewer(TEXT("1.0.0-rc.1.1"), TEXT("1.0.0-rc.1")));
    TestFalse(TEXT("Build metadata is ignored"), IsNewer(TEXT("1.0.0+abc"), TEXT("1.0.0")));
    TestFalse(TEXT("Unreadable latest"), IsNewer(TEXT("latest"), TEXT("0.0.7")));
    TestFalse(TEXT("Unreadable current"), IsNewer(TEXT("0.0.8"), TEXT("")));
    TestFalse(TEXT("Two parts"), IsNewer(TEXT("1.0"), TEXT("0.0.7")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameUpdateReleaseTest, "OpenGameUpdate.Release", Flags)
bool FGameUpdateReleaseTest::RunTest(const FString&)
{
    const FGameUpdateConfig Config = TestConfig();
    FGameUpdateRelease R;
    FString Error;
    const FString Json = TEXT(R"({"tag_name":"v0.0.8","html_url":"https://github.com/owner/game/releases/tag/v0.0.8","body":"Notes",
        "assets":[{"name":"Game-0.0.8-Windows-x64.zip","browser_download_url":"https://example.com/a.zip","size":5},
                  {"name":"Game-0.0.8-Windows-x64-Setup.exe","browser_download_url":"https://example.com/setup.exe","size":587000000},
                  {"name":"Game-0.0.8-Windows-x64-Setup.exe.sha256","browser_download_url":"https://example.com/setup.exe.sha256","size":99}]})");
    TestTrue(TEXT("Release reads"), GameUpdate::ParseGitHubRelease(Json, Config, R, Error));
    TestEqual(TEXT("Version drops the v"), R.Version, FString(TEXT("0.0.8")));
    TestEqual(TEXT("Notes"), R.Notes, FString(TEXT("Notes")));
    TestEqual(TEXT("Installer URL"), R.InstallerUrl, FString(TEXT("https://example.com/setup.exe")));
    TestEqual(TEXT("Installer size"), R.InstallerSize, int64(587000000));
    TestEqual(TEXT("Checksum URL"), R.ChecksumUrl, FString(TEXT("https://example.com/setup.exe.sha256")));

    FGameUpdateRelease NoInstaller;
    TestTrue(TEXT("Release without installer still reads"),
        GameUpdate::ParseGitHubRelease(TEXT(R"({"tag_name":"v0.0.8","assets":[]})"), Config, NoInstaller, Error));
    TestFalse(TEXT("No installer"), NoInstaller.HasInstaller());
    FGameUpdateRelease Insecure;
    GameUpdate::ParseGitHubRelease(TEXT(R"({"tag_name":"v0.0.8","assets":[{"name":"Game-0.0.8-Windows-x64-Setup.exe","browser_download_url":"http://example.com/s.exe"}]})"),
        Config, Insecure, Error);
    TestFalse(TEXT("Plain HTTP download is refused"), Insecure.HasInstaller());
    TestFalse(TEXT("Not JSON"), GameUpdate::ParseGitHubRelease(TEXT("<html>"), Config, R, Error));
    TestFalse(TEXT("No tag"), GameUpdate::ParseGitHubRelease(TEXT(R"({"message":"Not Found"})"), Config, R, Error));

    const FString Hash = TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    TestEqual(TEXT("sha256sum line"), GameUpdate::ParseChecksum(Hash + TEXT("  Setup.exe\n"), TEXT("Setup.exe")), Hash);
    TestEqual(TEXT("Binary-mode name"), GameUpdate::ParseChecksum(Hash.ToUpper() + TEXT(" *Setup.exe"), TEXT("Setup.exe")), Hash);
    TestEqual(TEXT("Lone hash"), GameUpdate::ParseChecksum(Hash, TEXT("Setup.exe")), Hash);
    TestTrue(TEXT("Another file's hash"), GameUpdate::ParseChecksum(Hash + TEXT("  Other.exe"), TEXT("Setup.exe")).IsEmpty());
    TestTrue(TEXT("Short hash"), GameUpdate::ParseChecksum(TEXT("abc  Setup.exe"), TEXT("Setup.exe")).IsEmpty());

    TestEqual(TEXT("Megabytes"), GameUpdate::FormatSize(587177650), FString(TEXT("587 MB")));
    TestEqual(TEXT("Gigabytes"), GameUpdate::FormatSize(1200000000), FString(TEXT("1.2 GB")));
    TestEqual(TEXT("Kilobytes"), GameUpdate::FormatSize(820000), FString(TEXT("820 KB")));
    TestEqual(TEXT("Progress line"), GameUpdate::ProgressLine(412000000, 587000000, 24000000), FString(TEXT("412 MB of 587 MB  ·  24 MB/s")));
    TestEqual(TEXT("Unknown size and speed"), GameUpdate::ProgressLine(5000000, 0, 0), FString(TEXT("5 MB")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameUpdateHashTest, "OpenGameUpdate.Hash", Flags)
bool FGameUpdateHashTest::RunTest(const FString&)
{
    auto Hex = [](const char* Text) { return GameUpdate::Sha256Hex(TConstArrayView<uint8>(reinterpret_cast<const uint8*>(Text), FCStringAnsi::Strlen(Text))); };
    TestEqual(TEXT("Empty"), Hex(""), FString(TEXT("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
    TestEqual(TEXT("abc"), Hex("abc"), FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
    // Two blocks, so the length lands in the second.
    TestEqual(TEXT("56 bytes"), Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
        FString(TEXT("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1")));
    TArray<uint8> Million;
    Million.Init('a', 1000000);
    TestEqual(TEXT("A million a's"), GameUpdate::Sha256Hex(Million), FString(TEXT("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0")));
    const FString File = FPaths::ProjectSavedDir() / TEXT("Automation") / (FGuid::NewGuid().ToString() + TEXT(".bin"));
    TestTrue(TEXT("Write test file"), FFileHelper::SaveArrayToFile(Million, *File));
    TestEqual(TEXT("File hash matches"), GameUpdate::Sha256OfFile(File), GameUpdate::Sha256Hex(Million));
    IFileManager::Get().Delete(*File);
    TestTrue(TEXT("Missing file"), GameUpdate::Sha256OfFile(File).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameUpdateSubsystemTest, "OpenGameUpdate.Subsystem", Flags)
bool FGameUpdateSubsystemTest::RunTest(const FString&)
{
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / FGuid::NewGuid().ToString());
    FGameUpdateConfig Config = TestConfig();
    Config.InstallDir = Root / TEXT("Install");
    Config.DownloadDir = Root / TEXT("Download");
    IFileManager::Get().MakeDirectory(*Config.InstallDir, true);

    const TSharedRef<FFakeSource> Source = MakeShared<FFakeSource>();
    Source->Release.Version = TEXT("0.0.8");
    Source->Release.InstallerName = TEXT("Game-0.0.8-Windows-x64-Setup.exe");
    Source->Release.InstallerUrl = TEXT("https://example.com/setup.exe");
    Source->Release.InstallerSize = Source->Payload.Len();
    Source->Release.Sha256 = GameUpdate::Sha256Hex(TConstArrayView<uint8>(reinterpret_cast<const uint8*>(TCHAR_TO_UTF8(*Source->Payload)), Source->Payload.Len()));
    FString LaunchedPath, LaunchedArguments;
    auto* Updater = NewObject<UGameUpdateSubsystem>(NewObject<UGameInstance>());
    Updater->Configure(Config, Source, [&](const FString& Path, const FString& Arguments) { LaunchedPath = Path; LaunchedArguments = Arguments; return true; });
    int32 Changes = 0, Restarts = 0;
    Updater->OnChanged.AddLambda([&] { ++Changes; });
    Updater->OnReadyToRestart.AddLambda([&] { ++Restarts; });

    Source->bReleaseOk = false;
    Updater->Check();
    TestEqual(TEXT("Offline check fails"), Updater->GetState(), EGameUpdateState::Failed);
    TestEqual(TEXT("With the source's reason"), Updater->GetMessage(), FString(TEXT("offline")));
    TestFalse(TEXT("Nothing available"), Updater->IsUpdateAvailable());

    Source->bReleaseOk = true;
    Source->Release.Version = TEXT("0.0.7");
    Updater->Check();
    TestEqual(TEXT("Same version is up to date"), Updater->GetState(), EGameUpdateState::UpToDate);
    TestFalse(TEXT("Up to date cannot install"), Updater->CanInstall());

    Source->Release.Version = TEXT("0.0.8");
    Updater->Check();
    TestEqual(TEXT("Newer release is available"), Updater->GetState(), EGameUpdateState::Available);
    TestTrue(TEXT("Changes are announced"), Changes >= 6);
    TestEqual(TEXT("Can install on Windows only"), Updater->CanInstall(), bool(PLATFORM_WINDOWS));
    if (!PLATFORM_WINDOWS) { IFileManager::Get().DeleteDirectory(*Root, false, true); return true; }

    // A damaged download is deleted and never started.
    const FString Good = Source->Payload;
    Source->Payload = TEXT("tampered bytes");
    Updater->DownloadAndInstall();
    WaitForVerify(Updater);
    TestEqual(TEXT("Damaged download fails"), Updater->GetState(), EGameUpdateState::Failed);
    TestTrue(TEXT("Update is still available to retry"), Updater->IsUpdateAvailable());
    TestTrue(TEXT("Installer not started"), LaunchedPath.IsEmpty());
    TestFalse(TEXT("Damaged file deleted"), FPaths::FileExists(Config.DownloadDir / TEXT("0.0.8") / Source->Release.InstallerName));

    Source->Payload = Good;
    Updater->DownloadAndInstall();
    WaitForVerify(Updater);
    TestEqual(TEXT("Verified installer starts"), Updater->GetState(), EGameUpdateState::Installing);
    TestEqual(TEXT("Restart requested once"), Restarts, 1);
    TestTrue(TEXT("Installer is the download"), LaunchedPath.EndsWith(TEXT("0.0.8/Game-0.0.8-Windows-x64-Setup.exe")));
    FString InstallDir = Config.InstallDir;
    FPaths::MakePlatformFilename(InstallDir);
    TestEqual(TEXT("Installer targets this install"), LaunchedArguments, FString::Printf(TEXT("/SILENT /DIR=\"%s\""), *InstallDir));

    // Where the install folder cannot be written, the game offers the release page instead.
    Config.InstallDir.Empty();
    Updater->Configure(Config, Source);
    Updater->Check();
    TestTrue(TEXT("Still reports the update"), Updater->IsUpdateAvailable());
    TestFalse(TEXT("But cannot install it"), Updater->CanInstall());
    IFileManager::Get().DeleteDirectory(*Root, false, true);
    return true;
}

namespace
{
    struct FLiveResult { bool bDone = false, bOk = false; FGameUpdateRelease Release; FString Error; };
    class FWaitForLiveRelease : public IAutomationLatentCommand
    {
    public:
        FWaitForLiveRelease(FAutomationTestBase* InTest, TSharedRef<IGameUpdateSource> InSource, TSharedRef<FLiveResult> InResult)
            : Test(InTest), Source(InSource), Result(InResult) {}
        virtual bool Update() override
        {
            if (!Result->bDone && GetCurrentRunTime() < 60) return false;
            if (!Result->bDone) Source->Cancel();
            Test->TestTrue(TEXT("GitHub answered: ") + Result->Error, Result->bDone && Result->bOk);
            Test->TestTrue(TEXT("Release version reads"), GameUpdate::IsNewer(Result->Release.Version, TEXT("0.0.0")));
            Test->TestTrue(TEXT("Asset found"), Result->Release.HasInstaller());
            Test->TestEqual(TEXT("Checksum read through GitHub's redirect"), Result->Release.Sha256.Len(), 64);
            Test->AddInfo(FString::Printf(TEXT("Latest %s, %s, sha256 %s"), *Result->Release.Version,
                *Result->Release.InstallerName, *Result->Release.Sha256));
            return true;
        }
    private:
        FAutomationTestBase* Test;
        TSharedRef<IGameUpdateSource> Source;
        TSharedRef<FLiveResult> Result;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameUpdateLiveTest, "OpenGameUpdate.Live", Flags)
bool FGameUpdateLiveTest::RunTest(const FString&)
{
    // -OpenGameUpdateLive reads the project's repository; -OpenGameUpdateAsset=<name with {version}> checks another asset.
    if (!FParse::Param(FCommandLine::Get(), TEXT("OpenGameUpdateLive")))
    {
        AddInfo(TEXT("The live GitHub check requires -OpenGameUpdateLive; no network request made."));
        return true;
    }
    FGameUpdateConfig Config = FGameUpdateConfig::Load();
    FParse::Value(FCommandLine::Get(), TEXT("OpenGameUpdateAsset="), Config.InstallerAsset);
    if (!TestTrue(TEXT("Project has an update config"), Config.IsValid())) return false;
    const TSharedRef<IGameUpdateSource> Source = GameUpdate::MakeGitHubSource(Config);
    const TSharedRef<FLiveResult> Result = MakeShared<FLiveResult>();
    Source->FetchLatest([Result](bool bOk, const FGameUpdateRelease& Release, const FString& Error)
    {
        Result->bDone = true; Result->bOk = bOk; Result->Release = Release; Result->Error = Error;
    });
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForLiveRelease(this, Source, Result));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameUpdateConfigTest, "OpenGameUpdate.Config", Flags)
bool FGameUpdateConfigTest::RunTest(const FString&)
{
    FGameUpdateConfig C = TestConfig();
    TestTrue(TEXT("Test config is valid"), C.IsValid());
    TestEqual(TEXT("Asset name"), C.AssetName(TEXT("1.2.3")), FString(TEXT("Game-1.2.3-Windows-x64-Setup.exe")));
    C.InstallDir = TEXT("C:/Games/Game/");
    // A trailing backslash would escape the closing quote.
    TestEqual(TEXT("Install folder without trailing slash"), C.Arguments(),
        PLATFORM_WINDOWS ? FString(TEXT("/SILENT /DIR=\"C:\\Games\\Game\"")) : FString(TEXT("/SILENT /DIR=\"C:/Games/Game\"")));
    C.Repository = TEXT("no-slash");
    TestFalse(TEXT("Repository needs owner/name"), C.IsValid());
    const FGameUpdateConfig Loaded = FGameUpdateConfig::Load();
    TestTrue(TEXT("Editor never installs over the engine"), Loaded.InstallDir.IsEmpty());
    TestFalse(TEXT("Version from project settings"), Loaded.CurrentVersion.IsEmpty());
    return true;
}
#endif
