// SPDX-License-Identifier: MIT
#include "UOpenGolfGameMode.h"
#include "OpenGolfSettings.h"
#include "GolfGimmieIndicator.h"
#include "GolfCourseDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Course/CourseClubs.h"
#include "Course/GolfCourseSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "OpenFairwayBall.h"
#include "Hole/OpenGolfCupComponent.h"
#include "UOpenGolfBall.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "ShotRecording.h"
#include "Training/TrainingSession.h"
#include "Online/CloudShotRecorder.h"
#include "OpenGolfOnlineSubsystem.h"
#include "TimerManager.h"
#include "UOpenGolfHUD.h"
#include "UOpenGolfPlayerController.h"
#include "OpenGolfFlagstickComponent.h"
#include "UOpenGolfRange.h"

DEFINE_LOG_CATEGORY_STATIC(LogUOpenGolfGameMode, Log, All);

AUOpenGolfGameMode::AUOpenGolfGameMode()
{
    HUDClass = AUOpenGolfHUD::StaticClass();
    PlayerControllerClass = AUOpenGolfPlayerController::StaticClass();
    DefaultPawnClass = nullptr;
    RangeClass = AUOpenGolfRange::StaticClass();
    Recorder = MakeShared<ShotRecording::FShotRecorder>();
}

void AUOpenGolfGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    Course = UGolfCourseSubsystem::FindCourse(Options);
    if (Course && !Course->Layout.IsEmpty() && !Course->Layout[0].Tees.IsEmpty())
    {
#if WITH_EDITOR
        if (GetWorld()->WorldType == EWorldType::PIE)
            HoleIndex = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("PreviewHole"), 1)-1, 0, Course->Layout.Num()-1);
#endif
        TeeIndex = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("TeeIndex"), 0), 0, Course->Layout[0].Tees.Num() - 1);
        TeeSet = Course->Layout[0].Tees[TeeIndex].SetName;
    }
    StartingHole = HoleIndex;
    PlayMode = Course ? EOpenGolfPlayMode::Hole : EOpenGolfPlayMode::Range;
    if (Course)
        UE_LOG(LogUOpenGolfGameMode, Log, TEXT("Course round: %s."), *Course->DisplayName.ToString());
}

int32 AUOpenGolfGameMode::GetPar() const
{
    if (Course && Course->Layout.IsValidIndex(HoleIndex))
        return Course->Layout[HoleIndex].Par;
    return Course ? Course->Par : AUOpenGolfRange::Par;
}

bool AUOpenGolfGameMode::IsRoundComplete() const
{
    return IsCourseRound() && Round.bHoled && (!Course || HoleIndex + 1 >= Course->Layout.Num()) &&
        CompletedHoles.Num() == HoleIndex - StartingHole + 1;
}

EGolfPenalty AUOpenGolfGameMode::GetRecentPenalty() const
{
    const UWorld* World = GetWorld();
    return World && LastPenaltyTime >= 0 && World->GetTimeSeconds() - LastPenaltyTime < PenaltyNoticeS ? LastPenalty : EGolfPenalty::None;
}

float AUOpenGolfGameMode::GetHoleLengthM() const
{
    if (Course && Course->Layout.IsValidIndex(HoleIndex))
        if (const auto* Tee = Course->Layout[HoleIndex].FindTee(TeeSet))
            return Tee->PublishedYards * 0.9144f;
    return Course ? Course->LengthM : AUOpenGolfRange::HoleDistanceM;
}

void AUOpenGolfGameMode::ActivateCourseHole()
{
    if (!Course || !Course->Layout.IsValidIndex(HoleIndex) || !Range || !Ball)
        return;
    const auto& Hole = Course->Layout[HoleIndex];
    const auto* Tee = Hole.FindTee(TeeSet);
    if (!ensure(Tee)) return;
    Range->SetCourseHole(Tee->PositionCm, Hole.CupCm, Hole.RoutingCm);
    Ball->SetCups({Range->GetCup()->GetCupDescriptor()});
    OpeningAimDeg = OpenGolfPlay::BearingDeg(Tee->PositionCm, Hole.RoutingCm.IsEmpty() ? Hole.CupCm : Hole.RoutingCm[0], 0);
    ResetToTeeForPlay();
}

void AUOpenGolfGameMode::StartPlay()
{
    GolfSettings::Load();
    if (auto* Online = GetGameInstance()->GetSubsystem<UOpenGolfOnlineSubsystem>())
    {
        TWeakObjectPtr<UOpenGolfOnlineSubsystem> WeakOnline(Online);
        CloudRecorder = MakeShared<OpenGolfOnline::FCloudShotRecorder>([WeakOnline](const auto& Shot, const FString& Owner)
        { return WeakOnline.IsValid() && WeakOnline->QueueShot(Shot,Owner); });
        CloudReadHandle = Online->OnBeforeReadProfile.AddWeakLambda(this,[this]{if(CloudRecorder)CloudRecorder->Release();});
        CloudAccountHandle = Online->OnAccountChanged.AddWeakLambda(this,[this]{if(CloudRecorder)CloudRecorder->Clear();});
    }
    UWorld* World = GetWorld();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    UClass* SpawnClass = RangeClass ? RangeClass.Get() : AUOpenGolfRange::StaticClass();
    for (TActorIterator<AUOpenGolfRange> It(World); It; ++It)
    {
        if (Range)
        {
            // Play must still start (every actor's BeginPlay depends on it), so use the first one.
            UE_LOG(LogUOpenGolfGameMode, Error, TEXT("The map contains multiple range controllers; using %s and ignoring %s. Keep exactly one."),
                *Range->GetName(), *It->GetName());
            continue;
        }
        Range = *It;
    }
    if (!Range)
        Range = World->SpawnActor<AUOpenGolfRange>(SpawnClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (ensure(Range))
    {
        Range->BuildDefaultLayout();
        Ball = World->SpawnActor<AUOpenGolfBall>(Range->GetTeeLocation() + FVector::UpVector * 10.0f, FRotator::ZeroRotator, Params);
    }
    if (Ball)
    {
        Ball->OnBallAtRest.AddDynamic(this, &ThisClass::HandleBallAtRest);
        Ball->PoleRadiusCm = UOpenGolfFlagstickComponent::PoleDiameterCm * 0.5f;
    }
    if (!IsCourseRound()) CreateTrainingSession();
    Round.Start(GetPar());
    if (Range) { MapWindMph = Range->WindSpeedMph; MapWindFromDeg = Range->WindFromDeg; }
    StartSessionDefaults();
    SelectClubForLie();

    if (UOpenLaunchMonitorSubsystem* LaunchMonitor = GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>())
    {
        ShotHandle = LaunchMonitor->OnShotReceivedNative.AddUObject(this, &ThisClass::HandleShot);
        ClubDataHandle = LaunchMonitor->OnClubDataReceivedNative.AddUObject(this, &ThisClass::HandleClubData);
    }

    GetWorldTimerManager().SetTimer(RecordingTimer, FTimerDelegate::CreateWeakLambda(this, [this]
                                                        {
        if (Recorder) Recorder->Tick();
        if (CloudRecorder) CloudRecorder->Retry();
        if (GimmieIndicator && Range)
            GimmieIndicator->UpdateIndicator(Range->Terrain, GetCupLocation(), SessionGimmieFeet,
                PlayMode == EOpenGolfPlayMode::Hole && !Round.bPractice && !Round.bHoled); }),
        0.1f, true);

    // Spawn the range before players start so the controller can find the ball.
    GolfSettings::ApplyRuntime(World);
    Super::StartPlay();
    ActivateCourseHole();
    if (Ball && Range && Range->GetCup())
        Ball->SetCups({Range->GetCup()->GetCupDescriptor()});
}

void AUOpenGolfGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(Ball))
        Ball->OnBallAtRest.RemoveDynamic(this, &ThisClass::HandleBallAtRest);
    GetWorldTimerManager().ClearTimer(ResetTimer);
    GetWorldTimerManager().ClearTimer(AddressTimer);
    GetWorldTimerManager().ClearTimer(RecordingTimer);
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UOpenLaunchMonitorSubsystem* LaunchMonitor = GameInstance->GetSubsystem<UOpenLaunchMonitorSubsystem>())
        {
            LaunchMonitor->OnShotReceivedNative.Remove(ShotHandle);
            LaunchMonitor->OnClubDataReceivedNative.Remove(ClubDataHandle);
        }
    }
    if (Recorder)
    {
        Recorder->Shutdown();
    }
    if (auto* Online = GetGameInstance()->GetSubsystem<UOpenGolfOnlineSubsystem>())
    {
        if (CloudRecorder) CloudRecorder->Release();
        Online->OnBeforeReadProfile.Remove(CloudReadHandle);
        Online->OnAccountChanged.Remove(CloudAccountHandle);
        Online->Flush();
    }
    CloudRecorder.Reset();
    Super::EndPlay(EndPlayReason);
}

void AUOpenGolfGameMode::CreateTrainingSession()
{
    Training = GetWorld()->SpawnActor<ATrainingSession>();
}

bool AUOpenGolfGameMode::IsDrillActive() const
{
    return Training && Training->Active();
}

bool AUOpenGolfGameMode::CanPlaceTrainingBall() const
{
    return !IsCourseRound() && (!IsDrillActive() || Training->Active()->AllowsPlacement());
}

void AUOpenGolfGameMode::ResetBallToTee()
{
    if (!IsDrillActive()) ResetToTeeForPlay();
}

void AUOpenGolfGameMode::ResetToTeeForPlay()
{
    Round.Start(GetPar());
    LastScoredShot = INDEX_NONE;
    if (PlayMode == EOpenGolfPlayMode::Hole) StartSessionDefaults();
    if (CloudRecorder) CloudRecorder->Cancel();
    if (Recorder)
        Recorder->CancelSimulation();
    PlaceOnTee();
}

void AUOpenGolfGameMode::ResetBall()
{
    if (!IsDrillActive()) ResetForPlay();
}

void AUOpenGolfGameMode::ResetForPlay()
{
    if (!Round.bPractice)
    {
        ResetToTeeForPlay();
        return;
    }
    if (CloudRecorder) CloudRecorder->Cancel();
    if (Recorder) Recorder->CancelSimulation();
    GetWorldTimerManager().ClearTimer(ResetTimer);
    GetWorldTimerManager().ClearTimer(AddressTimer);
    HoledTime = -1.0;
    if (Ball)
    {
        if (auto* Presented = Cast<AUOpenGolfBall>(Ball)) Presented->ResetShotTrail();
        // Keep the player's aim (A/D): placing aimed the ball at the cup once, and PlaceAt faces it down the aim line.
        if (Training) Training->LeaveFinishedBall();
        Ball->PlaceAt(PracticeOrigin);
    }
    bAddressReady = true;
}

void AUOpenGolfGameMode::PlaceOnTee()
{
    GetWorldTimerManager().ClearTimer(ResetTimer);
    GetWorldTimerManager().ClearTimer(AddressTimer);
    HoledTime = -1.0;
    bAddressReady = true;
    if (Ball && Range)
    {
        if (auto* PresentedBall = Cast<AUOpenGolfBall>(Ball))
            PresentedBall->ResetShotTrail();
        // A hole starts down the target line; on the range the aim stays set across shots.
        if (PlayMode == EOpenGolfPlayMode::Hole)
            Ball->AimYawDeg = OpeningAimDeg;
        if (Training) Training->LeaveFinishedBall();
        Ball->PlaceAt(Range->GetTeeLocation() + FVector::UpVector * 10.0f);
        SelectClubForLie();
    }
}

#if WITH_DEV_AUTOMATION_TESTS
bool AUOpenGolfGameMode::SetPlayMode(EOpenGolfPlayMode Mode, const UGolfCourseDefinition* InCourse)
{
    if (Ball && Ball->IsMoving())
        return false;
    if (Training)
    {
        Training->ExitDrill();
        Training->Destroy();
        Training = nullptr;
    }
    PlayMode = Mode;
    if (!IsCourseRound()) CreateTrainingSession();
    Course = Mode == EOpenGolfPlayMode::Hole ? InCourse : nullptr;
    HoleIndex = 0;
    CompletedHoles.Reset();
    if (Course && !Course->Layout.IsEmpty()) ActivateCourseHole();
    UE_LOG(LogUOpenGolfGameMode, Log, TEXT("Play mode: %s."), Mode == EOpenGolfPlayMode::Hole ? TEXT("hole") : TEXT("range"));
    ResetBallToTee();
    return true;
}

#endif

bool AUOpenGolfGameMode::PlaceBallAt(const FVector& Location)
{
    if (!CanPlaceTrainingBall() || !Ball || Ball->IsMoving() || !OpenGolfPlay::CanPlaceBallAt(Location, GetCupLocation()))
        return false;
    GetWorldTimerManager().ClearTimer(ResetTimer);
    GetWorldTimerManager().ClearTimer(AddressTimer);
    if (CloudRecorder) CloudRecorder->Cancel();
    if (Recorder)
        Recorder->CancelSimulation();
    // A finished hole starts over; either way the hole is now practice.
    Round.Start(GetPar());
    Round.bPractice = true;
    HoledTime = -1.0;
    if (auto* PresentedBall = Cast<AUOpenGolfBall>(Ball))
        PresentedBall->ResetShotTrail();
    if (Training) Training->LeaveFinishedBall();
    Ball->PlaceAt(Location + FVector::UpVector * 10.0f);
    PracticeOrigin = Ball->GetActorLocation();
    AimAtCup();
    bAddressReady = true;
    return true;
}

FVector AUOpenGolfGameMode::GetCupLocation() const
{
    return Range ? Range->GetCupLocation() : FVector::ZeroVector;
}

float AUOpenGolfGameMode::GetDistanceToCupM() const
{
    return Ball ? static_cast<float>(FVector::Dist2D(Ball->GetActorLocation(), GetCupLocation())) / 100.0f : 0.0f;
}

bool AUOpenGolfGameMode::IsBallAtAddress() const
{
    if (!Ball || Ball->IsMoving())
        return false;
    if (PlayMode == EOpenGolfPlayMode::Hole)
        return bAddressReady && !Ball->IsHoled() && !Round.bHoled;
    if (Round.bPractice)
        return bAddressReady && !Ball->IsHoled() && FVector::Dist2D(Ball->GetActorLocation(), PracticeOrigin) <= PracticeAddressToleranceCm;
    return !Range || FVector::Dist2D(Ball->GetActorLocation(), Range->GetTeeLocation()) <= TeeAddressToleranceCm;
}

float AUOpenGolfGameMode::GetHoledSeconds() const
{
    return HoledTime >= 0.0 && GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - HoledTime) : -1.0f;
}

void AUOpenGolfGameMode::AimAtCup()
{
    if (!Ball)
        return;
    Ball->AimYawDeg = OpenGolfPlay::BearingDeg(Ball->GetActorLocation(), GetCupLocation(), Ball->AimYawDeg);
    Ball->SetActorRotation(FRotator(0.0f, Ball->AimYawDeg, 0.0f));
}

void AUOpenGolfGameMode::ReadyForNextShot()
{
    if (!Ball || Ball->IsMoving() || PlayMode != EOpenGolfPlayMode::Hole)
        return;
    AimAtCup();
    bAddressReady = true;
}

void AUOpenGolfGameMode::AddressNextShot()
{
    ReadyForNextShot();
    // Only here, never as a shot arrives: the club in hand then is the club the shot was hit with. A club the player
    // picked while the stopped shot was shown is kept.
    const UGameInstance* GameInstance = GetGameInstance();
    const auto* LaunchMonitor = GameInstance ? GameInstance->GetSubsystem<UOpenLaunchMonitorSubsystem>() : nullptr;
    if (LaunchMonitor && LaunchMonitor->GetClub() == ClubAtRest)
        SelectClubForLie();
}

bool AUOpenGolfGameMode::IsBallOnTee() const
{
    return Ball && Range && FVector::Dist2D(Ball->GetActorLocation(), Range->GetTeeLocation()) <= OpenGolfPlay::TeeBoxRadiusM * 100.0;
}

const UOpenGolfLieRules& AUOpenGolfGameMode::GetLieRules() const
{
    return Course ? Course->GetLieRules() : *GetDefault<UOpenGolfLieRules>();
}

FOpenGolfLieCut AUOpenGolfGameMode::GetLieCut() const
{
    const UGameInstance* GameInstance = GetGameInstance();
    const auto* LaunchMonitor = GameInstance ? GameInstance->GetSubsystem<UOpenLaunchMonitorSubsystem>() : nullptr;
    if (!IsCourseRound() || !Ball || !LaunchMonitor || Ball->IsHoled() || IsBallOnTee())
        return FOpenGolfLieCut();
    return GetLieRules().CutFor(Ball->GetResult().Surface, GetDistanceToCupM(), CourseClubs::Group(LaunchMonitor->GetClub()));
}

void AUOpenGolfGameMode::SelectClubForLie()
{
    UGameInstance* GameInstance = GetGameInstance();
    auto* LaunchMonitor = GameInstance ? GameInstance->GetSubsystem<UOpenLaunchMonitorSubsystem>() : nullptr;
    if (!IsCourseRound() || GolfSettings::Get().AutoClub == 0 || !Ball || !LaunchMonitor || Ball->IsMoving() || Ball->IsHoled())
        return;
    CourseClubs::FAdvice Advice;
    Advice.DistanceToCupM = GetDistanceToCupM();
    Advice.Surface = Ball->GetResult().Surface;
    Advice.bOnTee = IsBallOnTee();
    Advice.Rules = &GetLieRules();
    const ELaunchMonitorClub Club = CourseClubs::Recommend(Advice);
    // Unchanged, nothing is sent: SetClub also tells the launch monitor.
    if (Club != LaunchMonitor->GetClub())
        LaunchMonitor->SetClub(Club);
}

void AUOpenGolfGameMode::SetRecordShots(bool bRecord)
{
    if (Recorder)
    {
        Recorder->SetEnabled(bRecord);
    }
}

bool AUOpenGolfGameMode::IsRecordingShots() const
{
    return Recorder && Recorder->IsEnabled();
}

int32 AUOpenGolfGameMode::GetRecordedShotCount() const
{
    return Recorder ? Recorder->GetShotCount() : 0;
}

int32 AUOpenGolfGameMode::GetFailedRecordingCount() const
{
    return Recorder ? Recorder->GetFailedSaveCount() : 0;
}

FString AUOpenGolfGameMode::GetRecordingError() const
{
    return Recorder ? Recorder->GetLastError() : FString();
}

FString AUOpenGolfGameMode::GetRecordingFolder() const
{
    return Recorder ? Recorder->GetSessionDir() : FString();
}

void AUOpenGolfGameMode::HandleBallAtRest(const FOpenFairwayShotResult& Result)
{
    if (Training) Training->RememberRest(Result);
    if (CloudRecorder) CloudRecorder->OnRest(Result);
    if (Recorder)
    {
        Recorder->OnBallAtRest(Result);
    }
    if (Round.bPractice)
    {
        if (Result.bHoled) HoledTime = GetWorld()->GetTimeSeconds();
        if (GolfSettings::Get().AutoTee > 0)
            GetWorldTimerManager().SetTimer(ResetTimer, this, &ThisClass::ResetAfterShot, float(GolfSettings::Get().AutoTee), false);
        return;
    }
    if (PlayMode == EOpenGolfPlayMode::Hole)
    {
        if (Round.bHoled || LastScoredShot == Result.ShotNumber) return;
        LastScoredShot = Result.ShotNumber;
        Round.RecordStroke(bShotFromGreen, Result.bHoled);
        const EGolfPenalty Penalty = !Result.bHoled && Course && Ball ? Course->PenaltyAt(Ball->GetActorLocation()) : EGolfPenalty::None;
        if (Penalty != EGolfPenalty::None)
        {
            // Stroke and distance: one penalty stroke, then play again from where the last shot was hit.
            LastPenalty = Penalty;
            LastPenaltyTime = GetWorld()->GetTimeSeconds();
            Round.RecordStroke(false, false);
            Ball->PlaceAt(PreviousLie);
            SelectClubForLie();
            bAddressReady = true;
            return;
        }
        Round.TryConcede(GetDistanceToCupM(), SessionGimmieFeet, Result.Surface == EOpenFairwaySurface::Green);
        if (Round.bHoled)
        {
            HoledTime = GetWorld()->GetTimeSeconds();
            UE_LOG(LogUOpenGolfGameMode, Log, TEXT("Holed in %d (%d putts): %s."), Round.Strokes, Round.Putts, *Round.ResultName());
            GetWorldTimerManager().SetTimer(ResetTimer, this, &ThisClass::FinishRound, HoleOutDelayS, false);
        }
        else
        {
            // Let the shot be seen where it stopped, then turn to the cup for the next one.
            if (const auto* LaunchMonitor = GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>())
                ClubAtRest = LaunchMonitor->GetClub();
            GetWorldTimerManager().SetTimer(AddressTimer, this, &ThisClass::AddressNextShot, AddressDelayS, false);
        }
        return;
    }
    // Leave the ball where it stopped so the shot can be seen, then tee up for the next one.
    if (GolfSettings::Get().AutoTee > 0)
        GetWorldTimerManager().SetTimer(ResetTimer, this, &ThisClass::ResetAfterShot, float(GolfSettings::Get().AutoTee), false);
}

void AUOpenGolfGameMode::FinishRound()
{
    if (!IsCourseRound() || !Round.bHoled || CompletedHoles.Num() != HoleIndex - StartingHole)
        return;
    CompletedHoles.Add(Round);
    if (IsRoundComplete())
        OnRoundComplete.Broadcast();
    else
    {
        ++HoleIndex;
        ActivateCourseHole();
    }
}

void AUOpenGolfGameMode::ResetAfterShot()
{
    // A ball launched directly (not through HandleShot) during the wait finishes its shot.
    if (Ball && !Ball->IsMoving())
    {
        ResetForPlay();
    }
}

bool AUOpenGolfGameMode::ToLaunch(const FLaunchMonitorShot& Shot, FOpenFairwayLaunch& OutLaunch)
{
    if (Shot.BallSpeedSource == ELaunchMonitorValueSource::Missing || Shot.VerticalLaunchSource == ELaunchMonitorValueSource::Missing)
    {
        return false;
    }

    OutLaunch = FOpenFairwayLaunch();
    OutLaunch.BallSpeedMph = Shot.GetBallSpeedMph();
    OutLaunch.VerticalLaunchDeg = Shot.VerticalLaunchDeg;
    // An unmeasured direction is straight; unmeasured spin is derived or left out, never invented.
    OutLaunch.HorizontalLaunchDeg = Shot.HorizontalLaunchSource == ELaunchMonitorValueSource::Missing ? 0.0f : Shot.HorizontalLaunchDeg;
    OutLaunch.bHasBackSpin = Shot.BackSpinSource != ELaunchMonitorValueSource::Missing;
    OutLaunch.BackSpinRpm = Shot.BackSpinRpm;
    OutLaunch.bHasSideSpin = Shot.SideSpinSource != ELaunchMonitorValueSource::Missing;
    OutLaunch.SideSpinRpm = Shot.SideSpinRpm;
    OutLaunch.bHasTotalSpin = Shot.TotalSpinSource != ELaunchMonitorValueSource::Missing;
    OutLaunch.TotalSpinRpm = Shot.TotalSpinRpm;
    OutLaunch.bHasSpinAxis = Shot.SpinAxisSource != ELaunchMonitorValueSource::Missing;
    OutLaunch.SpinAxisDeg = Shot.SpinAxisDeg;
    return true;
}

void AUOpenGolfGameMode::HandleShot(const FLaunchMonitorShot& Shot)
{
    UOpenLaunchMonitorSubsystem* LaunchMonitor = GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>();
    // A refused shot is reported, so an Open Connect connector receives an error rather than a confirmation.
    auto Reject = [LaunchMonitor, &Shot](const TCHAR* Reason) { if (LaunchMonitor) LaunchMonitor->RejectShot(Shot.ShotNumber, Reason); };
    const auto* Player = Cast<AUOpenGolfPlayerController>(GetWorld()->GetFirstPlayerController());
    if (Player && (Player->IsMainMenuOpen() || Player->IsDrillDialogOpen()))
    {
        Reject(TEXT("UOpenGolf menu is open; shot ignored"));
        return;
    }
    const auto* OnlinePermission = GetGameInstance()->GetSubsystem<UOpenGolfOnlineSubsystem>();
    if (!(OnlinePermission && OnlinePermission->HasPermission(TEXT("ogolf:admin"))) &&
        !(LaunchMonitor && ShotRecording::IsRecordable(Shot, LaunchMonitor->GetConnectionState())))
    {
        Reject(TEXT("Connect a launch monitor to hit shots"));
        return;
    }
    FOpenFairwayLaunch CheckedLaunch;
    if (!Ball || !ToLaunch(Shot, CheckedLaunch))
    {
        Reject(TEXT("Shot needs ball speed and vertical launch angle"));
        return;
    }
    if (IsCourseRound() && Round.bHoled)
    {
        Reject(TEXT("The hole is complete"));
        return;
    }
    if (PlayMode == EOpenGolfPlayMode::Range && Training)
    {
        const FString DrillReason = Training->RejectReason();
        if (!DrillReason.IsEmpty())
        {
            Reject(*DrillReason);
            return;
        }
        Training->BeforeShot();
    }
    if (PlayMode == EOpenGolfPlayMode::Range)
        ResetForPlay();
    else if (Ball && Ball->IsMoving())
    {
        // A shot arriving mid-roll is hit from where the ball is now; the interrupted shot still counts.
        Round.RecordStroke(bShotFromGreen, false);
        Ball->PlaceAt(Ball->GetActorLocation());
    }
    else if (!bAddressReady)
        ReadyForNextShot();
    GetWorldTimerManager().ClearTimer(AddressTimer);
    // A course lie takes its penalty off the measured launch (Docs/Architecture.md); the recorders keep the shot as measured.
    const FOpenGolfLieCut LieCut = GetLieCut();
    const FOpenFairwayLaunch PlayedLaunch = OpenGolfPlay::ApplyLieCut(CheckedLaunch, LieCut);
    if (!LieCut.IsNone() && LaunchMonitor)
        UE_LOG(LogUOpenGolfGameMode, Log, TEXT("Lie %s, %s: ball speed -%.0f%%, spin -%.0f%%."), *UEnum::GetValueAsString(Ball->GetResult().Surface),
            *UOpenLaunchMonitorSubsystem::GetClubDisplayName(LaunchMonitor->GetClub()).ToString(), LieCut.BallSpeedPct, LieCut.SpinPct);
    // Only Training shots are recorded; a course round never records.
    if (Recorder && LaunchMonitor && PlayMode == EOpenGolfPlayMode::Range)
    {
        // The club selected now is the club this shot was hit with.
        Recorder->Configure(GolfSettings::RecordingPath(GolfSettings::Get()), GolfSettings::Get().RecordingDetail != 0);
        Recorder->OnShot(Shot, LaunchMonitor->GetConnectionState(), LaunchMonitor->GetClub(), LaunchMonitor->IsLeftHanded(), LaunchMonitor->GetFirmware());
    }

    if (CloudRecorder && LaunchMonitor)
    {
        auto* Online = GetGameInstance()->GetSubsystem<UOpenGolfOnlineSubsystem>();
        OpenGolfOnline::FShotOwner ShotOwner;
        // Recording (F9) saves shots; while signed in they go to the online profile as well as to local files.
        if (Online) { ShotOwner.Key=Online->GetAccountKey(); ShotOwner.bSave=Online->HasSession() && IsRecordingShots(); }
        CloudRecorder->Begin(Shot,LaunchMonitor->GetClub(),ShotOwner,PlayMode==EOpenGolfPlayMode::Range && !Round.bPractice,
            ShotRecording::IsRecordable(Shot,LaunchMonitor->GetConnectionState()));
    }
    // Range mode hits from the tee; Hole mode from wherever the ball lies.
    PreviousLie = Ball->GetActorLocation();
    if (Ball->Launch(PlayedLaunch))
    {
        bAddressReady = false;
        FOpenFairwayShotResult Launched;
        bShotFromGreen = Ball->GetShotResult(Launched) && Launched.Surface == EOpenFairwaySurface::Green;
        FOpenFairwayShotResult Result;
        if (CloudRecorder && Ball->GetShotResult(Result))
            CloudRecorder->BindSimulation(Shot.ShotNumber,Result.ShotNumber);
        if (Recorder && Ball->GetShotResult(Result))
            Recorder->BindSimulation(Shot.ShotNumber, Result.ShotNumber);
        if (auto* PresentedBall = Cast<AUOpenGolfBall>(Ball))
            PresentedBall->StartShotTrail();
    }
    else
    {
        Reject(TEXT("The ball could not be launched"));
        if (CloudRecorder) CloudRecorder->Cancel();
        if (Recorder)
            Recorder->CancelSimulation();
    }
}

void AUOpenGolfGameMode::HandleClubData(const FLaunchMonitorClubData& ClubData)
{
    if (CloudRecorder) CloudRecorder->OnClubData(ClubData);
    if (Recorder)
    {
        Recorder->OnClubData(ClubData);
    }
}

void AUOpenGolfGameMode::StartSessionDefaults()
{
    SessionGimmieFeet = GolfSettings::Get().GimmieFeet;
    if (Ball) Ball->GreenStimpFeet = GolfSettings::Get().Stimp;
    // A course round plays its own fixed wind (the range holds it there); Training starts from the preference.
    if (Range && Course) Range->SetWind(Course->WindMph, Course->WindFromDeg);
    else if (Range) Range->SetWind(GolfSettings::Get().WindMph >= 0 ? GolfSettings::Get().WindMph : MapWindMph, MapWindFromDeg);
    if (!GimmieIndicator)
    {
        // The game mode is an AInfo, hidden in game, so its own components never render; the ring gets a visible host.
        FActorSpawnParameters Params;
        Params.ObjectFlags |= RF_Transient;
        AActor* Host = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
        GimmieIndicator = NewObject<UGolfGimmieIndicator>(Host, TEXT("GimmieRing"));
        Host->SetRootComponent(GimmieIndicator);
        GimmieIndicator->RegisterComponent();
    }
}
