// SPDX-License-Identifier: MIT
#include "ShotFastForward.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "OpenFairwayBall.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogShotFastForward, Log, All);

UShotFastForward::UShotFastForward()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UShotFastForward::BeginPlay()
{
    Super::BeginPlay();
    // The engine's default limit (20) is below the skip rate; this world's copy of the settings allows it.
    if (AWorldSettings* Settings = GetWorld() ? GetWorld()->GetWorldSettings() : nullptr)
        Settings->MaxGlobalTimeDilation = FMath::Max(Settings->MaxGlobalTimeDilation, OpenGolfPlay::FastForwardSkipRate);
}

void UShotFastForward::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Stop();
    if (AOpenFairwayBall* Ball = BoundBall.Get())
    {
        Ball->OnBallAtRest.RemoveDynamic(this, &ThisClass::HandleShotEnded);
        Ball->OnBallHoled.RemoveDynamic(this, &ThisClass::HandleShotEnded);
    }
    Super::EndPlay(EndPlayReason);
}

AOpenFairwayBall* UShotFastForward::GetBall() const
{
    const UWorld* World = GetWorld();
    const AUOpenGolfGameMode* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    return Mode ? Mode->GetBall() : nullptr;
}

bool UShotFastForward::CanFastForward() const
{
    const AOpenFairwayBall* Ball = GetBall();
    return Ball && Ball->GetBallState() == EOpenFairwayBallState::Rollout && !Ball->IsHoled() && !IsInterrupted();
}

bool UShotFastForward::IsInterrupted() const
{
    const auto* Controller = Cast<AUOpenGolfPlayerController>(GetOwner());
    return Controller && (Controller->IsMainMenuOpen() || Controller->IsDrillDialogOpen());
}

double UShotFastForward::RealSeconds() const
{
    return GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
}

void UShotFastForward::Press(const FInputActionValue&)
{
    Input.Press(RealSeconds(), CanFastForward());
}

void UShotFastForward::Release(const FInputActionValue&)
{
    Input.Release(RealSeconds());
}

void UShotFastForward::HandleShotEnded(const FOpenFairwayShotResult&)
{
    Stop();
}

void UShotFastForward::Stop()
{
    Input.Reset();
    Apply(1.0f);
}

void UShotFastForward::Apply(float Rate)
{
    UWorld* World = GetWorld();
    if (!World || Rate == AppliedRate)
        return;
    UGameplayStatics::SetGlobalTimeDilation(World, Rate);
    AppliedRate = Rate;
    UE_LOG(LogShotFastForward, Verbose, TEXT("World speed %.0fx."), Rate);
}

void UShotFastForward::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AOpenFairwayBall* Ball = GetBall();
    if (Ball && BoundBall.Get() != Ball)
    {
        Ball->OnBallAtRest.AddUniqueDynamic(this, &ThisClass::HandleShotEnded);
        Ball->OnBallHoled.AddUniqueDynamic(this, &ThisClass::HandleShotEnded);
        BoundBall = Ball;
    }
    const bool bRolling = CanFastForward();
    // A shot ends a fast-forward however it ends: a new launch, the ball moved by hand or put back on the tee. A putt
    // that starts rolling in the frame the last shot was cut short is told apart by its shot number.
    const int32 ShotNumber = Ball ? Ball->GetShotNumber() : INDEX_NONE;
    if ((bWasRolling && !bRolling) || ShotNumber != LastShotNumber || IsInterrupted())
        Input.Reset();
    bWasRolling = bRolling;
    LastShotNumber = ShotNumber;
    Apply(Input.Rate(bRolling));
}
