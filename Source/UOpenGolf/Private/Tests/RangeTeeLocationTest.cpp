// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Misc/AutomationTest.h"
#include "OpenGolfTerrain.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfRange.h"

namespace RangeTeeLocationTestDetail
{
    // During Play the tee's ground is found once and reused, but a moved range finds it again.
    class FVerifyTeeLocation : public IAutomationLatentCommand
    {
    public:
        explicit FVerifyTeeLocation(FAutomationTestBase* InTest)
            : Test(InTest) {}

        virtual bool Update() override
        {
            UWorld* World = GEditor->PlayWorld;
            auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
            AUOpenGolfRange* Range = Mode ? Mode->GetRange() : nullptr;
            if (!Range)
            {
                if (GetCurrentRunTime() > 30.0)
                {
                    Test->AddError(TEXT("Range did not start."));
                    return true;
                }
                return false;
            }
            if (!IsValid(Range->Terrain))
            {
                Test->AddInfo(TEXT("The range has no terrain; the tee is its origin and nothing is cached."));
                return true;
            }
            const FVector Origin = Range->GetActorLocation();
            const FVector Tee = Range->GetTeeLocation();
            FVector Ground, Normal;
            Test->TestTrue(TEXT("Tee is the ground under the range"), Range->Terrain->GroundAt(Origin, Ground, Normal) && Tee.Equals(Ground, 0.1));
            Test->TestTrue(TEXT("Repeated tee query is stable"), Range->GetTeeLocation().Equals(Tee, 0.01));

            // The saved range is Static and cannot move in Play; this throwaway Play copy is made Movable
            // so the cache's origin check is exercised.
            Range->GetRootComponent()->SetMobility(EComponentMobility::Movable);
            const FVector Moved = Origin + FVector(0.0, 500.0, 0.0);
            Test->TestTrue(TEXT("Range moved"), Range->SetActorLocation(Moved) && Range->GetActorLocation().Equals(Moved, 0.01));
            const FVector MovedTee = Range->GetTeeLocation();
            Test->TestTrue(TEXT("Moved range finds its new ground"), Range->Terrain->GroundAt(Moved, Ground, Normal) && MovedTee.Equals(Ground, 0.1));
            Test->TestEqual(TEXT("Moved tee follows the range"), MovedTee.Y, Tee.Y + 500.0, 1.0);
            Range->SetActorLocation(Origin);
            Test->TestTrue(TEXT("Tee returns with the range"), Range->GetTeeLocation().Equals(Tee, 0.1));
            return true;
        }

    private:
        FAutomationTestBase* Test;
    };
} // namespace RangeTeeLocationTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangeTeeLocationTest, "UOpenGolf.Range.TeeLocation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRangeTeeLocationTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<RangeTeeLocationTestDetail::FVerifyTeeLocation>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
