// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Hole/OpenGolfCupComponent.h"
#include "Misc/AutomationTest.h"
#include "OpenGolfFlagstickComponent.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfCupPlacementTest, "UOpenGolf.Course.CupPlacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenGolfCupPlacementTest::RunTest(const FString& Parameters)
{
    const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("CupPlacementTest")), nullptr, true,
        ERHIFeatureLevel::Num, &Settings);
    AActor* Owner = World->SpawnActor<AActor>();
    auto* Cup = NewObject<UOpenGolfCupComponent>(Owner);
    Owner->SetRootComponent(Cup);
    Cup->RegisterComponent();
    auto* Flag = NewObject<UOpenGolfFlagstickComponent>(Owner);
    Flag->SetMobility(EComponentMobility::Movable);
    Flag->RegisterComponent();
    const FVector Ground(120, 250, 35);
    const FVector Slope = FVector(0.12, -0.08, 1).GetSafeNormal();
    for (const FVector& Normal : {FVector::UpVector, Slope})
    {
        Flag->SetWorldLocation(Ground);
        Cup->PlaceAtFlagstick(Flag, Normal * 3);
        TestTrue(TEXT("Cup origin is the ground position"), Cup->GetCupLocation().Equals(Ground));
        TestTrue(TEXT("Cup follows the normalized surface normal"), Cup->GetUpVector().Equals(Normal));
        for (UStaticMeshComponent* Disc : {Cup->GetHole()})
        {
            if (!TestNotNull(TEXT("Cup disc exists"), Disc))
                continue;
            TestEqual(TEXT("Cup visuals cannot deflect the ball"), Disc->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
            const FBox Bounds = Disc->GetStaticMesh()->GetBoundingBox();
            const FVector Top = Disc->GetComponentTransform().TransformPosition(FVector(0, 0, Bounds.Max.Z));
            const double Height = FVector::DotProduct(Top - Ground, Normal);
            TestTrue(TEXT("Top face is within 0.25 mm of the green"), Height > 0 && Height < 0.025);
            TestTrue(TEXT("Disc follows the green"), Disc->GetUpVector().Equals(Normal));
        }
        Cup->UpdatePinLift(Ground, UOpenGolfCupComponent::PinLiftSeconds);
        TestTrue(TEXT("Pin lifts vertically on a slope"), Flag->GetComponentLocation().Equals(Ground + FVector(0, 0, UOpenGolfCupComponent::PinLiftCm), 0.001));
        TestTrue(TEXT("Lifting the pin leaves the cup at ground level"), Cup->GetCupLocation().Equals(Ground));
        Cup->UpdatePinLift(Ground + FVector(1000, 0, 0), UOpenGolfCupComponent::PinLiftSeconds);
        TestTrue(TEXT("Pin returns to the ground"), Flag->GetComponentLocation().Equals(Ground, 0.001));
    }
    Cup->PlaceAtFlagstick(Flag, FVector::ZeroVector);
    TestTrue(TEXT("Zero normal falls back to world up"), Cup->GetUpVector().Equals(FVector::UpVector));
    Cup->PlaceAtFlagstick(Flag, FVector(std::numeric_limits<double>::infinity(), 0, 1));
    TestTrue(TEXT("Nonfinite normal falls back to world up"), Cup->GetUpVector().Equals(FVector::UpVector));
    Cup->PlaceOnSurface({Ground, Slope});
    TestNull(TEXT("A course can place a cup without a flag"), Cup->GetFlagstick());
    TestTrue(TEXT("Descriptor preserves the surface normal"), Cup->GetCupDescriptor().Normal.Equals(Slope));
    Cup->UpdatePinLift(Ground, 1);
    TestTrue(TEXT("An unflagged cup stays in place"), Cup->GetCupLocation().Equals(Ground));
    World->DestroyWorld(false);
    return true;
}

#endif
