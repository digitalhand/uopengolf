// SPDX-License-Identifier: MIT
// The play keys as Enhanced Input actions: every key is mapped, and a key handed over by a focused HUD panel acts as it
// would from the viewport, including a held aim key.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "Play/OpenGolfPlayRules.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"

class FVerifyInputBindings : public IAutomationLatentCommand
{
public:
    explicit FVerifyInputBindings(FAutomationTestBase* InTest) : Test(InTest) {}

    virtual bool Update() override
    {
        if (GetCurrentRunTime() > 60.0)
        {
            Test->AddError(FString::Printf(TEXT("Timed out at stage %d."), Stage));
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        auto* Controller = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
        if (!Mode || !Mode->GetBall() || !Controller || !Controller->GetKeyMapping())
            return false;
        ++Frames;

        switch (Stage)
        {
        case 0:
        {
            const FKey Keys[] = {EKeys::C, EKeys::T, EKeys::H, EKeys::P, EKeys::Tab, EKeys::R, EKeys::G, EKeys::B, EKeys::W,
                EKeys::LeftMouseButton, EKeys::RightMouseButton, EKeys::F9, EKeys::Escape, EKeys::A, EKeys::D, EKeys::LeftShift,
                EKeys::RightShift};
            for (const FKey& Key : Keys)
                Test->TestTrue(Key.ToString() + TEXT(" is a play key"), Controller->GetKeyMapping()->GetMappings().ContainsByPredicate(
                                                                         [&Key](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Key == Key; }));
            Test->TestFalse(TEXT("A key no action uses is left to the panel"), Controller->ForwardUIKey(EKeys::Z, true));
            Controller->SetWindPanelOpen(false);
            Test->TestTrue(TEXT("W is handed to the controller"), Controller->ForwardUIKey(EKeys::W, true));
            Frames = 0;
            ++Stage;
            return false;
        }
        case 1:
            // Enhanced Input evaluates the key on the controller's next tick.
            if (!Controller->IsWindPanelOpen() && Frames < 10)
                return false;
            Test->TestTrue(TEXT("A forwarded W opens the wind controls"), Controller->IsWindPanelOpen());
            Controller->ForwardUIKey(EKeys::W, false);
            Controller->SetWindPanelOpen(false);
            Test->TestTrue(TEXT("The ball is at address"), Controller->SetAimYaw(0.0f));
            AimBefore = Controller->GetAimYaw();
            Controller->ForwardUIKey(EKeys::D, true);
            HoldStart = World->GetTimeSeconds();
            ++Stage;
            return false;
        case 2:
            if (World->GetTimeSeconds() - HoldStart < 0.5)
                return false;
            Controller->ForwardUIKey(EKeys::D, false);
            AimHeld = Controller->GetAimYaw();
            Test->AddInfo(FString::Printf(TEXT("Holding D for %.2f s turned the aim %.2f deg."), World->GetTimeSeconds() - HoldStart, AimHeld - AimBefore));
            Test->TestTrue(TEXT("Holding D turns further than one tap"), AimHeld > AimBefore + OpenGolfPlay::AimTapStepDeg + 0.01f);
            Frames = 0;
            ++Stage;
            return false;
        case 3:
            // The release reaches the controller on its next tick; after that the aim holds still.
            if (Frames < 3)
                return false;
            AimReleased = Controller->GetAimYaw();
            Frames = 0;
            ++Stage;
            return false;
        case 4:
            if (Frames < 5)
                return false;
            Test->TestEqual(TEXT("Releasing D stops the turn"), Controller->GetAimYaw(), AimReleased);
            Controller->SetAimYaw(0.0f);
            return true;
        default:
            return true;
        }
    }

private:
    FAutomationTestBase* Test;
    int32 Stage = 0;
    int32 Frames = 0;
    double HoldStart = 0.0;
    float AimBefore = 0.0f;
    float AimHeld = 0.0f;
    float AimReleased = 0.0f;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputBindingsTest, "UOpenGolf.Range.InputBindings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputBindingsTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FVerifyInputBindings>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
