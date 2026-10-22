// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"

// Verify the default range (fairway strip, rough sides, putting green) by its physical materials, then
// reconstruct the historical mixed surfaces with component tags in this disposable PIE world to retain
// the rollout comparison. Component tags override physical materials. The putting green stays out of
// the comparison (no collision) so the reference rollouts remain flat single-surface runs.
class FCompareRolloutSurfaces : public IAutomationLatentCommand
{
public:
    explicit FCompareRolloutSurfaces(FAutomationTestBase* InTest)
        : Test(InTest) {}

    virtual bool Update() override
    {
        if (GetCurrentRunTime() > 90.0)
        {
            Test->AddError(TEXT("Timed out comparing rollout surfaces."));
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        auto* Controller = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
        if (!World || !World->HasBegunPlay() || !Mode || !Mode->GetRange() || !Mode->GetBall() || !Controller)
        {
            return false;
        }
        if (!bLaunched)
        {
            TArray<UStaticMeshComponent*> Pieces;
            Mode->GetRange()->GetLayoutComponents(Pieces);
            for (UStaticMeshComponent* Piece : Pieces)
            {
                if (Case == 0)
                {
                    OriginalTags.Add(Piece->GetFName(), Piece->ComponentTags);
                    if (Piece->GetFName() == TEXT("PuttingGreen"))
                    {
                        Test->TestTrue(TEXT("PuttingGreen plays as green"), Mode->GetBall()->ResolveSurface(Piece) == EOpenFairwaySurface::Green);
                        OriginalGreenMesh = Piece->GetStaticMesh();
                        Piece->SetMobility(EComponentMobility::Movable);
                        // This test compares a flat calibration surface, without the raised putting green.
                        Piece->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
                        Piece->SetRelativeScale3D(FVector(30, 30, 1));
                        Piece->AddRelativeLocation(FVector(0, 0, -50));
                        Piece->ComponentTags.AddUnique(AOpenFairwayBall::GetSurfaceTag(EOpenFairwaySurface::Fairway));
                    }
                    else if (Piece->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
                    {
                        const bool bRough = Piece->GetName().StartsWith(TEXT("Rough"));
                        Test->TestTrue(Piece->GetName() + (bRough ? TEXT(" plays as rough") : TEXT(" plays as fairway")),
                            Mode->GetBall()->ResolveSurface(Piece) == (bRough ? EOpenFairwaySurface::Rough : EOpenFairwaySurface::Fairway));
                    }
                }
                Piece->ComponentTags = OriginalTags[Piece->GetFName()];
                if (Piece->GetFName() == TEXT("PuttingGreen"))
                    Piece->ComponentTags.AddUnique(AOpenFairwayBall::GetSurfaceTag(Case == 1 ? EOpenFairwaySurface::Green : EOpenFairwaySurface::Fairway));
                EOpenFairwaySurface Surface = EOpenFairwaySurface::Fairway;
                if (Case > 0 && Piece->GetName().StartsWith(TEXT("Fringe")))
                    Surface = EOpenFairwaySurface::Fringe;
                else if (Case == 1 && Piece->GetFName() == TEXT("Green"))
                    Surface = EOpenFairwaySurface::Green;
                else if (Case == 1 && Piece->GetName().StartsWith(TEXT("Rough")))
                    Surface = EOpenFairwaySurface::Rough;
                if (Surface != EOpenFairwaySurface::Fairway)
                    Piece->ComponentTags.AddUnique(AOpenFairwayBall::GetSurfaceTag(Surface));
            }
            Controller->SelectReplayShot(0);
            Controller->InjectReplayShot();
            LastSurface = EOpenFairwaySurface::Fairway;
            bLaunched = true;
            return false;
        }
        FOpenFairwayShotResult Result;
        if (!Mode->GetBall()->GetShotResult(Result))
            return false;
        if (Result.Surface != LastSurface)
        {
            Test->AddInfo(FString::Printf(TEXT("%s transition to %s at %.3f yd, t %.3f s"), Label(),
                *StaticEnum<EOpenFairwaySurface>()->GetNameStringByValue(static_cast<int64>(Result.Surface)),
                Result.TotalM * OpenFairway::YardsPerMeter, Result.ElapsedS));
            LastSurface = Result.Surface;
        }
        if (!Result.bAtRest)
            return false;
        Results.Add(Result);
        Test->AddInfo(FString::Printf(TEXT("%s: carry %.4f, roll %.4f, total %.4f yd, time %.3f s"), Label(),
            Result.CarryM * OpenFairway::YardsPerMeter, Result.RollM * OpenFairway::YardsPerMeter,
            Result.TotalM * OpenFairway::YardsPerMeter, Result.ElapsedS));
        Test->TestEqual(TEXT("Surface tags cannot change first-contact carry"), Result.CarryM * OpenFairway::YardsPerMeter, 121.5477f, 0.02f);
        if (++Case < 3)
        {
            bLaunched = false;
            return false;
        }
        Test->TestTrue(TEXT("Default range ends on fairway"), Results[0].Surface == EOpenFairwaySurface::Fairway);
        Test->TestTrue(TEXT("Historical mixed range ends on green"), Results[1].Surface == EOpenFairwaySurface::Green);
        // Case 2 is case 1 with the green tagged fairway, so the difference is the green alone.
        Test->TestTrue(FString::Printf(TEXT("Green rolls farther than the same path tagged fairway (%.2f vs %.2f yd)"),
                           Results[1].RollM * OpenFairway::YardsPerMeter, Results[2].RollM * OpenFairway::YardsPerMeter),
            Results[1].RollM > Results[2].RollM);
        Test->TestEqual(TEXT("Default fairway actor matches flat headless rollout"), Results[0].RollM * OpenFairway::YardsPerMeter, 37.6923f, 0.1f);
        Test->TestEqual(TEXT("Default fairway total"), Results[0].TotalM * OpenFairway::YardsPerMeter, 159.2375f, 0.1f);
        CheckLies(Mode);

        const OpenFairway::FBallPhysicsProfile Profile;
        const auto Params = OpenFairway::FResolvedShotParams::Resolve(Profile, OpenFairway::FAirConditions(), 51.17f / OpenFairway::MpsPerMph, 6.95f, 1933.0f);
        Test->TestEqual(TEXT("Regime kinetic friction neutral"), Params.Regime.KineticFrictionMultiplier, 1.0f, 1e-6f);
        Test->TestEqual(TEXT("Regime rolling friction neutral"), Params.Regime.RollingFrictionMultiplier, 1.0f, 1e-6f);
        Test->TestEqual(TEXT("Regime grass drag neutral"), Params.Regime.GrassViscosityMultiplier, 1.0f, 1e-6f);
        Test->TestEqual(TEXT("Run adds no bite"), Params.Landing.SpinBiteScale, 0.0f);
        for (auto Surface : {OpenFairway::ESurface::Fairway, OpenFairway::ESurface::Fringe, OpenFairway::ESurface::Green})
        {
            const auto Contact = Params.ResolveSurface(Surface, Profile);
            Test->AddInfo(FString::Printf(TEXT("%s: kinetic %.5f, rolling %.5f, viscosity %.5f, rolling spin scale %.3f"),
                OpenFairway::SurfaceCatalog::GetName(Surface), Contact.KineticFriction, Contact.RollingFriction,
                Contact.GrassViscosity, Contact.RollingSpinScale));
        }

        // The same shot on rough carries the same and runs out much shorter.
        const auto OnFairway = Mode->GetBall()->PredictShot(Results[0].Launch, EOpenFairwaySurface::Fairway);
        const auto OnRough = Mode->GetBall()->PredictShot(Results[0].Launch, EOpenFairwaySurface::Rough);
        const float FairwayRollYd = (OnFairway.TotalM - OnFairway.CarryM) * OpenFairway::YardsPerMeter;
        const float RoughRollYd = (OnRough.TotalM - OnRough.CarryM) * OpenFairway::YardsPerMeter;
        Test->AddInfo(FString::Printf(TEXT("Headless roll: fairway %.2f yd, rough %.2f yd"), FairwayRollYd, RoughRollYd));
        Test->TestEqual(TEXT("Rough carry matches fairway"), OnRough.CarryM, OnFairway.CarryM, 0.01f);
        Test->TestTrue(TEXT("Rough rolls much shorter than fairway"), RoughRollYd < 0.8f * FairwayRollYd);
        return true;
    }

private:
    /** A real ground probe on the untagged range finds each lie through the surface materials. */
    UStaticMesh* OriginalGreenMesh = nullptr;

    void CheckLies(AUOpenGolfGameMode* Mode)
    {
        TArray<UStaticMeshComponent*> Pieces;
        Mode->GetRange()->GetLayoutComponents(Pieces);
        for (UStaticMeshComponent* Piece : Pieces)
        {
            Piece->ComponentTags = OriginalTags[Piece->GetFName()];
            if (Piece->GetFName() == TEXT("PuttingGreen"))
            {
                Piece->SetStaticMesh(OriginalGreenMesh);
                Piece->SetRelativeScale3D(FVector::OneVector);
                Piece->AddRelativeLocation(FVector(0, 0, 50));
                Piece->SetMobility(EComponentMobility::Static);
            }
        }
        AOpenFairwayBall* Ball = Mode->GetBall();
        const FVector Tee = Mode->GetRange()->GetTeeLocation();
        auto LieAt = [Ball, Tee](float YMeters, float XMeters = 30.0f)
        {
            Ball->PlaceAt(Tee + FVector(XMeters * 100.0, YMeters * 100.0, 50.0));
            FOpenFairwayShotResult Lie;
            Ball->GetShotResult(Lie);
            return Lie.Surface;
        };
        Test->TestTrue(TEXT("Fairway lie"), LieAt(0.0f) == EOpenFairwaySurface::Fairway);
        Test->TestTrue(TEXT("Fairway lie just inside the edge"), LieAt(AUOpenGolfRange::FairwayHalfWidthM - 0.5f) == EOpenFairwaySurface::Fairway);
        Test->TestTrue(TEXT("Rough lie right"), LieAt(AUOpenGolfRange::FairwayHalfWidthM + 8.0f) == EOpenFairwaySurface::Rough);
        Test->TestTrue(TEXT("Rough lie left"), LieAt(-AUOpenGolfRange::FairwayHalfWidthM - 8.0f) == EOpenFairwaySurface::Rough);
        // The putting green is 20 feet (6.1 m) in radius around the flag; fairway surrounds it.
        Test->TestTrue(TEXT("Green lie near the flag"), LieAt(1.0f, AUOpenGolfRange::GreenDistanceM) == EOpenFairwaySurface::Green);
        Test->TestTrue(TEXT("Green lie inside the edge"), LieAt(5.5f, AUOpenGolfRange::GreenDistanceM) == EOpenFairwaySurface::Green);
        Test->TestTrue(TEXT("Fairway lie off the green"), LieAt(7.0f, AUOpenGolfRange::GreenDistanceM) == EOpenFairwaySurface::Fairway);
    }

    const TCHAR* Label() const { return Case == 0 ? TEXT("Default all fairway") : Case == 1 ? TEXT("Historical mixed range")
                                                                                            : TEXT("Fringe retained, green replaced"); }
    FAutomationTestBase* Test;
    int32 Case = 0;
    bool bLaunched = false;
    EOpenFairwaySurface LastSurface = EOpenFairwaySurface::Fairway;
    TMap<FName, TArray<FName>> OriginalTags;
    TArray<FOpenFairwayShotResult> Results;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRolloutSurfaceTest, "UOpenGolf.Range.RolloutSurfaces",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRolloutSurfaceTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCompareRolloutSurfaces>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
