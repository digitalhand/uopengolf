// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "UOpenGolfRange.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenGolfFlagstickComponent.h"
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
    TArray<UStaticMeshComponent*> Pieces;
    Range->GetLayoutComponents(Pieces);
    TestTrue(TEXT("Visible scene is saved in the map"), Pieces.Num() > 20);
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
    for (const UStaticMeshComponent* Piece : Pieces)
        if (Piece->ComponentHasTag(AUOpenGolfRange::GreenTag))
            TestTrue(TEXT("Green can be selected as its own actor"), Piece->GetOwner() != Range);
    for (const FVector Point : {FVector(40000, 0, 100), FVector(20000, 20000, 100), FVector(20000, -20000, 100), FVector(-50000, 0, 100)})
    {
        FHitResult Hit;
        TestTrue(TEXT("Surrounding rough has collision"), World->LineTraceSingleByChannel(Hit, Point, Point - FVector(0, 0, 300), ECC_Visibility));
        const auto* Ball = GetDefault<AOpenFairwayBall>();
        TestEqual(TEXT("Surrounding ground plays as rough"), Ball->ResolveSurface(Hit.GetComponent()), EOpenFairwaySurface::Rough);
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
