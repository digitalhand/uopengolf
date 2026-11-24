// SPDX-License-Identifier: MIT
// Lie penalties and the automatic club in a course round of the saved map: no penalty on the tee, the rough's penalty
// on a launch-monitor shot, the putter and the greenside bunker, and nothing of either in Training.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Play/OpenGolfLieRules.h"
#include "Course/CourseClubs.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/BundledCourse.h"
#include "Tests/LieSearch.h"
#include "Tests/PermissionTestAccess.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfRange.h"

namespace CourseLieTestDetail
{
    /** A measured 7 iron as the custom-shot panel sends it: 120 mph, 16°, 7000 rpm. */
    constexpr float ShotSpeedMps = 53.6448f;
    constexpr float ShotLaunchDeg = 16.0f;
    constexpr int32 ShotSpinRpm = 7000;
    /** A 25 mph pitch that stops within seconds, even from the rough. */
    constexpr float ChipSpeedMps = 11.176f;
    constexpr float ChipLaunchDeg = 20.0f;
    constexpr int32 ChipSpinRpm = 3000;

    bool SameCut(const FOpenGolfLieCut& A, const FOpenGolfLieCut& B)
    {
        return FMath::IsNearlyEqual(A.BallSpeedPct, B.BallSpeedPct) && FMath::IsNearlyEqual(A.SpinPct, B.SpinPct);
    }
} // namespace CourseLieTestDetail

class FVerifyCourseLie : public IAutomationLatentCommand
{
public:
    explicit FVerifyCourseLie(FAutomationTestBase* InTest) : Test(InTest) {}

    virtual bool Update() override
    {
        using namespace CourseLieTestDetail;
        using EClub = ELaunchMonitorClub;
        if (GetCurrentRunTime() > 120.0)
        {
            Test->AddError(FString::Printf(TEXT("Timed out at stage %d."), Stage));
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
        auto* Monitor = World ? World->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>() : nullptr;
        if (!Mode || !Ball || !Monitor || !Mode->GetRange() || !Mode->GetRange()->Terrain)
            return false;
        const double Now = World->GetTimeSeconds();
        switch (Stage)
        {
        case 0:
            break;
        case 1:
            // A club the player picks while the stopped shot is shown is kept when the ball turns to the cup.
            if (Ball->IsMoving())
                return false;
            Monitor->SetClub(EClub::LobWedge);
            RestedAt = Now;
            ++Stage;
            return false;
        case 2:
            if (Now - RestedAt < AUOpenGolfGameMode::AddressDelayS + 0.5)
                return false;
            Test->TestEqual(TEXT("Auto club keeps a club picked while the shot was shown"), Monitor->GetClub(), EClub::LobWedge);
            // Untouched, the club in hand is replaced by the club for the new lie.
            Monitor->SetClub(EClub::Iron7);
            // The synthetic sign-in that lets a test hit shots can lapse while a shot rolls out; set it for this one.
            FOpenGolfPermissionTestAccess::Set(World);
            Test->TestTrue(TEXT("The second chip is hit"), Monitor->InjectCustomShot(ChipSpeedMps, ChipLaunchDeg, 0, ChipSpinRpm, 0)
                && Ball->IsMoving());
            ++Stage;
            return false;
        case 3:
            if (Ball->IsMoving())
                return false;
            RestedAt = Now;
            ++Stage;
            return false;
        case 4:
        {
            if (Now - RestedAt < AUOpenGolfGameMode::AddressDelayS + 0.5)
                return false;
            CourseClubs::FAdvice Advice;
            Advice.DistanceToCupM = Mode->GetDistanceToCupM();
            Advice.Surface = Ball->GetResult().Surface;
            Advice.bOnTee = Mode->IsBallOnTee();
            Advice.Rules = &BundledCourse::Load()->GetLieRules();
            Test->TestEqual(TEXT("Untouched, auto club picks the club for the new lie"), Monitor->GetClub(), CourseClubs::Recommend(Advice));
            FinishInTraining(*Mode, *Ball, *Monitor);
            return true;
        }
        default:
            return true;
        }
        PlayersClub = Monitor->GetClub();
        FOpenGolfPermissionTestAccess::Set(World);
        const UGolfCourseDefinition* Course = BundledCourse::Load();
        if (!Test->TestNotNull(TEXT("The bundled course loads"), Course))
            return true;
        const UOpenGolfLieRules& Rules = Course->LieRules ? *Course->LieRules : *GetDefault<UOpenGolfLieRules>();

        // The round starts on the tee: no penalty, and the 400 yd hole is beyond every club, so the driver is picked.
        Monitor->SetClub(EClub::Iron7);
        Mode->SetPlayMode(EOpenGolfPlayMode::Hole, Course);
        Test->TestTrue(TEXT("The round starts on the tee box"), Mode->IsBallOnTee());
        Test->TestTrue(TEXT("No penalty on the tee"), Mode->GetLieCut().IsNone());
        Test->TestEqual(TEXT("Auto club picks the driver on the tee"), Monitor->GetClub(), EClub::Driver);

        const FVector Cup = Mode->GetCupLocation();
        FVector Bunker;
        bRough = LieSearch::FindApproachRough(*Mode, Rough);
        const bool bBunker = LieSearch::FindGreensideBunker(*Mode, Bunker);
        Test->TestTrue(TEXT("The hole has rough away from the green"), bRough);
        Test->TestTrue(TEXT("The hole has a greenside bunker"), bBunker);

        // The same measured shot from the tee and from the rough: the rough takes its share of speed and spin.
        Mode->ResetBallToTee();
        Monitor->SetClub(EClub::Iron7);
        FOpenFairwayShotResult FromTee, FromRough;
        Test->TestTrue(TEXT("The tee shot is hit"), Monitor->InjectCustomShot(ShotSpeedMps, ShotLaunchDeg, 0, ShotSpinRpm, 0) && Ball->GetShotResult(FromTee));
        if (bRough)
        {
            Ball->PlaceAt(Rough);
            Monitor->SetClub(EClub::Iron7);
            const FOpenGolfLieCut Expected = Rules.CutFor(EOpenFairwaySurface::Rough, Mode->GetDistanceToCupM(), EOpenGolfClubGroup::Iron);
            Test->TestFalse(TEXT("The rough is penalised"), Expected.IsNone());
            Test->TestTrue(TEXT("The round reads the rough's penalty for an iron"), SameCut(Mode->GetLieCut(), Expected));
            Test->TestTrue(TEXT("The rough shot is hit"), Monitor->InjectCustomShot(ShotSpeedMps, ShotLaunchDeg, 0, ShotSpinRpm, 0) && Ball->GetShotResult(FromRough));
            Test->TestEqual(TEXT("The rough takes its share of ball speed"), FromRough.Launch.BallSpeedMph / FromTee.Launch.BallSpeedMph,
                Expected.BallSpeedScale(), 1e-3f);
            Test->TestEqual(TEXT("The rough takes its share of spin"), FromRough.Launch.TotalSpinRpm / FromTee.Launch.TotalSpinRpm, Expected.SpinScale(), 1e-3f);
            Test->TestEqual(TEXT("The launch angle stays measured"), FromRough.Launch.VerticalLaunchDeg, FromTee.Launch.VerticalLaunchDeg);

            Ball->PlaceAt(Rough);
            Monitor->SetClub(EClub::Putter);
            Test->TestTrue(TEXT("A putt from the rough has no penalty"), Mode->GetLieCut().IsNone());
        }
        if (bBunker)
        {
            Ball->PlaceAt(Bunker);
            Monitor->SetClub(EClub::SandWedge);
            Test->TestTrue(TEXT("A greenside bunker is an explosion shot"), SameCut(Mode->GetLieCut(),
                Rules.CutFor(EOpenFairwaySurface::Sand, Mode->GetDistanceToCupM(), EOpenGolfClubGroup::Wedge)));
            Monitor->SetClub(EClub::Iron7);
            Mode->SelectClubForLie();
            Test->TestEqual(TEXT("Auto club picks the sand wedge in a greenside bunker"), Monitor->GetClub(), EClub::SandWedge);
        }
        FVector Green, Normal;
        if (Mode->GetRange()->Terrain->GroundAt(Cup - FVector(300, 0, 0), Green, Normal))
        {
            Ball->PlaceAt(Green + FVector::UpVector * 10.0f);
            Mode->SelectClubForLie();
            Test->TestEqual(TEXT("Auto club picks the putter on the green"), Monitor->GetClub(), EClub::Putter);
        }

        if (!bRough)
        {
            FinishInTraining(*Mode, *Ball, *Monitor);
            return true;
        }
        // A chip from the rough that stops by itself, as a played shot does.
        Ball->PlaceAt(Rough);
        Monitor->SetClub(EClub::Iron7);
        Test->TestTrue(TEXT("The chip is hit"), Monitor->InjectCustomShot(ChipSpeedMps, ChipLaunchDeg, 0, ChipSpinRpm, 0) && Ball->IsMoving());
        Stage = 1;
        return false;
    }

private:
    /** Training never penalises a lie or changes the club; then the player's club is put back. */
    void FinishInTraining(AUOpenGolfGameMode& Mode, AOpenFairwayBall& Ball, UOpenLaunchMonitorSubsystem& Monitor)
    {
        Ball.PlaceAt(Ball.GetActorLocation());
        Mode.SetPlayMode(EOpenGolfPlayMode::Range);
        if (bRough)
        {
            Test->TestTrue(TEXT("A Training ball is placed in the rough"), Mode.PlaceBallAt(Rough));
            Monitor.SetClub(ELaunchMonitorClub::Iron7);
            Test->TestTrue(TEXT("Training has no lie penalty"), Mode.GetLieCut().IsNone());
            Mode.SelectClubForLie();
            Test->TestEqual(TEXT("Training keeps the player's club"), Monitor.GetClub(), ELaunchMonitorClub::Iron7);
            Mode.ResetBallToTee();
        }
        Monitor.SetClub(PlayersClub);
    }

    FAutomationTestBase* Test;
    int32 Stage = 0;
    double RestedAt = 0.0;
    bool bRough = false;
    FVector Rough = FVector::ZeroVector;
    ELaunchMonitorClub PlayersClub = ELaunchMonitorClub::Driver;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourseLieTest, "UOpenGolf.Course.LiePenalty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCourseLieTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FVerifyCourseLie>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
