// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "OpenFairwayBall.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"

namespace
{
    struct FVariantResults
    {
        TArray<FOpenFairwayShotResult> Shots;
        TArray<FVector> Positions;
        UClass* BaselineClass = nullptr;
    };

    class FLoadVariantMap : public IAutomationLatentCommand
    {
    public:
        FLoadVariantMap(FAutomationTestBase* InTest, FString InMap)
            : Test(InTest), Map(MoveTemp(InMap)) {}
        bool Update() override
        {
            Test->TestTrue(TEXT("Range map loads"), FEditorFileUtils::LoadMap(Map, false, true));
            return true;
        }

    private:
        FAutomationTestBase* Test;
        FString Map;
    };

    class FCheckVariant : public IAutomationLatentCommand
    {
    public:
        FCheckVariant(FAutomationTestBase* InTest, TSharedRef<FVariantResults> InResults, bool bInEnhanced)
            : Test(InTest), Results(InResults), bEnhanced(bInEnhanced)
        {
            GrassVar = IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.Grass3D"));
            OriginalGrass = GrassVar->GetInt();
        }
        ~FCheckVariant() { GrassVar->Set(OriginalGrass, ECVF_SetByConsole); }
        bool Update() override
        {
            if (GetCurrentRunTime() > 100)
            {
                Test->AddError(TEXT("Variant validation timed out"));
                return true;
            }
            UWorld* World = GEditor->PlayWorld;
            auto* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
            auto* PC = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
            if (!Mode || !PC || !World->HasBegunPlay() || !Mode->GetRange() || !Mode->GetBall())
                return false;
            auto* Range = Mode->GetRange();
            if (GetCurrentRunTime() < ReadyAt)
                return false;
            if (Stage == 0)
            {
                if (bEnhanced)
                    Test->TestTrue(TEXT("Enhanced map uses a distinct range subclass"), Range->GetClass() != Results->BaselineClass);
                else
                    Results->BaselineClass = Range->GetClass();
                TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
                Range->GetLayoutComponents(Components);
                for (auto* Component : Components)
                {
                    Test->TestTrue(TEXT("Grass mesh resolves"), Component->GetStaticMesh() != nullptr);
                    Test->TestEqual(TEXT("Grass has no collision"), Component->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
                }
                UMaterialInterface* FairwayMaterial = nullptr;
                UMaterialInterface* RoughMaterial = nullptr;
                TArray<UStaticMeshComponent*> Pieces;
                Range->GetLayoutComponents(Pieces);
                for (auto* Piece : Pieces)
                {
                    if (Piece->GetName() == TEXT("FairwayFront"))
                        FairwayMaterial = Piece->GetMaterial(0);
                    else if (Piece->GetName() == TEXT("RoughLeft"))
                        RoughMaterial = Piece->GetMaterial(0);
                }
                if (bEnhanced)
                    Test->TestTrue(TEXT("Turf override stays off the rough"), FairwayMaterial != RoughMaterial);
                GrassVar->Set(1, ECVF_SetByConsole);
                ReadyAt = GetCurrentRunTime() + 0.5;
                Stage = 1;
            }
            else if (Stage == 1)
            {
                Count = Range->GetGrassInstanceCount();
                Test->TestTrue(TEXT("Grass populated"), Count > 0);
                GrassVar->Set(0, ECVF_SetByConsole);
                ReadyAt = GetCurrentRunTime() + 0.5;
                Stage = 2;
            }
            else if (Stage == 2)
            {
                Test->TestEqual(TEXT("Grass off clears every variant"), Range->GetGrassInstanceCount(), 0);
                GrassVar->Set(1, ECVF_SetByConsole);
                ReadyAt = GetCurrentRunTime() + 0.5;
                Stage = 3;
            }
            else if (Stage == 3)
            {
                Test->TestEqual(TEXT("Grass restored deterministically"), Range->GetGrassInstanceCount(), Count);
                PC->SelectReplayShot(ShotIndex == 0 ? 3 : 0);
                PC->InjectReplayShot();
                Test->TestTrue(TEXT("Replay launches"), Mode->GetBall()->IsMoving());
                Stage = 4;
            }
            else if (Stage == 4 && !Mode->GetBall()->IsMoving())
            {
                FOpenFairwayShotResult Shot;
                Test->TestTrue(TEXT("Replay completed"), Mode->GetBall()->GetShotResult(Shot) && Shot.bAtRest);
                const FVector Position = Mode->GetBall()->GetActorLocation();
                Test->AddInfo(FString::Printf(TEXT("%s shot %d: carry %.4f yd total %.4f yd, rest %s"),
                    bEnhanced ? TEXT("Enhanced") : TEXT("Baseline"), ShotIndex, Shot.CarryM * OpenFairway::YardsPerMeter,
                    Shot.TotalM * OpenFairway::YardsPerMeter, *Position.ToString()));
                if (bEnhanced && Results->Shots.IsValidIndex(ShotIndex))
                {
                    Test->TestEqual(TEXT("Carry unaffected by presentation"), Shot.CarryM, Results->Shots[ShotIndex].CarryM, 0.01f);
                    Test->TestEqual(TEXT("Total unaffected by presentation"), Shot.TotalM, Results->Shots[ShotIndex].TotalM, 0.01f);
                    Test->TestTrue(TEXT("Rest position unaffected"), Position.Equals(Results->Positions[ShotIndex], 1.0));
                }
                else if (!bEnhanced)
                {
                    Results->Shots.Add(Shot);
                    Results->Positions.Add(Position);
                }
                if (++ShotIndex == 2)
                    return true;
                Stage = 3;
            }
            return false;
        }

    private:
        FAutomationTestBase* Test;
        TSharedRef<FVariantResults> Results;
        bool bEnhanced;
        IConsoleVariable* GrassVar;
        int32 OriginalGrass, Stage = 0, Count = 0, ShotIndex = 0;
        double ReadyAt = 0;
    };
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangeVariantTest, "UOpenGolf.OptionalRange.Variant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRangeVariantTest::RunTest(const FString& Parameters)
{
    FString Map;
    if (!FParse::Value(FCommandLine::Get(), TEXT("RangeVariantMap="), Map))
    {
        AddInfo(TEXT("Optional content test not requested; supply -RangeVariantMap=/Plugin/Maps/Map to validate it."));
        return true;
    }
    auto Results = MakeShared<FVariantResults>();
    auto& Framework = FAutomationTestFramework::Get();
    for (bool bEnhanced : {false, true})
    {
        Framework.EnqueueLatentCommand(MakeShared<FLoadVariantMap>(this, bEnhanced ? Map : TEXT("/Game/UOpenGolf/Maps/UOpenGolf")));
        ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
        Framework.EnqueueLatentCommand(MakeShared<FCheckVariant>(this, Results, bEnhanced));
        ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    }
    return true;
}
#endif
