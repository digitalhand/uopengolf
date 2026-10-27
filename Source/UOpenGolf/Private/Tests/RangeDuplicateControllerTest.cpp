// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfRange.h"

namespace RangeDuplicateControllerTestDetail
{
    // Two range controllers are a map error, but Play still starts, using one of them.
    class FVerifyPlayStarts : public IAutomationLatentCommand
    {
    public:
        explicit FVerifyPlayStarts(FAutomationTestBase* InTest)
            : Test(InTest) {}

        virtual bool Update() override
        {
            UWorld* World = GEditor->PlayWorld;
            auto* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
            APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
            if (World && World->HasBegunPlay() && Mode && Mode->GetRange() && Mode->GetBall() && Controller)
            {
                Test->TestTrue(TEXT("Player controller began play"), Controller->HasActorBegunPlay());
                return true;
            }
            if (GetCurrentRunTime() > 30.0)
            {
                Test->AddError(TEXT("Play did not start with two range controllers in the map."));
                return true;
            }
            return false;
        }

    private:
        FAutomationTestBase* Test;
    };
} // namespace RangeDuplicateControllerTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangeDuplicateControllerTest, "UOpenGolf.Range.DuplicateController",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRangeDuplicateControllerTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!World)
    {
        AddError(TEXT("Could not create a blank map."));
        return false;
    }
    World->SpawnActor<AUOpenGolfRange>();
    World->SpawnActor<AUOpenGolfRange>();
    AddExpectedMessage(TEXT("multiple range controllers"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1, false);
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<RangeDuplicateControllerTestDetail::FVerifyPlayStarts>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
