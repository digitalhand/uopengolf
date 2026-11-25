// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Async/TaskGraphInterfaces.h"
#include "Engine/GameInstance.h"
#include "GameUpdateFormat.h"
#include "GameUpdateHash.h"
#include "GameUpdateInstall.h"
#include "GameUpdateRelease.h"
#include "GameUpdateSource.h"
#include "GameUpdateSubsystem.h"
#include "GameUpdateVersion.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#if PLATFORM_UNIX
#include <sys/stat.h>
#endif

namespace
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    FGameUpdateConfig TestConfig()
    {
        FGameUpdateConfig C;
        C.Repository = TEXT("owner/game");
        C.InstallerAsset = TEXT("Game-{version}-Windows-x64-Setup.exe");
        C.InstallerArguments = TEXT("/SILENT /DIR=\"{installdir}\"");
        // A fixed version, not the project's: the tests offer 0.0.7 as the same release and 0.0.8 as a newer one.
        C.CurrentVersion = TEXT("0.0.7");
        C.UserAgent = TEXT("Game/0.0.7");
        C.InstallKind = EGameUpdateInstallKind::Installer;
        return C;
    }

    /** Answers from memory: a release, and downloads that write Files[Url], or Payload for any other URL. */
    struct FFakeSource final : IGameUpdateSource
    {
        bool bReleaseOk = true;
        FGameUpdateRelease Release;
        FString Payload = TEXT("installer bytes");
        TMap<FString, FString> Files;
        /** Every URL downloaded, in order. */
        TArray<FString> Downloads;
        int32 Cancels = 0;
        virtual void FetchLatest(FOnRelease OnDone) override { OnDone(bReleaseOk, Release, bReleaseOk ? FString() : TEXT("offline")); }
        virtual void Download(const FString& Url, const FString& Path, FOnProgress OnProgress, FOnDone OnDone) override
        {
            Downloads.Add(Url);
            const FString* Text = Files.Find(Url);
            FFileHelper::SaveStringToFile(Text ? *Text : Payload, *Path);
            OnProgress((Text ? *Text : Payload).Len());
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
                  {"name":"Game-0.0.8-Windows-x64-Setup.exe.sha256","browser_download_url":"https://example.com/setup.exe.sha256","size":99},
                  {"name":"SHA256SUMS","browser_download_url":"https://example.com/SHA256SUMS","size":400},
                  {"name":"SHA256SUMS.asc","browser_download_url":"https://example.com/SHA256SUMS.asc","size":200}]})");
    TestTrue(TEXT("Release reads"), GameUpdate::ParseGitHubRelease(Json, Config, R, Error));
    TestFalse(TEXT("Signed list ignored unless configured"), R.IsSigned());
    FGameUpdateConfig Signed = Config;
    Signed.ChecksumListAsset = TEXT("SHA256SUMS");
    Signed.ChecksumSignatureAsset = TEXT("SHA256SUMS.asc");
    FGameUpdateRelease SignedRelease;
    TestTrue(TEXT("Signed release reads"), GameUpdate::ParseGitHubRelease(Json, Signed, SignedRelease, Error));
    TestEqual(TEXT("Checksum list URL"), SignedRelease.SumsUrl, FString(TEXT("https://example.com/SHA256SUMS")));
    TestEqual(TEXT("Signature URL"), SignedRelease.SumsSignatureUrl, FString(TEXT("https://example.com/SHA256SUMS.asc")));
    TestTrue(TEXT("Signed"), SignedRelease.IsSigned());
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
    TestTrue(TEXT("A configured installer can install"), Updater->CanInstall());

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
#if PLATFORM_UNIX
    struct stat Info;
    TestTrue(TEXT("Download folder is private"), stat(TCHAR_TO_UTF8(*Config.DownloadDir), &Info) == 0 && (Info.st_mode & 0777) == 0700);
#endif
    FString InstallDir = Config.InstallDir;
    FPaths::MakePlatformFilename(InstallDir);
    TestEqual(TEXT("Installer targets this install"), LaunchedArguments, FString::Printf(TEXT("/SILENT /DIR=\"%s\""), *InstallDir));

    // Each missing piece is named, so the dialog and the log say why the release page is offered instead.
    const FGameUpdateRelease Complete = Source->Release;
    Updater->Configure(Config, Source);
    Updater->Check();
    TestTrue(TEXT("Nothing blocks a complete release"), Updater->GetInstallBlocker().IsEmpty());
    Source->Release.Sha256.Empty();
    Updater->Check();
    TestEqual(TEXT("A missing checksum is named"), Updater->GetInstallBlocker(), FString(TEXT("The installer's checksum couldn't be read.")));
    TestFalse(TEXT("And blocks the install"), Updater->CanInstall());
    Source->Release.InstallerUrl.Empty();
    Updater->Check();
    TestEqual(TEXT("A missing installer is named"), Updater->GetInstallBlocker(), FString(TEXT("This release has no installer.")));
    Source->Release = Complete;

    // Where the install folder cannot be written, the game offers the release page instead.
    Config.InstallDir.Empty();
    Updater->Configure(Config, Source);
    Updater->Check();
    TestTrue(TEXT("Still reports the update"), Updater->IsUpdateAvailable());
    TestFalse(TEXT("But cannot install it"), Updater->CanInstall());
    TestEqual(TEXT("An unwritable folder is named"), Updater->GetInstallBlocker(), FString(TEXT("The game's folder can't be written to.")));
    Config.InstallKind = EGameUpdateInstallKind::None;
    Updater->Configure(Config, Source);
    Updater->Check();
    TestEqual(TEXT("A copy without an install kind is named"), Updater->GetInstallBlocker(),
        FString(TEXT("Updating from the game isn't available for this copy.")));
    IFileManager::Get().DeleteDirectory(*Root, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameUpdateLinuxHelperTest, "OpenGameUpdate.LinuxHelper", Flags)
bool FGameUpdateLinuxHelperTest::RunTest(const FString&)
{
    // The subsystem's side of a Linux update, on any platform: what it checks and what it starts. The helper script
    // itself is tested by Assets/Scripts/test_apply_update.py.
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / FGuid::NewGuid().ToString());
    const FString Tools = Root / TEXT("Tools");
    FGameUpdateConfig Config = TestConfig();
    Config.InstallKind = EGameUpdateInstallKind::Archive;
    Config.InstallerAsset = TEXT("Game-{version}-Linux-x64.tar.gz");
    Config.InstallDir = Root / TEXT("Games/Game") + TEXT("/");
    Config.DownloadDir = Root / TEXT("Download");
    Config.UpdateHelper = Tools / TEXT("apply-update.sh");
    Config.Pkexec = Tools / TEXT("pkexec");
    Config.PackageInstaller = Tools / TEXT("install-update");
    IFileManager::Get().MakeDirectory(*Config.InstallDir, true);

    const TSharedRef<FFakeSource> Source = MakeShared<FFakeSource>();
    Source->Release.Version = TEXT("0.0.8");
    Source->Release.InstallerName = TEXT("Game-0.0.8-Linux-x64.tar.gz");
    Source->Release.InstallerUrl = TEXT("https://example.com/game.tar.gz");
    Source->Release.Sha256 = GameUpdate::Sha256Hex(TConstArrayView<uint8>(reinterpret_cast<const uint8*>(TCHAR_TO_UTF8(*Source->Payload)), Source->Payload.Len()));
    FString Program, Arguments;
    auto* Updater = NewObject<UGameUpdateSubsystem>(NewObject<UGameInstance>());
    const UGameUpdateSubsystem::FLaunchInstaller Launcher = [&](const FString& InProgram, const FString& InArguments)
    {
        Program = InProgram; Arguments = InArguments; return true;
    };

    Updater->Configure(Config, Source, Launcher);
    Updater->Check();
    TestEqual(TEXT("A missing helper is named"), Updater->GetInstallBlocker(), FString(TEXT("The game's update helper is missing. Reinstall the game.")));
    for (const FString& Tool : { Config.UpdateHelper, Config.Pkexec, Config.PackageInstaller })
        FFileHelper::SaveStringToFile(TEXT("#!/bin/sh\n"), *Tool);

    Updater->Configure(Config, Source, Launcher);
    Updater->Check();
    TestTrue(TEXT("A portable copy can install"), Updater->CanInstall());
    TestEqual(TEXT("Portable kind"), Updater->GetInstallKind(), EGameUpdateInstallKind::Archive);
    Updater->DownloadAndInstall();
    WaitForVerify(Updater);
    TestEqual(TEXT("Helper started"), Updater->GetState(), EGameUpdateState::Installing);
    TestEqual(TEXT("Through /bin/sh"), Program, FString(TEXT("/bin/sh")));
    const FString File = Config.DownloadDir / TEXT("0.0.8/Game-0.0.8-Linux-x64.tar.gz");
    TestTrue(TEXT("Helper, mode and download"), Arguments.StartsWith(FString::Printf(
        TEXT("\"%s\" --pid %u --mode archive --file \"%s\" --installdir \"%s\" --version \"0.0.8\" --result \"%s\" -- \""),
        *Config.UpdateHelper, FPlatformProcess::GetCurrentProcessId(), *File, *(Root / TEXT("Games/Game")), *(Config.DownloadDir / TEXT("result")))));
    TestTrue(TEXT("Restarts this program"), Arguments.Contains(FString::Printf(TEXT("-- \"%s\""), FPlatformProcess::ExecutablePath())));

    // A system package: the package asset, and pkexec with the root installer and the release's signed list.
    Config.InstallKind = EGameUpdateInstallKind::Package;
    Config.PackageFormat = TEXT("rpm");
    Config.PackageAsset = TEXT("Game-{version}-Linux-x86_64.rpm");
    Config.ChecksumListAsset = TEXT("SHA256SUMS");
    Config.ChecksumSignatureAsset = TEXT("SHA256SUMS.asc");
    TestEqual(TEXT("Package asset"), Config.AssetName(TEXT("0.0.8")), FString(TEXT("Game-0.0.8-Linux-x86_64.rpm")));
    Source->Release.InstallerName = TEXT("Game-0.0.8-Linux-x86_64.rpm");
    Updater->Configure(Config, Source, Launcher);
    Updater->Check();
    TestEqual(TEXT("An unsigned release can't install as root"), Updater->GetInstallBlocker(),
        FString(TEXT("This release isn't signed, so it can't be installed as administrator.")));

    Source->Release.SumsUrl = TEXT("https://example.com/SHA256SUMS");
    Source->Release.SumsSignatureUrl = TEXT("https://example.com/SHA256SUMS.asc");
    Source->Files.Add(Source->Release.SumsUrl, Source->Release.Sha256 + TEXT("  Game-0.0.8-Linux-x64.tar.gz\n"));
    Source->Files.Add(Source->Release.SumsSignatureUrl, TEXT("signature"));
    Updater->Configure(Config, Source, Launcher);
    Updater->Check();
    TestTrue(TEXT("A signed package release can install"), Updater->CanInstall());
    Arguments.Empty();
    Source->Downloads.Empty();
    Updater->DownloadAndInstall();
    WaitForVerify(Updater);
    TestEqual(TEXT("A list that doesn't name the package fails"), Updater->GetMessage(),
        FString(TEXT("The release's signed checksum list doesn't match its download.")));
    TestEqual(TEXT("...before the package downloads"), Source->Downloads.Num(), 2);
    TestTrue(TEXT("...and nothing starts"), Arguments.IsEmpty());

    Source->Files.Add(Source->Release.SumsUrl, Source->Release.Sha256 + TEXT("  Game-0.0.8-Linux-x86_64.rpm\n"));
    Source->Downloads.Empty();
    Updater->DownloadAndInstall();
    WaitForVerify(Updater);
    TestEqual(TEXT("Package helper started"), Updater->GetState(), EGameUpdateState::Installing);
    TestEqual(TEXT("Signed list, signature, then the package"), FString::Join(Source->Downloads, TEXT(" ")),
        FString::Join(TArray<FString>{ Source->Release.SumsUrl, Source->Release.SumsSignatureUrl, Source->Release.InstallerUrl }, TEXT(" ")));
    const FString Version = Config.DownloadDir / TEXT("0.0.8");
    TestTrue(TEXT("Package mode elevates the root installer with the signed list"), Arguments.Contains(FString::Printf(
        TEXT("--mode package --file \"%s\" --installdir \"%s\" --version \"0.0.8\" --pkexec \"%s\" --elevate \"%s\" ")
        TEXT("--sums \"%s\" --signature \"%s\" --result"), *(Version / TEXT("Game-0.0.8-Linux-x86_64.rpm")),
        *(Root / TEXT("Games/Game")), *Config.Pkexec, *Config.PackageInstaller, *(Version / TEXT("SHA256SUMS")),
        *(Version / TEXT("SHA256SUMS.asc")))));

    // An update from an earlier start that is still installing keeps its download.
    FFileHelper::SaveStringToFile(LexToString(FPlatformProcess::GetCurrentProcessId()), *(Config.DownloadDir / GameUpdate::HelperPidFile));
    TestFalse(TEXT("This game is not its own helper"), GameUpdate::IsHelperRunning(Config.DownloadDir));

    IFileManager::Get().Delete(*Config.Pkexec);
    Updater->Configure(Config, Source, Launcher);
    Updater->Check();
    TestEqual(TEXT("Missing pkexec is named"), Updater->GetInstallBlocker(),
        FString(TEXT("pkexec isn't installed, so the update can't ask for the administrator password.")));
    IFileManager::Get().DeleteDirectory(*Root, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameUpdateInstallTest, "OpenGameUpdate.Install", Flags)
bool FGameUpdateInstallTest::RunTest(const FString&)
{
    const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / FGuid::NewGuid().ToString());
    const FString Marker = Dir / GameUpdate::PackageFormatFile;
    IFileManager::Get().MakeDirectory(*Dir, true);
    TestTrue(TEXT("No marker, no package"), GameUpdate::ReadPackageFormat(Dir).IsEmpty());
    TestTrue(TEXT("No folder, no package"), GameUpdate::ReadPackageFormat(FString()).IsEmpty());
    const TPair<const TCHAR*, const TCHAR*> Markers[] = {
        { TEXT("deb\n"), TEXT("deb") }, { TEXT(" rpm \n"), TEXT("rpm") }, { TEXT("Deb"), TEXT("") }, { TEXT("../x"), TEXT("") },
        { TEXT(""), TEXT("") }, { TEXT("averyveryverylongformat"), TEXT("") },
    };
    for (const TPair<const TCHAR*, const TCHAR*>& Case : Markers)
    {
        FFileHelper::SaveStringToFile(Case.Key, *Marker);
        TestEqual(*FString::Printf(TEXT("Marker \"%s\""), Case.Key), GameUpdate::ReadPackageFormat(Dir + TEXT("/")), FString(Case.Value));
    }
    IFileManager::Get().DeleteDirectory(*Dir, false, true);

    using K = EGameUpdateInstallKind;
    TestEqual(TEXT("Editor"), GameUpdate::LinuxInstallKind(FString(), TEXT("deb"), TEXT("x")), K::None);
    TestEqual(TEXT("Package with an asset"), GameUpdate::LinuxInstallKind(TEXT("/opt/game"), TEXT("deb"), TEXT("x")), K::Package);
    TestEqual(TEXT("Package format without an asset"), GameUpdate::LinuxInstallKind(TEXT("/opt/game"), TEXT("pacman"), FString()), K::Archive);
    TestEqual(TEXT("Portable"), GameUpdate::LinuxInstallKind(TEXT("/home/p/Game"), FString(), FString()), K::Archive);

    FGameUpdateHelperLaunch Launch;
    Launch.Helper = TEXT("/opt/My Game/apply-update.sh");
    Launch.Kind = K::Package;
    Launch.File = TEXT("/home/p/.cache/GameUpdate/1.0.0/Game-1.0.0.deb");
    Launch.InstallDir = TEXT("/opt/My Game/");
    Launch.Version = TEXT("1.0.0");
    Launch.Pid = 42;
    Launch.Pkexec = TEXT("/usr/bin/pkexec");
    Launch.PackageInstaller = TEXT("/usr/lib/game/install-update");
    Launch.Sums = TEXT("/home/p/.cache/GameUpdate/1.0.0/SHA256SUMS");
    Launch.Signature = TEXT("/home/p/.cache/GameUpdate/1.0.0/SHA256SUMS.asc");
    Launch.ResultFile = TEXT("/home/p/.cache/GameUpdate/result");
    Launch.RestartProgram = TEXT("/opt/My Game/Game/Binaries/Linux/Game");
    Launch.RestartArguments = TEXT(" Game -SaveToUserDir \"-log=a b\" ");
    TestEqual(TEXT("Helper arguments"), GameUpdate::LinuxHelperArguments(Launch), FString(TEXT(
        "\"/opt/My Game/apply-update.sh\" --pid 42 --mode package --file \"/home/p/.cache/GameUpdate/1.0.0/Game-1.0.0.deb\" "
        "--installdir \"/opt/My Game\" --version \"1.0.0\" --pkexec \"/usr/bin/pkexec\" --elevate \"/usr/lib/game/install-update\" "
        "--sums \"/home/p/.cache/GameUpdate/1.0.0/SHA256SUMS\" --signature \"/home/p/.cache/GameUpdate/1.0.0/SHA256SUMS.asc\" "
        "--result \"/home/p/.cache/GameUpdate/result\" -- \"/opt/My Game/Game/Binaries/Linux/Game\" Game -SaveToUserDir \"-log=a b\"")));
    Launch.Kind = K::Archive;
    TestFalse(TEXT("Archives don't elevate"), GameUpdate::LinuxHelperArguments(Launch).Contains(TEXT("--elevate")));
    TestFalse(TEXT("Archives pass no signed list"), GameUpdate::LinuxHelperArguments(Launch).Contains(TEXT("--sums")));
    Launch.File = TEXT("/home/p/\"quoted\".tar.gz");
    TestTrue(TEXT("A quote can't be passed"), GameUpdate::LinuxHelperArguments(Launch).IsEmpty());

    TestTrue(TEXT("Success"), GameUpdate::ParseHelperFailure(TEXT("ok 1.0.0\n")).IsEmpty());
    TestTrue(TEXT("No result"), GameUpdate::ParseHelperFailure(TEXT("")).IsEmpty());
    TestEqual(TEXT("Failure reason"), GameUpdate::ParseHelperFailure(TEXT("failed The password prompt was cancelled.\n")),
        FString(TEXT("The password prompt was cancelled.")));
    TestEqual(TEXT("Unreadable result"), GameUpdate::ParseHelperFailure(TEXT("garbage")), FString(TEXT("The last update didn't finish.")));
    const FString Long = GameUpdate::ParseHelperFailure(TEXT("failed ") + FString::ChrN(500, TEXT('x')));
    TestEqual(TEXT("A long reason is shortened"), Long.Len(), GameUpdate::MaxFailureLength);
    TestTrue(TEXT("...visibly"), Long.EndsWith(TEXT("...")));

    TestEqual(TEXT("Trailing slash"), GameUpdate::WithoutTrailingSlash(TEXT("/opt/game//")), FString(TEXT("/opt/game")));
    TestEqual(TEXT("Root stays"), GameUpdate::WithoutTrailingSlash(TEXT("/")), FString(TEXT("/")));
    TestEqual(TEXT("No slash"), GameUpdate::WithoutTrailingSlash(TEXT("/opt/game")), FString(TEXT("/opt/game")));

    const FString PidDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / FGuid::NewGuid().ToString());
    const FString PidFile = PidDir / GameUpdate::HelperPidFile;
    TestFalse(TEXT("No helper.pid"), GameUpdate::IsHelperRunning(PidDir));
    for (const TCHAR* Text : { TEXT(""), TEXT("abc"), TEXT("-5"), TEXT("1.5"), TEXT("99999999999") })
    {
        FFileHelper::SaveStringToFile(Text, *PidFile);
        TestFalse(*FString::Printf(TEXT("helper.pid \"%s\""), Text), GameUpdate::IsHelperRunning(PidDir));
    }
#if PLATFORM_UNIX
    // init always runs.
    FFileHelper::SaveStringToFile(TEXT("1\n"), *PidFile);
    TestTrue(TEXT("A running helper"), GameUpdate::IsHelperRunning(PidDir));
#endif
    IFileManager::Get().DeleteDirectory(*PidDir, false, true);
    TestTrue(TEXT("Download folder in the player's cache"), GameUpdate::LinuxDownloadDir(TEXT("Game")).EndsWith(TEXT("/GameUpdate")));
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
    // Unreal's ini reader drops unquoted { and }, which turns {version} into the literal word "version".
    TestTrue(TEXT("Installer asset keeps {version}"), Loaded.InstallerAsset.Contains(TEXT("{version}")));
    TestTrue(TEXT("Installer arguments keep {installdir}"), Loaded.InstallerArguments.Contains(TEXT("{installdir}")));
    // Linux overrides, read on any host.
    FConfigFile Linux;
    TestTrue(TEXT("Linux game config loads"), FConfigCacheIni::LoadLocalIniFile(Linux, TEXT("Game"), true, TEXT("Linux")));
    for (const TCHAR* Key : { TEXT("InstallerAsset"), TEXT("PackageAsset_deb"), TEXT("PackageAsset_rpm") })
    {
        FString Value;
        TestTrue(*FString::Printf(TEXT("Linux %s keeps {version}"), Key), Linux.GetString(TEXT("OpenGameUpdate"), Key, Value) && Value.Contains(TEXT("{version}")));
    }
    FString PackageInstaller;
    TestTrue(TEXT("Linux package installer is absolute"), Linux.GetString(TEXT("OpenGameUpdate"), TEXT("PackageInstaller"), PackageInstaller) && PackageInstaller.StartsWith(TEXT("/")));
    return true;
}
#endif
