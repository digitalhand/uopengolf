// SPDX-License-Identifier: MIT
// Hunter Ranch in Play: every hole's tees and cup sit on playable ground, a round runs 1 to 18,
// and a ball that stops in the lake costs a penalty stroke and is replayed from where it was hit.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GolfCourseDefinition.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenGolfTerrain.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Play/OpenGolfPlayRules.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/PermissionTestAccess.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfRange.h"

namespace HunterRoundTestDetail
{
    const TCHAR* const CoursePath = TEXT("/HunterRanch/Course/DA_HunterRanch.DA_HunterRanch");
    const TCHAR* const MapPath = TEXT("/HunterRanch/Course/Course");

    AUOpenGolfGameMode* PlayingMode()
    {
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        return Mode && Mode->GetBall() && Mode->GetRange() && Mode->GetRange()->Terrain ? Mode : nullptr;
    }

    /** The region the terrain draws at PointM, named "<surface>|<source id>"; a later region is drawn over an earlier one. */
    FString RegionAt(const UOpenGolfTerrainDefinition& Definition, FVector2D PointM)
    {
        for (int32 I = Definition.Regions.Num() - 1; I >= 0; --I)
            if (UOpenGolfTerrainDefinition::Contains(Definition.Regions[I].OutlineM, PointM))
                return Definition.Regions[I].Name.ToString();
        return TEXT("rough");
    }

    /** Ground and surface checks for one hole's cup and every tee set. */
    void CheckHole(FAutomationTestBase& Test, const AUOpenGolfGameMode& Mode, int32 Number)
    {
        const UGolfCourseDefinition& Course = *Mode.GetCourse();
        const FGolfCourseHole& Hole = Course.Layout[Number - 1];
        const AOpenGolfTerrainArea& Terrain = *Mode.GetRange()->Terrain;
        const FString Name = FString::Printf(TEXT("Hole %d"), Number);
        const UOpenGolfSurfacePreset* CupSurface = Terrain.Definition->SurfaceAt(FVector2D(Hole.CupCm) / 100);
        Test.TestTrue(Name + TEXT(": cup is on the green"), CupSurface && CupSurface->Surface == EOpenFairwaySurface::Green);
        FVector Ground, Normal;
        Test.TestTrue(Name + TEXT(": ground under the cup"), Terrain.GroundAt(Hole.CupCm, Ground, Normal));
        for (const FGolfCourseTee& Tee : Hole.Tees)
        {
            const FString TeeName = FString::Printf(TEXT("%s %s tee"), *Name, *Tee.SetName.ToString());
            Test.TestEqual(TeeName + TEXT(" is dry and in bounds"), Course.PenaltyAt(Tee.PositionCm), EGolfPenalty::None);
            Test.TestTrue(TeeName + TEXT(" has ground"), Terrain.GroundAt(Tee.PositionCm, Ground, Normal));
            const FString Region = RegionAt(*Terrain.Definition, FVector2D(Tee.PositionCm) / 100);
            Test.TestTrue(FString::Printf(TEXT("%s is on a tee pad, not %s"), *TeeName, *Region), Region.StartsWith(TEXT("tee|")));
        }
        const FVector Aim = Hole.RoutingCm.IsEmpty() ? Hole.CupCm : Hole.RoutingCm[0];
        const FGolfCourseTee* Active = Hole.Tees.FindByPredicate([&](const FGolfCourseTee& T)
            { return FVector::Dist2D(T.PositionCm, Mode.GetRange()->GetTeeLocation()) < 1; });
        if (Test.TestNotNull(Name + TEXT(": ball tees off from a mapped tee"), Active))
            Test.TestEqual(Name + TEXT(": opening aim follows the route"), Mode.GetOpeningAimDeg(),
                OpenGolfPlay::BearingDeg(Active->PositionCm, Aim, 0), 0.01f);
    }

    class FCheckHunterRound : public IAutomationLatentCommand
    {
    public:
        explicit FCheckHunterRound(FAutomationTestBase* InTest) : Test(InTest) {}
        virtual bool Update() override
        {
            if (GetCurrentRunTime() > 200)
            {
                Test->AddError(TEXT("Timed out waiting for the Hunter Ranch round."));
                return true;
            }
            AUOpenGolfGameMode* Mode = PlayingMode();
            if (!Mode) return false;
            if (!Submitted)
            {
                const auto* Course = LoadObject<UGolfCourseDefinition>(nullptr, CoursePath);
                if (!Test->TestNotNull(TEXT("Course loads"), Course) || !Test->TestEqual(TEXT("Hole count"), Course->Layout.Num(), 18)) return true;
                Mode->SetPlayMode(EOpenGolfPlayMode::Hole, Course);
            }
            if (Submitted == 18 && Mode->GetCompletedHoles().Num() < 18)
            {
                // Hole 18 is holed, but its score joins the card only after the hole-out pause.
                Test->TestFalse(TEXT("The round completes only once the last hole is on the card"), Mode->IsRoundComplete());
                return false;
            }
            if (Submitted == 18 && Mode->GetCompletedHoles().Num() == 18)
            {
                Test->TestTrue(TEXT("Last cup ends the round"), Mode->IsRoundComplete());
                for (const auto& Score : Mode->GetCompletedHoles())
                    Test->TestEqual(TEXT("A duplicate rest adds no stroke"), Score.Strokes, 1);
                return true;
            }
            if (Mode->GetHoleNumber() != Submitted + 1) return false;
            const auto& Hole = Mode->GetCourse()->Layout[Submitted];
            Test->TestEqual(TEXT("Active par"), Mode->GetPar(), Hole.Par);
            Test->TestEqual(TEXT("All published tee sets are present"), Hole.Tees.Num(), 5);
            Test->TestTrue(TEXT("Ball starts on the mapped tee"), Mode->IsBallOnTee());
            Test->TestTrue(TEXT("Cup follows the active green"), FVector::Dist2D(Mode->GetCupLocation(), Hole.CupCm) < 1);
            Test->TestTrue(TEXT("Round is open before hole out"), !Mode->IsRoundComplete());
            CheckHole(*Test, *Mode, Submitted + 1);
            FOpenFairwayShotResult Rest;
            Rest.ShotNumber = ++Submitted;
            Rest.bHoled = true;
            Mode->GetBall()->OnBallAtRest.Broadcast(Rest);
            Mode->GetBall()->OnBallAtRest.Broadcast(Rest);
            return false;
        }
    private:
        FAutomationTestBase* Test;
        int32 Submitted = 0;
    };

    /** A dry, level spot beside the lake and a point well inside it, straight across the lake's edge. */
    bool FindLakeShot(const UGolfCourseDefinition& Course, const AOpenGolfTerrainArea& Terrain, FVector& From, FVector& Into)
    {
        if (Course.Water.IsEmpty())
            return false;
        const TArray<FVector2D>& Lake = Course.Water[0].OutlineCm;
        FVector2D Centre = FVector2D::ZeroVector;
        for (const FVector2D& P : Lake)
            Centre += P / Lake.Num();
        for (const FVector2D& Edge : Lake)
        {
            const FVector2D Out = (Edge - Centre).GetSafeNormal();
            const FVector Dry(Edge + Out * 400, 0), Wet(Edge - Out * 500, 0);
            FVector Ground, Normal;
            if (Course.PenaltyAt(Dry) == EGolfPenalty::None && Course.PenaltyAt(Wet) == EGolfPenalty::Water &&
                Course.PenaltyAt(FVector(Edge + Out * 1000, 0)) == EGolfPenalty::None &&
                Terrain.GroundAt(Dry, Ground, Normal) && Normal.Z > 0.985)
            {
                From = Ground;
                Into = Wet;
                return true;
            }
        }
        return false;
    }

    class FCheckLakePenalty : public IAutomationLatentCommand
    {
    public:
        explicit FCheckLakePenalty(FAutomationTestBase* InTest) : Test(InTest) {}
        virtual bool Update() override
        {
            if (GetCurrentRunTime() > 120)
            {
                Test->AddError(FString::Printf(TEXT("Timed out at step %d of the lake penalty."), Step));
                return true;
            }
            AUOpenGolfGameMode* Mode = PlayingMode();
            if (!Mode) return false;
            AOpenFairwayBall* Ball = Mode->GetBall();
            UWorld* World = Mode->GetWorld();
            if (Step == 0)
            {
                const auto* Course = LoadObject<UGolfCourseDefinition>(nullptr, CoursePath);
                if (!Test->TestNotNull(TEXT("Course loads"), Course)) return true;
                Mode->SetPlayMode(EOpenGolfPlayMode::Hole, Course);
                if (!Test->TestTrue(TEXT("A dry spot beside the lake"), FindLakeShot(*Course, *Mode->GetRange()->Terrain, From, Into)))
                    return true;
                Ball->PlaceAt(From + FVector::UpVector * 2.15);
                Ball->AimYawDeg = OpenGolfPlay::BearingDeg(From, Into, 0);
                ++Step;
                return false;
            }
            if (Step == 1)
            {
                StrokesBefore = Mode->GetHoleRound().Strokes;
                FOpenGolfPermissionTestAccess::Set(World);
                // A soft chip that carries about 9 m and stops on the flat lake floor.
                auto* Monitor = World->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>();
                Test->TestTrue(TEXT("The chip is accepted"), Monitor->InjectCustomShot(11.0f, 30.0f, 0.0f, 4000, 0.0f));
                ShotNumber = Ball->GetShotNumber();
                ++Step;
                return false;
            }
            FOpenFairwayShotResult Result;
            if (!Ball->GetShotResult(Result) || Result.ShotNumber != ShotNumber || !Result.bAtRest || Mode->GetRecentPenalty() == EGolfPenalty::None)
                return false;
            Test->TestEqual(TEXT("The lake is the reason"), Mode->GetRecentPenalty(), EGolfPenalty::Water);
            Test->TestEqual(TEXT("The chip and one penalty stroke count"), Mode->GetHoleRound().Strokes, StrokesBefore + 2);
            Test->TestTrue(FString::Printf(TEXT("The ball is replayed from where it was hit (%.1f cm away)"),
                FVector::Dist2D(Ball->GetActorLocation(), From)), FVector::Dist2D(Ball->GetActorLocation(), From) < 5);
            return true;
        }
    private:
        FAutomationTestBase* Test;
        int32 Step = 0;
        int32 StrokesBefore = 0;
        int32 ShotNumber = 0;
        FVector From = FVector::ZeroVector, Into = FVector::ZeroVector;
    };
} // namespace HunterRoundTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHunterRoundTest, "UOpenGolf.Course.HunterRanch.Round",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHunterRoundTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(HunterRoundTestDetail::MapPath, false, true))
        return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HunterRoundTestDetail::FCheckHunterRound>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHunterWaterTest, "UOpenGolf.Course.HunterRanch.Water",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHunterWaterTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(HunterRoundTestDetail::MapPath, false, true))
        return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HunterRoundTestDetail::FCheckLakePenalty>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
