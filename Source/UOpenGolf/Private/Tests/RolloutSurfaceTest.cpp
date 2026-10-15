// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"

// Verify the default all-fairway range, then reconstruct the historical mixed
// surfaces in this disposable PIE world to retain the rollout comparison.
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
            Mode->GetRange()->GetComponents(Pieces);
            for (UStaticMeshComponent* Piece : Pieces)
            {
                if (Case == 0)
                {
                    OriginalTags.Add(Piece->GetFName(), Piece->ComponentTags);
                    if (Piece->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
                    {
                        Test->TestTrue(Piece->GetName() + TEXT(" uses fairway physics by default"),
                            Piece->ComponentTags.Contains(AOpenFairwayBall::GetSurfaceTag(EOpenFairwaySurface::Fairway)));
                    }
                }
                Piece->ComponentTags = OriginalTags[Piece->GetFName()];
                EOpenFairwaySurface Surface = EOpenFairwaySurface::Fairway;
                if (Case > 0 && Piece->GetName().StartsWith(TEXT("Fringe")))
                    Surface = EOpenFairwaySurface::Fringe;
                else if (Case == 1 && Piece->GetFName() == TEXT("Green"))
                    Surface = EOpenFairwaySurface::Green;
                else if (Case == 1 && Piece->GetName().StartsWith(TEXT("Rough")))
                    Surface = EOpenFairwaySurface::Rough;
                if (Surface != EOpenFairwaySurface::Fairway)
                {
                    Piece->ComponentTags.Remove(AOpenFairwayBall::GetSurfaceTag(EOpenFairwaySurface::Fairway));
                    Piece->ComponentTags.Add(AOpenFairwayBall::GetSurfaceTag(Surface));
                }
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
        Test->TestEqual(TEXT("Mixed range reproduces reported rollout"), Results[1].RollM * OpenFairway::YardsPerMeter, 31.9f, 0.15f);
        Test->TestEqual(TEXT("Default fairway actor matches flat headless rollout"), Results[0].RollM * OpenFairway::YardsPerMeter, 37.6923f, 0.1f);
        Test->TestEqual(TEXT("Default fairway total"), Results[0].TotalM * OpenFairway::YardsPerMeter, 159.2375f, 0.1f);

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
        return true;
    }

private:
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
