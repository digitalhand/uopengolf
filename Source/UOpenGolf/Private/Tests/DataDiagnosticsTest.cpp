// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DataDiagnostics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/OutputDeviceRedirector.h"
#include "HAL/FileManager.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataDiagnosticsTest,"UOpenGolf.Menu.DataDiagnostics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDataDiagnosticsTest::RunTest(const FString&)
{
    GolfSettings::FPreferences P;
    P.DiagnosticDirectory = FPaths::ProjectSavedDir() / TEXT("Automation/DiagnosticTest") / FGuid::NewGuid().ToString();
    P.RecordingDirectory = P.DiagnosticDirectory / TEXT("Recordings");
    TestTrue(TEXT("Writable directories accepted"),GolfSettings::ValidateDataDirectories(P).IsEmpty());
    P.DiagnosticLevel = 2;
    GolfSettings::ConfigureDiagnostics(P);
    const FName Category(TEXT("LogUOpenGolfTest"));
    GLog->Serialize(TEXT("retained-warning"),ELogVerbosity::Warning,Category);
    GLog->Serialize(TEXT("filtered-info"),ELogVerbosity::Log,Category);
    GLog->Serialize(TEXT("access_token private"),ELogVerbosity::Warning,Category);
    P.DiagnosticLevel = 0; GolfSettings::ConfigureDiagnostics(P);
    GLog->Serialize(TEXT("filtered-off"),ELogVerbosity::Warning,Category);
    P.DiagnosticLevel = 4; GolfSettings::ConfigureDiagnostics(P);
    GLog->Serialize(TEXT("retained-debug"),ELogVerbosity::Verbose,Category);
    GLog->FlushThreadedLogs();
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files,*(P.DiagnosticDirectory / TEXT("*.log")),true,false);
    FString Contents;
    if (TestEqual(TEXT("One diagnostic file"),Files.Num(),1))
        FFileHelper::LoadFileToString(Contents,*(P.DiagnosticDirectory / Files[0]));
    TestTrue(TEXT("Warning saved"),Contents.Contains(TEXT("retained-warning")));
    TestTrue(TEXT("Debug saved"),Contents.Contains(TEXT("retained-debug")));
    TestFalse(TEXT("Info filtered"),Contents.Contains(TEXT("filtered-info")));
    TestFalse(TEXT("Off filtered"),Contents.Contains(TEXT("filtered-off")));
    TestFalse(TEXT("Credentials excluded"),Contents.Contains(TEXT("private")));
    GolfSettings::ConfigureDiagnostics(GolfSettings::Get());
    IFileManager::Get().DeleteDirectory(*P.DiagnosticDirectory,false,true);
    return true;
}
#endif
