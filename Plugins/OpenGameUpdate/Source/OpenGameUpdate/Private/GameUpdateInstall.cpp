// SPDX-License-Identifier: MIT
#include "GameUpdateInstall.h"
#include "GameUpdateLog.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if PLATFORM_UNIX
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace GameUpdate
{
    FString WithoutTrailingSlash(FString Path)
    {
        while (Path.Len() > 1 && (Path.EndsWith(TEXT("/")) || Path.EndsWith(TEXT("\\")))) Path.LeftChopInline(1);
        return Path;
    }

    namespace
    {
        bool IsFormatWord(const FString& Word)
        {
            if (Word.IsEmpty() || Word.Len() > 16) return false;
            for (const TCHAR C : Word)
                if (!(FChar::IsDigit(C) || (C >= TEXT('a') && C <= TEXT('z')))) return false;
            return true;
        }

        /** Appends Value in double quotes; false when it holds a quote that would end the argument early. */
        bool AppendQuoted(FString& Out, const FString& Value)
        {
            if (Value.Contains(TEXT("\""))) return false;
            Out += TEXT(" \"") + Value + TEXT("\"");
            return true;
        }
    }

    FString ReadPackageFormat(const FString& InstallDir)
    {
        if (InstallDir.IsEmpty()) return {};
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *(WithoutTrailingSlash(InstallDir) / PackageFormatFile))) return {};
        Text.TrimStartAndEndInline();
        return IsFormatWord(Text) ? Text : FString();
    }

    EGameUpdateInstallKind LinuxInstallKind(const FString& InstallDir, const FString& PackageFormat, const FString& PackageAsset)
    {
        if (InstallDir.IsEmpty()) return EGameUpdateInstallKind::None;
        return !PackageFormat.IsEmpty() && !PackageAsset.IsEmpty() ? EGameUpdateInstallKind::Package : EGameUpdateInstallKind::Archive;
    }

    FString LinuxHelperArguments(const FGameUpdateHelperLaunch& Launch)
    {
        const bool bPackage = Launch.Kind == EGameUpdateInstallKind::Package;
        FString Out;
        bool bOk = AppendQuoted(Out, Launch.Helper);
        Out += FString::Printf(TEXT(" --pid %u --mode %s"), Launch.Pid, bPackage ? TEXT("package") : TEXT("archive"));
        const auto Option = [&Out, &bOk](const TCHAR* Name, const FString& Value)
        {
            Out += TEXT(" ") + FString(Name);
            bOk &= AppendQuoted(Out, Value);
        };
        Option(TEXT("--file"), Launch.File);
        Option(TEXT("--installdir"), WithoutTrailingSlash(Launch.InstallDir));
        Option(TEXT("--version"), Launch.Version);
        if (bPackage)
        {
            Option(TEXT("--pkexec"), Launch.Pkexec);
            Option(TEXT("--elevate"), Launch.PackageInstaller);
            Option(TEXT("--sums"), Launch.Sums);
            Option(TEXT("--signature"), Launch.Signature);
        }
        Option(TEXT("--result"), Launch.ResultFile);
        Out += TEXT(" --");
        bOk &= AppendQuoted(Out, Launch.RestartProgram);
        const FString Arguments = Launch.RestartArguments.TrimStartAndEnd();
        if (!Arguments.IsEmpty()) Out += TEXT(" ") + Arguments;
        return bOk ? Out.TrimStart() : FString();
    }

    FString ParseHelperFailure(const FString& ResultText)
    {
        TArray<FString> Lines;
        ResultText.ParseIntoArrayLines(Lines);
        const FString Line = Lines.IsEmpty() ? FString() : Lines[0].TrimStartAndEnd();
        if (Line.IsEmpty() || Line == TEXT("ok") || Line.StartsWith(TEXT("ok "))) return {};
        FString Reason = Line.StartsWith(TEXT("failed")) ? Line.RightChop(6).TrimStart() : FString();
        if (Reason.IsEmpty()) return TEXT("The last update didn't finish.");
        return Reason.Len() > MaxFailureLength ? Reason.Left(MaxFailureLength - 3) + TEXT("...") : Reason;
    }

    FString LinuxDownloadDir(const FString& Project)
    {
        FString Cache = FPlatformMisc::GetEnvironmentVariable(TEXT("XDG_CACHE_HOME"));
        // The XDG spec says to ignore a relative value.
        if (Cache.IsEmpty() || FPaths::IsRelative(Cache)) Cache = FPaths::Combine(FPlatformProcess::UserHomeDir(), TEXT(".cache"));
        return FPaths::Combine(Cache, Project + TEXT("Update"));
    }

    bool MakePrivateDirectory(const FString& Dir)
    {
        if (!IFileManager::Get().MakeDirectory(*Dir, true)) return false;
#if PLATFORM_UNIX
        struct stat Info;
        const FTCHARToUTF8 Path(*Dir);
        if (lstat(Path.Get(), &Info) != 0 || !S_ISDIR(Info.st_mode) || Info.st_uid != geteuid() || chmod(Path.Get(), 0700) != 0)
        {
            UE_LOG(LogGameUpdate, Warning, TEXT("The download folder %s isn't a private folder of this account."), *Dir);
            return false;
        }
#endif
        return true;
    }

    bool IsHelperRunning(const FString& DownloadDir)
    {
        FString Text;
        if (DownloadDir.IsEmpty() || !FFileHelper::LoadFileToString(Text, *(DownloadDir / HelperPidFile))) return false;
        Text.TrimStartAndEndInline();
        if (Text.IsEmpty() || Text.Len() > 10 || !Text.IsNumeric() || Text.Contains(TEXT("."))) return false;
        const int64 Pid = FCString::Atoi64(*Text);
        return Pid > 0 && Pid <= MAX_uint32 && Pid != FPlatformProcess::GetCurrentProcessId()
            && FPlatformProcess::IsApplicationRunning(uint32(Pid));
    }
}
