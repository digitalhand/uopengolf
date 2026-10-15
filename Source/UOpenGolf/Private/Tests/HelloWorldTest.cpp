// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UOpenGolfGameMode.h"
#include "UOpenGolfHUD.h"

#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyUOpenGolfHelloWorld, FAutomationTestBase*, Test);

bool FVerifyUOpenGolfHelloWorld::Update()
{
    UWorld* World = GEditor->PlayWorld;
    if (!World || !World->HasBegunPlay())
    {
        if (GetCurrentRunTime() > 30.0)
        {
            Test->AddError(TEXT("HelloWorld Play In Editor did not start within 30 seconds."));
            return true;
        }
        return false;
    }

    const APlayerController* Controller = World->GetFirstPlayerController();
    const AUOpenGolfHUD* HUD = Controller ? Cast<AUOpenGolfHUD>(Controller->GetHUD()) : nullptr;
    if (!HUD && GetCurrentRunTime() < 30.0)
    {
        return false;
    }
    Test->TestEqual(TEXT("Startup map"), UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()),
        FString(TEXT("/Game/UOpenGolf/Maps/HelloWorld")));
    Test->TestNotNull(TEXT("Configured game mode"), World->GetAuthGameMode<AUOpenGolfGameMode>());
    if (Test->TestNotNull(TEXT("Spawned hello-world HUD"), HUD))
    {
        Test->TestEqual(TEXT("Greeting"), HUD->GetGreeting().ToString(), FString(TEXT("Hello, Unreal Open Golf")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOpenGolfHelloWorldTest, "UOpenGolf.Smoke.HelloWorld",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUOpenGolfHelloWorldTest::RunTest(const FString& Parameters)
{
    const FString StartupMap = UGameMapsSettings::GetGameDefaultMap();
    if (!TestEqual(TEXT("Configured game startup map"), StartupMap, FString(TEXT("/Game/UOpenGolf/Maps/HelloWorld"))))
    {
        return false;
    }
    TestEqual(TEXT("Configured editor startup map"), GetDefault<UGameMapsSettings>()->EditorStartupMap.GetLongPackageName(), StartupMap);
    if (!FEditorFileUtils::LoadMap(StartupMap, false, true))
    {
        AddError(TEXT("Could not load the committed HelloWorld map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyUOpenGolfHelloWorld(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
