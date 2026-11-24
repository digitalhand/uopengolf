// SPDX-License-Identifier: MIT
#include "Training/TrainingSession.h"
#include "Training/DistanceLadder.h"
#include "Training/DistancePractice.h"
#include "Training/TrainingScoring.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "OpenFairwayBall.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"

ATrainingSession::ATrainingSession()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Training")));
}

TConstArrayView<TSubclassOf<UTrainingDrill>> ATrainingSession::Games()
{
    static const TSubclassOf<UTrainingDrill> Entries[] = {UDistancePractice::StaticClass(), UDistanceLadder::StaticClass()};
    return MakeArrayView(Entries);
}

AUOpenGolfGameMode* ATrainingSession::Mode() const
{
    return GetWorld()->GetAuthGameMode<AUOpenGolfGameMode>();
}

bool ATrainingSession::CanStartDrill(const FDrillChoice& Choice) const
{
    const AUOpenGolfGameMode* GameMode = Mode();
    if (!GameMode || GameMode->IsCourseRound() || !Games().Contains(Choice.Game) || !TrainingScoring::IsValidRadius(Choice.RadiusFeet))
        return false;
    if (Choice.Game.GetDefaultObject()->UsesTries() && !TrainingScoring::IsValidTries(Choice.Tries))
        return false;
    return GameMode->GetBall() && !GameMode->GetBall()->IsMoving();
}

bool ATrainingSession::BeginDrill(const FDrillChoice& Choice)
{
    if (!CanStartDrill(Choice))
        return false;
    UTrainingDrill* Drill = NewObject<UTrainingDrill>(this, Choice.Game);
    if (!Drill->Start(*this, Choice.Tries, Choice.RadiusFeet))
        return false;
    if (ActiveDrill)
        ActiveDrill->Finish(*this, false);
    LeaveFinishedBall();
    RunStart = FinishedBalls.Num();
    ActiveDrill = Drill;
    TeeUpForDrill();
    return true;
}

bool ATrainingSession::StartDrill(TSubclassOf<UTrainingDrill> Game, int32 Tries, int32 RadiusFeet)
{
    return !ActiveDrill && BeginDrill({Game, Tries, RadiusFeet});
}

bool ATrainingSession::ChooseDrill(TSubclassOf<UTrainingDrill> Game, int32 Tries, int32 RadiusFeet)
{
    const FDrillChoice Choice{Game, Tries, RadiusFeet};
    if (!ActiveDrill)
        return BeginDrill(Choice);
    auto* Player = Cast<AUOpenGolfPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!Player || Player->IsDrillDialogOpen() || !CanStartDrill(Choice))
        return false;
    PendingDrill = Choice;
    Player->OpenDrillDialog();
    if (!Player->IsDrillDialogOpen())
        PendingDrill.Reset();
    return PendingDrill.IsSet();
}

bool ATrainingSession::StartPendingDrill()
{
    if (!PendingDrill)
        return false;
    const FDrillChoice Choice = PendingDrill.GetValue();
    PendingDrill.Reset();
    return BeginDrill(Choice);
}

void ATrainingSession::RestartDrill()
{
    if (!ActiveDrill)
        return;
    PendingBall.Reset();
    while (FinishedBalls.Num() > RunStart)
        FinishedBalls.Pop(EAllowShrinking::No)->DestroyComponent();
    // Teeing up first stops a shot in flight, so the drill can move its target.
    TeeUpForDrill();
    ActiveDrill->Restart(*this);
    Mode()->AimAtCup();
}

void ATrainingSession::ExitDrill()
{
    if (!ActiveDrill)
        return;
    ActiveDrill->Finish(*this, true);
    ActiveDrill = nullptr;
    TeeUpForDrill();
}

void ATrainingSession::TeeUpForDrill()
{
    AUOpenGolfGameMode* GameMode = Mode();
    if (auto* Player = Cast<AUOpenGolfPlayerController>(GetWorld()->GetFirstPlayerController()))
    {
        Player->SetPlacingBall(false);
        Player->ClearMapAim();
    }
    GameMode->ResetToTeeForPlay();
    GameMode->AimAtCup();
}

void ATrainingSession::RememberRest(const FOpenFairwayShotResult& Result)
{
    if (ActiveDrill)
        ActiveDrill->OnShotAtRest(*this, Result, Mode()->GetBall()->GetActorLocation());
    if (Result.TerminationReason == EOpenFairwayShotTermination::SafetyStop)
        return;
    const auto* Mesh = Cast<UStaticMeshComponent>(Mode()->GetBall()->GetRootComponent());
    if (!Mesh || !Mesh->GetStaticMesh())
        return;
    PendingBall = Mesh->GetComponentTransform();
}

void ATrainingSession::LeaveFinishedBall()
{
    if (!PendingBall)
        return;
    const auto* LiveMesh = CastChecked<UStaticMeshComponent>(Mode()->GetBall()->GetRootComponent());
    // Individual mesh components can use the ball's existing cooked material on every platform.
    auto* Copy = NewObject<UStaticMeshComponent>(this);
    Copy->SetMobility(EComponentMobility::Movable);
    Copy->SetupAttachment(GetRootComponent());
    Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Copy->SetGenerateOverlapEvents(false);
    Copy->SetCanEverAffectNavigation(false);
    Copy->SetHiddenInSceneCapture(true);
    Copy->SetComponentTickEnabled(false);
    Copy->SetStaticMesh(LiveMesh->GetStaticMesh());
    for (int32 Slot = 0; Slot < LiveMesh->GetNumMaterials(); ++Slot)
        Copy->SetMaterial(Slot, LiveMesh->GetMaterial(Slot));
    Copy->SetWorldTransform(PendingBall.GetValue());
    Copy->RegisterComponent();
    FinishedBalls.Add(Copy);
    PendingBall.Reset();
}

int32 ATrainingSession::FinishedBallCount() const
{
    return FinishedBalls.Num() + (PendingBall.IsSet() ? 1 : 0);
}

FString ATrainingSession::RejectReason() const
{
    if (!ActiveDrill)
        return FString();
    if (ActiveDrill->IsComplete())
        return TEXT("Drill complete; choose Play Again or Exit Drill");
    const auto* Ball = Mode()->GetBall();
    // Every try is scored where it stops, so a drill shot never interrupts the last one.
    return Ball && Ball->IsMoving() ? TEXT("Wait for the ball to stop") : FString();
}

void ATrainingSession::BeforeShot()
{
    if (ActiveDrill)
        ActiveDrill->ApplyPending();
}

void ATrainingSession::OpenDrillDialog()
{
    if (auto* Player = Cast<AUOpenGolfPlayerController>(GetWorld()->GetFirstPlayerController()))
        Player->OpenDrillDialog();
}
