// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "OpenFairwayBall.h"
#include "OpenFairway/GolfPhysics.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfHUD.h"
#include "UOpenGolfRange.h"

#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyUOpenGolfStartup, FAutomationTestBase*, Test);

bool FVerifyUOpenGolfStartup::Update()
{
    UWorld* World = GEditor->PlayWorld;
    if (!World || !World->HasBegunPlay())
    {
        if (GetCurrentRunTime() > 30.0)
        {
            Test->AddError(TEXT("UOpenGolf Play In Editor did not start within 30 seconds."));
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
        FString(TEXT("/Game/UOpenGolf/Maps/UOpenGolf")));
    const AUOpenGolfGameMode* Mode = World->GetAuthGameMode<AUOpenGolfGameMode>();
    Test->TestNotNull(TEXT("Spawned HUD"), HUD);
    if (Test->TestNotNull(TEXT("Configured game mode"), Mode))
    {
        Test->TestNotNull(TEXT("Spawned range"), Mode->GetRange());
        const AOpenFairwayBall* Ball = Mode->GetBall();
        if (Test->TestNotNull(TEXT("Spawned ball"), Ball) && Mode->GetRange())
        {
            // Teed up: resting on the ground at the tee, one ball radius above it.
            const FVector Offset = Ball->GetActorLocation() - Mode->GetRange()->GetTeeLocation();
            Test->TestFalse(TEXT("Ball at rest"), Ball->IsMoving());
            Test->TestEqual(TEXT("Ball on the tee"), Offset.Size2D(), 0.0, 1.0);
            Test->TestEqual(TEXT("Ball resting on the ground"), Offset.Z, static_cast<double>(OpenFairway::GolfPhysics::Radius * 100.0f), 0.5);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOpenGolfStartupTest, "UOpenGolf.Smoke.UOpenGolf",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUOpenGolfStartupTest::RunTest(const FString& Parameters)
{
    const FString StartupMap = UGameMapsSettings::GetGameDefaultMap();
    if (!TestEqual(TEXT("Configured game startup map"), StartupMap, FString(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"))))
    {
        return false;
    }
    TestEqual(TEXT("Configured editor startup map"), GetDefault<UGameMapsSettings>()->EditorStartupMap.GetLongPackageName(), StartupMap);
    if (!FEditorFileUtils::LoadMap(StartupMap, false, true))
    {
        AddError(TEXT("Could not load the committed UOpenGolf map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyUOpenGolfStartup(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
