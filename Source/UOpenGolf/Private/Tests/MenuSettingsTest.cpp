// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "OpenGolfSettings.h"
#include "Play/OpenGolfHoleRound.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "FileHelpers.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "OpenFairwayBall.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "Framework/Application/SlateApplication.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuPreferencesTest,"UOpenGolf.Menu.PreferenceValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMenuPreferencesTest::RunTest(const FString&)
{
    GolfSettings::FPreferences P;
    TestEqual(TEXT("Stimp preserves existing default"),P.Stimp,10);
    TestEqual(TEXT("Auto tee preserves existing default"),P.AutoTee,3);
    P.Stimp=999;P.AutoTee=-7;P.GimmieFeet=11;P.HudPercent=0;P.SpeedUnits=9;P.Validate();
    TestEqual(TEXT("Stimp clamps"),P.Stimp,15);TestEqual(TEXT("Invalid tee time recovers"),P.AutoTee,3);
    TestEqual(TEXT("Invalid radius is off"),P.GimmieFeet,0);TestEqual(TEXT("Invalid scale recovers"),P.HudPercent,100);
    P.AutoTee=0;P.Stimp=8;P.Validate();TestEqual(TEXT("Manual persists validation"),P.AutoTee,0);
    for(int32 Radius:{0,3,5,8,10}){P.GimmieFeet=Radius;P.Validate();TestEqual(TEXT("Approved radius"),P.GimmieFeet,Radius);}
    TestEqual(TEXT("Square Home is the default provider"),P.Provider,0);
    TestEqual(TEXT("Default connector port"),P.OpenConnectPort,LaunchMonitorDefaultOpenConnectPort);
    P.Provider=7;P.OpenConnectPort=70000;P.OpenConnectLan=4;P.Validate();
    TestEqual(TEXT("Provider clamps"),P.Provider,1);TestEqual(TEXT("Invalid port recovers"),P.OpenConnectPort,LaunchMonitorDefaultOpenConnectPort);TestEqual(TEXT("LAN clamps"),P.OpenConnectLan,1);
    P.OpenConnectPort=5921;P.Validate();TestEqual(TEXT("Custom port kept"),P.OpenConnectPort,5921);
    TestEqual(TEXT("Time zone defaults to this computer's"),GolfSettings::FPreferences().TimeZone,0);
    P.TimeZone=99;P.Validate();TestEqual(TEXT("Unknown time zone recovers"),P.TimeZone,0);
    P.TimeZone=-1;P.Validate();TestEqual(TEXT("Negative time zone recovers"),P.TimeZone,0);
    {
        const TConstArrayView<GolfSettings::FTimeZoneChoice> Zones=GolfSettings::TimeZones();
        TestTrue(TEXT("Local is the first zone"),FString(Zones[0].Id).IsEmpty());
        // ICU reads an unknown zone ID as GMT, so a mistyped ID would show UTC times. In July every offered zone but
        // UTC is away from GMT, so each formats a fixed instant differently from UTC.
        const FDateTime July(2026,7,1,12,0,0);
        const FString InUtc=FText::AsDateTime(July,EDateTimeStyle::Medium,EDateTimeStyle::Short,TEXT("Etc/UTC")).ToString();
        TSet<FString> Ids;
        for(int32 I=1;I<Zones.Num();++I)
        {
            const FString Id=Zones[I].Id;
            TestFalse(FString::Printf(TEXT("%s has an ID"),Zones[I].Label),Id.IsEmpty());
            TestFalse(FString::Printf(TEXT("%s is listed once"),Zones[I].Label),Ids.Contains(Id));Ids.Add(Id);
            TestFalse(FString::Printf(TEXT("%s has a short name"),Zones[I].Label),FString(Zones[I].Short).IsEmpty());
            // Longer labels are clipped by the settings drop-down.
            TestTrue(FString::Printf(TEXT("%s fits the drop-down"),Zones[I].Label),FCString::Strlen(Zones[I].Label)<=14);
            if(Id!=TEXT("Etc/UTC"))
                TestNotEqual(FString::Printf(TEXT("%s converts from UTC"),Zones[I].Label),FText::AsDateTime(July,EDateTimeStyle::Medium,EDateTimeStyle::Short,Id).ToString(),InUtc);
        }
        P.TimeZone=Zones.Num()-1;P.Validate();TestEqual(TEXT("Last zone kept"),P.TimeZone,Zones.Num()-1);
    }
    auto* Monitor=NewObject<UOpenLaunchMonitorSubsystem>(NewObject<UGameInstance>());Monitor->SetSpinMode(ESquareSpinMode::Calculated);
    TestEqual(TEXT("Spin mode retained before connection"),Monitor->GetSpinMode(),ESquareSpinMode::Calculated);
    Monitor->SetSpinMode(ESquareSpinMode::Measured);TestEqual(TEXT("Measured selectable"),Monitor->GetSpinMode(),ESquareSpinMode::Measured);
    const FString File=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation")/(FGuid::NewGuid().ToString()+TEXT(".ini")));
    P.CalculatedSpin=1;P.AutoTee=0;P.GimmieFeet=8;P.TimeZone=4;TestTrue(TEXT("Write preferences"),P.WriteConfig(File));
    GConfig->UnloadFile(File);
    GolfSettings::FPreferences Loaded;Loaded.ReadConfig(File);
    TestTrue(TEXT("Preferences survive disk round trip"),P==Loaded);
    GConfig->UnloadFile(File);IFileManager::Get().Delete(*File);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuGimmieTest,"UOpenGolf.Menu.GimmieScoring",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMenuGimmieTest::RunTest(const FString&)
{
    for(float Radius:{3.f,5.f,8.f,10.f})
    {
        FOpenGolfHoleRound Round;Round.Start(4);Round.RecordStroke(false,false);
        TestFalse(TEXT("Outside radius"),Round.TryConcede(Radius*.3048f+.001f,Radius,true));
        TestFalse(TEXT("Off green"),Round.TryConcede(.1f,Radius,false));
        TestTrue(TEXT("Inclusive boundary"),Round.TryConcede(Radius*.3048f,Radius,true));
        TestEqual(TEXT("Shot plus conceded putt"),Round.Strokes,2);TestEqual(TEXT("One putt"),Round.Putts,1);
        TestTrue(TEXT("Separate conceded state"),Round.bConceded);
        TestFalse(TEXT("Duplicate cannot score"),Round.TryConcede(.1f,Radius,true));TestEqual(TEXT("Still two strokes"),Round.Strokes,2);
    }
    FOpenGolfHoleRound R;R.Start(4);TestFalse(TEXT("Off"),R.TryConcede(.1f,0,true));
    R.bPractice=true;TestFalse(TEXT("Practice cannot concede"),R.TryConcede(.1f,10,true));
    R.Start(4);R.RecordStroke(true,true);TestFalse(TEXT("Physical hole-out no extra stroke"),R.TryConcede(0,10,true));TestEqual(TEXT("Physical putt stays one"),R.Strokes,1);
    return true;
}
class FCheckMenuIsolation : public IAutomationLatentCommand
{
public:
    explicit FCheckMenuIsolation(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(GetCurrentRunTime()>60){Test->AddError(TEXT("Menu input isolation timed out"));return true;}
        UWorld* W=GEditor->PlayWorld;auto* M=W&&W->HasBegunPlay()?W->GetAuthGameMode<AUOpenGolfGameMode>():nullptr;
        auto* P=W?Cast<AUOpenGolfPlayerController>(W->GetFirstPlayerController()):nullptr;
        if(!M||!M->GetBall()||!P)return false;
        auto* Monitor=W->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>();
        if(Stage==0)
        {
            Before=M->GetBall()->GetShotNumber();P->OpenMainMenu();Test->TestTrue(TEXT("Menu opens"),P->IsMainMenuOpen());
            Test->TestFalse(TEXT("Aim blocked"),P->SetAimYaw(15));Test->TestFalse(TEXT("Forwarded gameplay key blocked"),P->ForwardUIKey(EKeys::H,true));
            Monitor->InjectReplayShotAt(0);Test->TestEqual(TEXT("Incoming shot rejected"),M->GetBall()->GetShotNumber(),Before);
            P->SetMainMenuOpen(false);Stage=1;return false;
        }
        Test->TestEqual(TEXT("Rejected shot not queued on resume"),M->GetBall()->GetShotNumber(),Before);
        Monitor->InjectReplayShotAt(0);Test->TestTrue(TEXT("Shots work after resume"),M->GetBall()->GetShotNumber()>Before);
        return true;
    }
private: FAutomationTestBase* Test;int32 Stage=0,Before=0;
};
class FCheckReturnToMainMenu : public IAutomationLatentCommand
{
public:
    explicit FCheckReturnToMainMenu(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(GetCurrentRunTime()>90){Test->AddError(TEXT("Return to main menu timed out"));return true;}
        UWorld* W=GEditor->PlayWorld;auto* M=W&&W->HasBegunPlay()?W->GetAuthGameMode<AUOpenGolfGameMode>():nullptr;
        auto* P=W?Cast<AUOpenGolfPlayerController>(W->GetFirstPlayerController()):nullptr;
        if(!M||!M->GetBall()||!P)return false;
        auto* Monitor=W->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>();
        if(Stage==0)
        {
            // Enter Training through the menu's own Escape handling, then pause.
            P->OpenMainMenu();Test->TestFalse(TEXT("Main menu before Training"),P->IsPauseMenu());
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0));
            Test->TestFalse(TEXT("Escape enters Training"),P->IsMainMenuOpen());
            P->OpenMainMenu();Test->TestTrue(TEXT("Escape in Training opens the pause menu"),P->IsPauseMenu());
            OldController=P;OldMonitor=Monitor;
            P->ReturnToMainMenu();Stage=1;return false;
        }
        if(P==OldController.Get())return false;
        Test->TestTrue(TEXT("Main menu open after returning"),P->IsMainMenuOpen());
        Test->TestFalse(TEXT("It is the main menu, not the pause menu"),P->IsPauseMenu());
        Test->TestTrue(TEXT("The launch monitor subsystem survives"),Monitor==OldMonitor.Get());
        return true;
    }
private: FAutomationTestBase* Test;int32 Stage=0;TWeakObjectPtr<AUOpenGolfPlayerController> OldController;TWeakObjectPtr<UOpenLaunchMonitorSubsystem> OldMonitor;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuReturnTest,"UOpenGolf.Menu.ReturnToMainMenu",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMenuReturnTest::RunTest(const FString&)
{
    if(!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"),false,true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCheckReturnToMainMenu>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuIsolationTest,"UOpenGolf.Menu.InputIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMenuIsolationTest::RunTest(const FString&)
{
    if(!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"),false,true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCheckMenuIsolation>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
