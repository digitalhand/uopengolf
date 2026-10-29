// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "UOpenGolfRange.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenGolfFlagstickComponent.h"
#include "OpenGolfTerrain.h"
#include "ProceduralMeshComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangeLayoutTest, "UOpenGolf.Range.EditableLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRangeLayoutTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
        return false;
    UWorld* World = GEditor->GetEditorWorldContext().World();
    TArray<AUOpenGolfRange*> Ranges;
    for (TActorIterator<AUOpenGolfRange> It(World); It; ++It)
        Ranges.Add(*It);
    if (!TestEqual(TEXT("One saved range controller"), Ranges.Num(), 1))
        return false;
    AUOpenGolfRange* Range = Ranges[0];
    TestNotNull(TEXT("The range plays on its terrain"), Range->Terrain.Get());
    // Daylight and view settings are saved as separate actors, so they can be selected before Play.
    TArray<UDirectionalLightComponent*> Suns;
    TArray<USkyAtmosphereComponent*> Skies;
    TArray<USkyLightComponent*> SkyLights;
    TArray<UPostProcessComponent*> Views;
    Range->GetLayoutComponents(Suns);
    Range->GetLayoutComponents(Skies);
    Range->GetLayoutComponents(SkyLights);
    Range->GetLayoutComponents(Views);
    TestEqual(TEXT("One sun"), Suns.Num(), 1);
    TestEqual(TEXT("One sky atmosphere"), Skies.Num(), 1);
    TestEqual(TEXT("One sky light"), SkyLights.Num(), 1);
    TestEqual(TEXT("One range view"), Views.Num(), 1);
    TArray<UStaticMeshComponent*> Pieces;
    Range->GetLayoutComponents(Pieces);
    TArray<UOpenGolfFlagstickComponent*> Flags;
    Range->GetLayoutComponents(Flags);
    if (TestEqual(TEXT("One flagstick in the saved scene"), Flags.Num(), 1))
    {
        TArray<UProceduralMeshComponent*> Cloth;
        Flags[0]->GetOwner()->GetComponents(Cloth);
        TestEqual(TEXT("One cloth surface per flagstick"), Cloth.Num(), 1);
        Flags[0]->ReregisterComponent();
        Cloth.Reset();
        Flags[0]->GetOwner()->GetComponents(Cloth);
        TestEqual(TEXT("Reregistering does not duplicate cloth"), Cloth.Num(), 1);
    }
    const int32 Count = Range->GetLayoutActors().FilterByPredicate([](const TObjectPtr<AActor>& Actor)
                                                    { return IsValid(Actor); })
                            .Num();
    Range->BuildDefaultLayout();
    TestEqual(TEXT("Building again preserves the layout"), Range->GetLayoutActors().Num(), Count);
    // The cut-turf ground draws the tee, so no painted tee box is left in the layout.
    for (const UStaticMeshComponent* Piece : Pieces)
        TestNotEqual(TEXT("No painted tee box"), Piece->GetName(), FString(TEXT("TeeBox")));
    const auto* DefaultBall = GetDefault<AOpenFairwayBall>();
    // Traces start well above the terrain's mounds, which rise about 5 m, and pass through tree colliders to the ground.
    auto SurfaceAt = [World, DefaultBall](const FVector& Point, bool& bHit)
    {
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(RangeLayoutSurface), false);
        Params.bReturnPhysicalMaterial = true;
        for (int32 Attempt = 0; Attempt < 8; ++Attempt)
        {
            bHit = World->LineTraceSingleByChannel(Hit, Point + FVector(0, 0, 5000), Point - FVector(0, 0, 5000), ECC_Visibility, Params);
            if (!bHit || !Hit.GetComponent() || !Hit.GetComponent()->ComponentHasTag(AOpenFairwayBall::GetObstacleTag()))
                break;
            Params.AddIgnoredComponent(Hit.GetComponent());
        }
        return bHit ? DefaultBall->ResolveSurface(Hit.GetComponent(), Hit.PhysMaterial.Get()) : EOpenFairwaySurface::Fairway;
    };
    if (Flags.Num() == 1)
    {
        bool bHit = false;
        const EOpenFairwaySurface UnderFlag = SurfaceAt(Flags[0]->GetComponentLocation(), bHit);
        TestTrue(TEXT("The flag stands on the green"), bHit && UnderFlag == EOpenFairwaySurface::Green);
    }
    for (const FVector Point : {FVector(40000, 0, 0), FVector(20000, 20000, 0), FVector(20000, -20000, 0), FVector(-50000, 0, 0)})
    {
        bool bHit = false;
        const EOpenFairwaySurface Surface = SurfaceAt(Point, bHit);
        TestTrue(TEXT("Surrounding ground has collision"), bHit);
        TestEqual(TEXT("Surrounding ground plays as rough"), Surface, EOpenFairwaySurface::Rough);
    }
    auto* Ball = World->SpawnActor<AOpenFairwayBall>();
    FOpenFairwayLaunch Launch;
    Launch.BallSpeedMph = 80.0f;
    Launch.VerticalLaunchDeg = 20.0f;
    TestTrue(TEXT("Safety-stop fixture launches"), Ball->Launch(Launch));
    Ball->SetActorLocation(FVector(110000, 0, 100));
    Ball->Tick(1.0f / 60.0f);
    FOpenFairwayShotResult Result;
    TestTrue(TEXT("Interrupted shot has a result"), Ball->GetShotResult(Result));
    TestTrue(TEXT("Safety stop ends motion"), Result.bAtRest && !Ball->IsMoving());
    TestEqual(TEXT("Safety stop is explicit"), Result.TerminationReason, EOpenFairwayShotTermination::SafetyStop);
    TestEqual(TEXT("Safety stop does not fabricate a landing"), Result.CarryM, 0.0f);
    Ball->Destroy();
    return true;
}

class FCheckPlayableBounds : public IAutomationLatentCommand
{
public:
    explicit FCheckPlayableBounds(FAutomationTestBase* InTest) : Test(InTest) {}
    bool Update() override
    {
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        if (!Mode || !Mode->GetBall())
        {
            if (GetCurrentRunTime() > 30)
            {
                Test->AddError(TEXT("Range did not start"));
                return true;
            }
            return false;
        }
        auto* Ball = Mode->GetBall();
        for (float Aim : {-45.0f, 45.0f, 0.0f})
        {
            Mode->ResetBallToTee();
            Ball->AimYawDeg = Aim;
            FOpenFairwayLaunch Launch;
            Launch.BallSpeedMph = Aim == 0 ? 240.0f : 150.0f;
            Launch.VerticalLaunchDeg = 12.5f;
            Launch.bHasBackSpin = true;
            Launch.BackSpinRpm = 2300.0f;
            Test->TestTrue(TEXT("Wide or long shot launches"), Ball->Launch(Launch));
            for (int32 Step = 0; Ball->IsMoving() && Step < 7200; ++Step)
                Ball->Tick(1.0f / 60.0f);
            FOpenFairwayShotResult Result;
            Ball->GetShotResult(Result);
            Test->TestTrue(TEXT("Shot lands and rests naturally"), Result.bLanded && Result.bAtRest && Result.TerminationReason == EOpenFairwayShotTermination::Rest);
            Test->TestEqual(TEXT("Surrounding ground plays as rough"), Result.Surface, EOpenFairwaySurface::Rough);
            const FVector FromTee = Ball->GetActorLocation() - Mode->GetRange()->GetTeeLocation();
            Test->TestTrue(TEXT("Shot finishes beyond the former range boundary"), Aim == 0 ? FromTee.X > 32000 : FMath::Abs(FromTee.Y) > 7000);
        }
        return true;
    }

private:
    FAutomationTestBase* Test;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayableBoundsTest, "UOpenGolf.Range.PlayableBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayableBoundsTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
        return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCheckPlayableBounds>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
