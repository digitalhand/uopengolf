// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Misc/AutomationTest.h"
#include "RangeTestMap.h"
#include "OpenFairwayBall.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfRange.h"

// Shots on the saved range into obstacles tagged like the kit's trunk colliders: a line drive into a trunk
// comes back toward the tee, and a pitch onto a flat-topped obstacle slides off and rests on the ground.
class FPlayObstacleShots : public IAutomationLatentCommand
{
public:
    explicit FPlayObstacleShots(FAutomationTestBase* InTest)
        : Test(InTest) {}

    virtual bool Update() override
    {
        if (GetCurrentRunTime() > 90.0)
        {
            Test->AddError(TEXT("Timed out playing obstacle shots."));
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        if (!World || !World->HasBegunPlay() || !Mode || !Mode->GetRange() || !Mode->GetBall())
            return false;
        AOpenFairwayBall* Ball = Mode->GetBall();
        if (!bLaunched)
        {
            Mode->GetRange()->WindSpeedMph = 0;
            Ball->WindSpeedMph = 0;
            Ball->PlaceAt(Mode->GetRange()->GetTeeLocation());
            Launch(World, Ball);
            bLaunched = true;
            return false;
        }
        FOpenFairwayShotResult Result;
        const bool bShot = Ball->GetShotResult(Result);
        if (Case == 1 && bShot && !Result.bAtRest)
        {
            const FBox Box = Obstacle->GetComponentsBoundingBox();
            const FVector Ball3D = Ball->GetActorLocation();
            bTouchedTop |= Box.IsInsideXY(Ball3D) && Ball3D.Z > Box.Max.Z && Ball3D.Z < Box.Max.Z + 10;
        }
        if (!bShot || !Result.bAtRest)
            return false;
        const FVector Rest = Ball->GetActorLocation();
        const double Along = FVector::DotProduct(Rest - Start, Direction) / 100;
        FHitResult Below;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ObstacleContactGround), false, Ball);
        Query.AddIgnoredActor(Obstacle);
        const bool bGround = World->LineTraceSingleByChannel(Below, Rest + FVector(0, 0, 100), Rest - FVector(0, 0, 200), ECC_Visibility, Query);
        Test->AddInfo(FString::Printf(TEXT("%s: rest %.2f m along the line, %.2f cm above the ground, surface %s"), Case == 0 ? TEXT("Trunk") : TEXT("Flat top"),
            Along, bGround ? Rest.Z - Below.ImpactPoint.Z : -1.0, *StaticEnum<EOpenFairwaySurface>()->GetNameStringByValue(static_cast<int64>(Result.Surface))));
        Test->TestTrue(TEXT("The shot ends at rest, not by a safety stop"), Result.TerminationReason == EOpenFairwayShotTermination::Rest);
        Test->TestTrue(TEXT("The ball rests on the ground"), bGround && FMath::Abs(Rest.Z - Below.ImpactPoint.Z - OpenFairway::GolfPhysics::Radius * 100) < 2.0);
        if (Case == 0)
        {
            Test->TestTrue(TEXT("A line drive into a trunk stops short of it"), Along < ObstacleDistanceM - 1.0);
        }
        else
        {
            const FBox Box = Obstacle->GetComponentsBoundingBox();
            Test->TestTrue(TEXT("The ball does not rest on a flat-topped obstacle"), Rest.Z < Box.Max.Z - 10);
            Test->TestTrue(TEXT("The pitch reached the obstacle's top"), bTouchedTop);
        }
        Obstacle->Destroy();
        bLaunched = false;
        return ++Case == 2;
    }

private:
    void Launch(UWorld* World, AOpenFairwayBall* Ball)
    {
        Start = Ball->GetActorLocation();
        Direction = FRotator(0, Ball->AimYawDeg, 0).Vector();
        FOpenFairwayLaunch Shot;
        Shot.bHasBackSpin = true;
        if (Case == 0)
        {
            Shot.BallSpeedMph = 70;
            Shot.VerticalLaunchDeg = 4;
            Shot.BackSpinRpm = 2500;
            ObstacleDistanceM = 20;
        }
        else
        {
            Shot.BallSpeedMph = 65;
            Shot.VerticalLaunchDeg = 32;
            Shot.BackSpinRpm = 7000;
            ObstacleDistanceM = Ball->PredictShot(Shot, EOpenFairwaySurface::Fairway).CarryM;
        }
        const FVector Center = Start + Direction * ObstacleDistanceM * 100;
        FHitResult Ground;
        World->LineTraceSingleByChannel(Ground, Center + FVector(0, 0, 5000), Center - FVector(0, 0, 5000), ECC_Visibility,
            FCollisionQueryParams(SCENE_QUERY_STAT(ObstacleContactPlace), false, Ball));
        Obstacle = World->SpawnActor<AActor>();
        auto* Mesh = NewObject<UStaticMeshComponent>(Obstacle, TEXT("Obstacle"));
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Case == 0 ? TEXT("/Engine/BasicShapes/Cylinder.Cylinder") : TEXT("/Engine/BasicShapes/Cube.Cube")));
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->SetVisibility(false);
        Mesh->ComponentTags.Add(AOpenFairwayBall::GetObstacleTag());
        // Case 0: a 3 m wide, 8 m tall trunk. Case 1: an 8 m square block whose top is 1 m above the ground.
        Mesh->SetWorldScale3D(Case == 0 ? FVector(3, 3, 8) : FVector(8, 8, 2));
        Mesh->SetWorldLocation(FVector(Center.X, Center.Y, Ground.ImpactPoint.Z + (Case == 0 ? 400 : 0)));
        Obstacle->SetRootComponent(Mesh);
        Mesh->RegisterComponent();
        Test->TestTrue(TEXT("Obstacle placed on the ground"), Ground.bBlockingHit);
        Test->TestTrue(TEXT("Shot launched"), Ball->Launch(Shot));
    }

    FAutomationTestBase* Test;
    int32 Case = 0;
    bool bLaunched = false;
    bool bTouchedTop = false;
    double ObstacleDistanceM = 0;
    FVector Start = FVector::ZeroVector;
    FVector Direction = FVector::ForwardVector;
    AActor* Obstacle = nullptr;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObstacleContactTest, "UOpenGolf.Range.ObstacleContact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObstacleContactTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(GetRangeTestMap(), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FPlayObstacleShots>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
