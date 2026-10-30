// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RangeTestMap.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "OpenGolfTerrain.h"
#include "OpenFairwayBall.h"
#include "OpenFairwayPhysicalMaterial.h"
#include "UOpenGolfRange.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoastalTerrainContactTest, "UOpenGolf.Range.CoastalTerrain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoastalTerrainContactTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(GetRangeTestMap(), false, true))
        return false;
    UWorld* World = GEditor->GetEditorWorldContext().World();
    AOpenGolfTerrainArea* Area = nullptr;
    for (TActorIterator<AOpenGolfTerrainArea> It(World); It; ++It)
        Area = *It;
    if (!TestNotNull(TEXT("Saved range has a terrain area"), Area))
        return false;
    FVector2D BunkerCenter = FVector2D::ZeroVector;
    if (!TestTrue(TEXT("Saved range has a bunker probe inside its sand outline"), Area->Definition && FindRangeBunkerCenter(*Area->Definition, BunkerCenter)))
        return false;
    struct FProbe
    {
        FVector2D Position;
        EOpenFairwaySurface Surface;
    };
    const FProbe Probes[] = {
        {{0, 0}, EOpenFairwaySurface::Fairway},
        // The par-4 green at the pin, its fringe beside it, and the old 150-yard green, now fairway.
        {{AUOpenGolfRange::HoleDistanceM, 0}, EOpenFairwaySurface::Green},
        {{363.0, 12.5}, EOpenFairwaySurface::Fringe},
        {{137.16, 0}, EOpenFairwaySurface::Fairway},
        {{360.0, 19.0}, EOpenFairwaySurface::Sand},
        {BunkerCenter, EOpenFairwaySurface::Sand},
        {{60, -42}, EOpenFairwaySurface::Rough},
        {{-252.01, 0}, EOpenFairwaySurface::Rough},
        {{-251.99, 0}, EOpenFairwaySurface::Rough},
        {{503.99, 0}, EOpenFairwaySurface::Rough},
        {{504.01, 0}, EOpenFairwaySurface::Rough},
        {{0, 125.99}, EOpenFairwaySurface::Rough},
        {{0, 126.01}, EOpenFairwaySurface::Rough},
        {{0, -125.99}, EOpenFairwaySurface::Rough},
        {{0, -126.01}, EOpenFairwaySurface::Rough}};
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CoastalContact), false);
    Query.bReturnPhysicalMaterial = true;
    for (const auto& Probe : Probes)
    {
        const FVector Position = Area->GetActorTransform().TransformPosition(FVector(Probe.Position.X * 100, Probe.Position.Y * 100, 0));
        FHitResult Hit;
        const FString Label = Probe.Position.ToString();
        if (!TestTrue(Label + TEXT(" ball sphere hits ground"), World->SweepSingleByChannel(Hit,
                                                                    Position + FVector(0, 0, 1000), Position - FVector(0, 0, 1000), FQuat::Identity,
                                                                    ECC_Visibility, FCollisionShape::MakeSphere(2.135f), Query)))
            continue;
        const auto* Physical = Cast<UOpenFairwayPhysicalMaterial>(Hit.PhysMaterial.Get());
        if (TestNotNull(Label + TEXT(" physical material"), Physical))
            TestTrue(Label + TEXT(" correct surface"), Physical->Surface == Probe.Surface);
        const double Expected = Area->Definition->HeightAt(Probe.Position) * 100 + Area->GetActorLocation().Z;
        const double ToleranceCm = Probe.Surface == EOpenFairwaySurface::Rough ? 10 : 3;
        TestTrue(Label + TEXT(" contact matches sampled height"), FMath::Abs(Hit.ImpactPoint.Z - Expected) < ToleranceCm);
        AddInfo(FString::Printf(TEXT("%s: height difference %.3f cm (limit %.0f cm)"), *Label, Hit.ImpactPoint.Z - Expected, ToleranceCm));
        TestTrue(Label + TEXT(" upward contact normal"), Hit.ImpactNormal.Z > 0.95);
    }
    int32 Instances = 0, Trees = 0, Colliders = 0;
    for (AActor* Actor : Area->GeneratedActors)
    {
        if (!IsValid(Actor))
            continue;
        const bool bGround = Actor->ActorHasTag(AOpenGolfTerrainArea::VegetationTag);
        const bool bTrees = Actor->ActorHasTag(AOpenGolfTerrainArea::TreesTag);
        const bool bColliders = Actor->ActorHasTag(AOpenGolfTerrainArea::TreeCollidersTag);
        TArray<UInstancedStaticMeshComponent*> Components;
        Actor->GetComponents(Components);
        for (auto* Component : Components)
        {
            if (bGround || bTrees)
            {
                (bGround ? Instances : Trees) += Component->GetInstanceCount();
                TestTrue(TEXT("Grass and tree meshes do not collide with the ball"), Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
            }
            else if (bColliders)
            {
                Colliders += Component->GetInstanceCount();
                TestTrue(TEXT("Trunk colliders are ball obstacles"), AOpenFairwayBall::IsObstacle(Component));
                TestTrue(TEXT("Trunk colliders block the ball's channel"), Component->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block);
                TestFalse(TEXT("Trunk colliders are hidden"), Component->IsVisible());
            }
        }
    }
    TestTrue(TEXT("Saved range contains baked vegetation"), Instances > 100);
    TestEqual(TEXT("Every tree has one trunk collider"), Colliders, Trees);
    AddInfo(FString::Printf(TEXT("Baked vegetation instances: %d; trees: %d; trunk colliders: %d"), Instances, Trees, Colliders));
    return true;
}
#endif
