// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenGolfTerrain.h"
#include "UOpenGolfRange.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCupThirtyFootTest, "UOpenGolf.Range.CupThirtyFoot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCupThirtyFootTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
        return false;
    UWorld* World = GEditor->GetEditorWorldContext().World();
    AUOpenGolfRange* Range = nullptr;
    for (TActorIterator<AUOpenGolfRange> It(World); It; ++It)
        Range = *It;
    if (!TestNotNull(TEXT("Range"), Range) || !TestNotNull(TEXT("Terrain"), Range->Terrain.Get()))
        return false;

    // A separate flat green, high above the map, exercises the same real ball actor and collision adapter.
    AActor* Flat = World->SpawnActor<AActor>();
    auto* Mesh = NewObject<UStaticMeshComponent>(Flat);
    Flat->SetRootComponent(Mesh);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Mesh->SetWorldLocation(FVector(0, 0, 9990));
    Mesh->SetWorldScale3D(FVector(40, 40, 0.2));
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Mesh->SetCollisionResponseToAllChannels(ECR_Block);
    Mesh->ComponentTags.Add(AOpenFairwayBall::GetSurfaceTag(EOpenFairwaySurface::Green));
    Mesh->RegisterComponent();
    auto* Ball = World->SpawnActor<AOpenFairwayBall>();
    Ball->bDrawTracer = false;
    Ball->WindSpeedMph = 0;
    Ball->GreenStimpFeet = 10;
    constexpr double DistanceCm = 30 * 30.48;
    for (bool bSaved : {false, true})
    {
        FOpenFairwayCup Cup{FVector(0, 0, 10000), FVector::UpVector};
        if (bSaved && !Range->Terrain->GroundAt(Range->GetCupLocation(), Cup.Position, Cup.Normal))
        {
            AddError(TEXT("Cannot sample the saved cup."));
            break;
        }
        FVector Start = Cup.Position - FVector(DistanceCm, 0, 0), StartNormal;
        if (bSaved && !Range->Terrain->GroundAt(Start, Start, StartNormal))
        {
            AddError(TEXT("Cannot sample the 30 ft start."));
            break;
        }
        TestEqual(TEXT("Start is exactly 30 horizontal feet away"), FVector::Dist2D(Start, Cup.Position), DistanceCm, 0.001);
        Ball->SetCups({{Cup.Position + FVector(500, 500, 0), Cup.Normal}, Cup});
        for (int32 Variant = 0; Variant < 3; ++Variant)
        {
            FOpenFairwayLaunch Launch;
            Launch.BallSpeedMph = 8.6f;
            Launch.bHasTotalSpin = true;
            Launch.VerticalLaunchDeg = Variant == 0 ? 0 : -0.26f;
            Launch.HorizontalLaunchDeg = Variant == 0 ? 0 : 4.01f;
            float Aim = Variant == 2 ? -4.01f : 0;
            FOpenFairwayShotResult Result;
            const bool bAimThroughCup = Variant != 1;
            for (int32 Attempt = 0; Attempt < (bAimThroughCup ? 8 : 1); ++Attempt)
            {
                Ball->PlaceAt(Start + FVector(0, 0, 5));
                Ball->AimYawDeg = Aim;
                TestTrue(TEXT("The 8.6 mph putt launches"), Ball->Launch(Launch));
                FVector Previous = Ball->GetActorLocation();
                double MissCm = 0;
                bool bPassedCup = false;
                for (int32 Frame = 0; Frame < 30 * 240 && Ball->IsMoving(); ++Frame)
                {
                    Ball->Tick(1.0f / 240);
                    const FVector Current = Ball->GetActorLocation();
                    if (!bPassedCup && Previous.X < Cup.Position.X && Current.X >= Cup.Position.X)
                    {
                        const double Alpha = (Cup.Position.X - Previous.X) / (Current.X - Previous.X);
                        MissCm = FMath::Lerp(Previous.Y, Current.Y, Alpha) - Cup.Position.Y;
                        bPassedCup = true;
                    }
                    Previous = Current;
                }
                TestFalse(TEXT("The shot comes to rest"), Ball->IsMoving());
                TestTrue(TEXT("The shot reports a result"), Ball->GetShotResult(Result));
                if (!bAimThroughCup || Result.bOverCup || Result.bHoled || !bPassedCup)
                    break;
                Aim -= FMath::RadiansToDegrees(FMath::Atan2(MissCm, DistanceCm));
            }
            if (bAimThroughCup)
                TestTrue(TEXT("An aimed 30 ft putt reaches the aperture"), Result.bOverCup || Result.bHoled);
            const FString Label = FString::Printf(TEXT("%s / %s"), bSaved ? TEXT("saved green") : TEXT("flat green"),
                Variant == 0 ? TEXT("straight launch, aimed") : Variant == 1 ? TEXT("replay angles, unadjusted aim") : TEXT("replay angles, aimed"));
            AddInfo(FString::Printf(TEXT("%s: 30 ft, 8.6 mph, stimp 10; aim %.3f deg; cup normal %s; crossed=%d holed=%d; entry %.3f m/s, offset %.4f m, spin %.1f rpm; exit %s m/s; max height above cup plane %.2f cm."),
                *Label, Aim, *Cup.Normal.ToString(), Result.bOverCup, Result.bHoled, Result.OverCupSpeedMps,
                Result.CupEntryOffsetM, Result.CupEntrySpinRpm, *Result.CupExitVelocityMps.ToString(), Result.CupMaxHeightM * 100));
        }
    }
    Ball->SetCupLocations({FVector(1, 2, 3)});
    TestEqual(TEXT("Legacy registration replaces descriptors"), Ball->GetCups().Num(), 1);
    TestTrue(TEXT("Legacy positions use world up"), Ball->GetCups()[0].Normal.Equals(FVector::UpVector));
    Ball->SetCups({{FVector::ZeroVector, FVector::ZeroVector}});
    TestTrue(TEXT("Invalid descriptor normal falls back to world up"), Ball->GetCups()[0].Normal.Equals(FVector::UpVector));
    Ball->Destroy();
    Flat->Destroy();
    return true;
}
#endif
