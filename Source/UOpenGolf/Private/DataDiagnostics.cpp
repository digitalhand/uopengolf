// SPDX-License-Identifier: MIT
#include "DataDiagnostics.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"

namespace GolfSettings
{
namespace
{
class FDiagnosticOutput final : public FOutputDevice
{
public:
    FCriticalSection Mutex;
    FString Directory, File;
    int32 Level = 0;
    virtual bool CanBeUsedOnAnyThread() const override { return true; }
    virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        FScopeLock Lock(&Mutex);
        const FString Name = Category.ToString();
        if (!(Name.StartsWith(TEXT("LogUOpenGolf")) || Name.StartsWith(TEXT("LogOpen")) || Name.StartsWith(TEXT("LogSquare")))) return;
        const int32 V = int32(Verbosity) & int32(ELogVerbosity::VerbosityMask);
        const int32 Max = Level == 1 ? int32(ELogVerbosity::Error) : Level == 2 ? int32(ELogVerbosity::Warning) : Level == 3 ? int32(ELogVerbosity::Log) : int32(ELogVerbosity::VeryVerbose);
        if (!Level || V > Max || File.IsEmpty()) return;
        const FString Text(Message);
        for (const TCHAR* Secret : {TEXT("access_token"), TEXT("refresh_token"), TEXT("id_token"), TEXT("Bearer "), TEXT("Authorization:"), TEXT("client_secret")})
            if (Text.Contains(Secret)) return;
        FFileHelper::SaveStringToFile(FDateTime::UtcNow().ToIso8601() + TEXT(" ") + Name + TEXT(": ") + Text + LINE_TERMINATOR,
            *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
    }
};
TUniquePtr<FDiagnosticOutput> Output;
}
void ConfigureDiagnostics(const FPreferences& P)
{
    if (!GLog) return;
    // Drain messages under the old configuration before changing their destination or severity filter.
    GLog->FlushThreadedLogs();
    if (!Output) { Output = MakeUnique<FDiagnosticOutput>(); GLog->AddOutputDevice(Output.Get()); }
    if (GEngine && Output->Level != P.DiagnosticLevel)
        for (const TCHAR* Category : {TEXT("LogUOpenGolfRecording"), TEXT("LogUOpenGolfGameMode"), TEXT("LogOpenLaunchMonitor"), TEXT("LogOpenFairway")})
            GEngine->Exec(nullptr, *FString::Printf(TEXT("log %s %s"), Category, P.DiagnosticLevel == 4 ? TEXT("VeryVerbose") : TEXT("Log")));
    FScopeLock Lock(&Output->Mutex);
    const FString Directory = DiagnosticPath(P);
    if (Output->Directory != Directory)
    {
        Output->Directory = Directory;
        IFileManager::Get().MakeDirectory(*Directory, true);
        Output->File = Directory / (TEXT("UOpenGolf-diagnostics-") + FGuid::NewGuid().ToString() + TEXT(".log"));
    }
    Output->Level = P.DiagnosticLevel;
}
void ShutdownDiagnostics()
{
    if (Output && GLog) GLog->RemoveOutputDevice(Output.Get());
    Output.Reset();
}
}
