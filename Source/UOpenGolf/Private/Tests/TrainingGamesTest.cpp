// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Hole/OpenGolfCupComponent.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenGolfSettings.h"
#include "Tests/PermissionTestAccess.h"
#include "OpenGolfFlagstickComponent.h"
#include "OpenGolfTerrain.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Training/DistanceLadder.h"
#include "Training/DistancePractice.h"
#include "Training/TargetRings.h"
#include "UOpenGolfBall.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"

namespace TrainingGamesTestDetail
{
    constexpr double CmPerYard = 91.44;

    /**
     * The edges draw with the screen-footprint line material and carry what it needs: UV1 (0 on the inner row, 1 on the
     * outer row; the edge width) and UV2 (the radial direction). The fill keeps the tracer material.
     */
    void CheckRingLines(FAutomationTestBase& Test, UProceduralMeshComponent& Mesh, const FVector& Pin, double Edge)
    {
        auto ParentPath = [&Mesh](int32 Section)
        {
            const auto* Tint = Cast<UMaterialInstanceDynamic>(Mesh.GetMaterial(Section));
            return Tint && Tint->Parent ? Tint->Parent->GetPathName() : FString();
        };
        Test.TestEqual(TEXT("The fill keeps the tracer material"), ParentPath(0), FString(TEXT("/Game/UOpenGolf/Art/M_ShotTrail.M_ShotTrail")));
        const double InnerRadius[] = {91.44, 182.88};
        for (int32 Ring = 0; Ring < 2; ++Ring)
        {
            const int32 Section = Ring == 0 ? 1 : 3;
            Test.TestEqual(TEXT("Edges use the screen-footprint line material"), ParentPath(Section),
                FString(TEXT("/Game/UOpenGolf/Art/M_TargetRingLine.M_TargetRingLine")));
            int32 BadRows = 0, BadDirections = 0;
            for (const FProcMeshVertex& Vertex : Mesh.GetProcMeshSection(Section)->ProcVertexBuffer)
            {
                const double Radius = FVector::Dist2D(Pin, Vertex.Position);
                const bool bRow = (Vertex.UV1.X == 0 || Vertex.UV1.X == 1) && FMath::IsNearlyEqual(Vertex.UV1.Y, Edge, .01) &&
                                  FMath::IsNearlyEqual(Radius, InnerRadius[Ring] + Vertex.UV1.X * Edge, 1.);
                const FVector2D Away = FVector2D(Vertex.Position - Pin).GetSafeNormal();
                BadRows += !bRow;
                BadDirections += !(FMath::IsNearlyEqual(Vertex.UV2.Size(), 1., 1e-3) && FVector2D::DotProduct(Vertex.UV2, Away) > .99);
            }
            double TopZ = -UE_BIG_NUMBER;
            for (const FProcMeshVertex& Vertex : Mesh.GetProcMeshSection(Section)->ProcVertexBuffer)
                TopZ = FMath::Max(TopZ, Vertex.Position.Z);
            Test.TestTrue(FString::Printf(TEXT("Ring %d bounds clear the edges' screen growth"), Ring), Mesh.Bounds.GetBox().Max.Z - TopZ >= 50.);
            Test.TestEqual(FString::Printf(TEXT("Ring %d rows carry t and the edge width"), Ring), BadRows, 0);
            Test.TestEqual(FString::Printf(TEXT("Ring %d vertices carry their radial direction"), Ring), BadDirections, 0);
        }
    }

    /** The 3 ft and 6 ft rings and their labels around the pin, clear of the grass. */
    void CheckTarget(FAutomationTestBase& Test, const UTargetDrill& Drill, const AUOpenGolfRange& Range, const FVector& Tee, const FVector& Pin)
    {
        const UTargetRings* Rings = Drill.GetRings();
        UProceduralMeshComponent* Mesh = Rings ? Rings->GetMesh() : nullptr;
        if (!Test.TestTrue(TEXT("Fill, 3 ft edge, gap and 6 ft edge"), Mesh && Mesh->GetNumSections() == 4))
            return;
        const float Edge = UTargetRings::EdgeWidthCm(FVector::Dist2D(Tee, Pin));
        const double Expected[] = {91.44, 91.44 + Edge, 0, 182.88 + Edge};
        for (int32 Section = 0; Section < 4; ++Section)
        {
            const FProcMeshSection* Band = Mesh->GetProcMeshSection(Section);
            double Outer = 0;
            for (const uint32 Index : Band->ProcIndexBuffer)
                Outer = FMath::Max(Outer, FVector::Dist2D(Pin, Band->ProcVertexBuffer[Index].Position));
            Test.TestEqual(FString::Printf(TEXT("Band %d outer radius"), Section), Outer, Expected[Section], 1.);
        }
        CheckRingLines(Test, *Mesh, Pin, Edge);
        const FVector Vertex = Mesh->GetProcMeshSection(1)->ProcVertexBuffer[0].Position;
        FVector Ground, Normal;
        Test.TestTrue(TEXT("The rings stand clear of 55 mm rough"),
            Range.Terrain->GroundAt(Vertex, Ground, Normal, AUOpenGolfBall::ContactChannel) && Vertex.Z - Ground.Z >= 5.5);
        const auto Labels = Rings->GetLabels();
        if (!Test.TestEqual(TEXT("Two labels"), Labels.Num(), 2))
            return;
        const FVector Line = (Pin - Tee).GetSafeNormal2D();
        for (int32 Index = 0; Index < 2; ++Index)
        {
            const UTextRenderComponent* Label = Labels[Index];
            const double Radius = FVector::Dist2D(Pin, Label->GetComponentLocation());
            Test.TestEqual(TEXT("Label text"), Label->Text.ToString(), FString(Index == 0 ? TEXT("3 ft") : TEXT("6 ft")));
            Test.TestTrue(TEXT("Label shown"), Label->IsVisible());
            Test.TestTrue(TEXT("Each label lies inside its own ring"), Index == 0 ? Radius < 91.44 : Radius > 91.44 + Edge && Radius < 182.88);
            Test.TestTrue(TEXT("Labels sit on the tee side"), FVector::DotProduct(Label->GetComponentLocation() - Pin, Line) < 0);
            Test.TestTrue(TEXT("Labels lie face up"), Label->GetForwardVector().Z > .9);
            Test.TestTrue(TEXT("Labels read upright from the tee"), FVector::DotProduct(Label->GetUpVector(), Line) > .9);
        }
    }

    double PinYards(const AUOpenGolfGameMode& Mode, const FVector& Tee)
    {
        return FVector::Dist2D(Tee, Mode.GetCupLocation()) / CmPerYard;
    }

    /**
     * Choosing a game while another runs opens the Exit Drill prompt; the running game keeps going until the player
     * exits, and Exit Drill then starts the chosen game with its tries. Leaves Distance Practice with 5 tries running.
     */
    void CheckSwitchGames(FAutomationTestBase& Test, ATrainingSession& Session, const AUOpenGolfGameMode& Mode,
        AUOpenGolfPlayerController& Player, const FVector& Tee)
    {
        if (!Test.TestTrue(TEXT("Start Distance Practice to switch from"), Session.StartDrill(UDistancePractice::StaticClass(), 3)))
            return;
        auto* Practice = CastChecked<UDistancePractice>(Session.Active());
        Test.TestTrue(TEXT("Pick 30 yd"), Practice->SetDistance(30));
        const TWeakObjectPtr<UProceduralMeshComponent> PracticeRings = Practice->GetRings()->GetMesh();
        Test.TestFalse(TEXT("Another game never starts over a running one unasked"), Session.StartDrill(UDistanceLadder::StaticClass(), 5));
        if (!Test.TestTrue(TEXT("Choosing the Ladder asks first"), Session.ChooseDrill(UDistanceLadder::StaticClass(), 5)))
            return;
        Test.TestTrue(TEXT("The Exit Drill prompt opens"), Player.IsDrillDialogOpen());
        Test.TestTrue(TEXT("Distance Practice runs until the player exits"), Session.Active() == Practice);
        Player.CloseDrillDialog();
        Test.TestFalse(TEXT("Closing the prompt forgets the choice"), Session.GetPendingDrill().IsSet());
        Test.TestTrue(TEXT("Distance Practice keeps running"), Session.Active() == Practice && Practice->GetDistance() == 30);
        Session.ChooseDrill(UDistanceLadder::StaticClass(), 5);
        if (!Test.TestTrue(TEXT("Exit Drill starts the chosen game"), Session.StartPendingDrill()))
            return;
        Player.CloseDrillDialog();
        const auto* Ladder = Cast<UDistanceLadder>(Session.Active());
        if (!Test.TestNotNull(TEXT("The Ladder runs"), Ladder))
            return;
        Test.TestEqual(TEXT("With the tries chosen for it"), Ladder->GetTries(), 5);
        Test.TestFalse(TEXT("The practice rings are gone"), PracticeRings.IsValid() && PracticeRings->IsRegistered());
        Test.TestEqual(TEXT("The Ladder starts at 15 yd"), Ladder->GetPinYards(), 15);
        Test.TestEqual(TEXT("The pin moves to the Ladder's target"), int32(FMath::RoundToInt(PinYards(Mode, Tee))), 15);
        Session.ChooseDrill(UDistancePractice::StaticClass(), 5);
        Test.TestTrue(TEXT("Exit Drill starts Distance Practice again"), Session.StartPendingDrill());
        Player.CloseDrillDialog();
        const auto* Again = Cast<UDistancePractice>(Session.Active());
        Test.TestTrue(TEXT("A fresh Distance Practice with 5 tries"), Again && Again->GetTries() == 5 && Again->GetSet().Shots == 0);
    }

    class FPlayDistancePractice : public IAutomationLatentCommand
    {
    public:
        ~FPlayDistancePractice()
        {
            if (SavedPreferences) GolfSettings::Commit(SavedPreferences.GetValue(), GEditor->PlayWorld);
        }
        explicit FPlayDistancePractice(FAutomationTestBase& InTest) : Test(InTest) {}
        bool Update() override
        {
            if (GetCurrentRunTime() > 90)
            {
                Test.AddError(FString::Printf(TEXT("Timed out at drill step %d"), Step));
                return true;
            }
            UWorld* World = GEditor->PlayWorld;
            auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
            auto* Player = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
            if (!Mode || !Player || !Mode->GetRange() || !Mode->GetTrainingSession() || !Mode->GetBall())
                return false;
            auto* Session = Mode->GetTrainingSession();
            auto* Range = Mode->GetRange();
            auto* Ball = Mode->GetBall();
            const FVector Tee = Range->GetTeeLocation();
            const auto Game = UDistancePractice::StaticClass();
            FOpenFairwayLaunch Chip;
            Chip.BallSpeedMph = 23;
            Chip.VerticalLaunchDeg = 24;
            switch (Step)
            {
            case 0:
                OriginalPin = Mode->GetCupLocation();
                Test.TestTrue(TEXT("Free Training permits a practice lie"), Mode->PlaceBallAt(Tee + FVector(500, 0, 0)));
                Test.TestTrue(TEXT("Hit a free-practice shot"), Ball->Launch(Chip));
                break;
            case 1:
            {
                if (Ball->IsMoving())
                    return false;
                Test.TestFalse(TEXT("Only 3 or 5 tries"), Session->StartDrill(Game, 4));
                if (!Test.TestTrue(TEXT("Start Distance Practice"), Session->StartDrill(Game, 3)))
                    return true;
                auto* Drill = CastChecked<UDistancePractice>(Session->Active());
                Test.TestEqual(TEXT("Start at 25 yd"), Drill->GetDistance(), 25);
                Test.TestTrue(TEXT("Starting cancels the practice lie"), Mode->IsBallAtAddress() && !Mode->IsPractice());
                Player->SetPlacingBall(true);
                Test.TestFalse(TEXT("Placement shortcut is blocked"), Player->IsPlacingBall());
                Test.TestFalse(TEXT("Placement is blocked in gameplay"), Mode->PlaceBallAt(Tee + FVector(500, 0, 0)));
                Test.TestEqual(TEXT("The free-practice shot survives entry"), Session->FinishedBallCount(), 1);
                for (const int32 Yards : {10, 250, 25})
                {
                    Test.TestTrue(TEXT("Set the yardage"), Drill->SetDistance(Yards));
                    const FVector Pin = Mode->GetCupLocation();
                    Test.TestEqual(TEXT("Horizontal distance to the tee"), FVector::Dist2D(Tee, Pin), Yards * 91.44, .1);
                    Test.TestTrue(TEXT("The displayed flag and physics cup agree"), Range->GetFlagstick()->GetComponentLocation().Equals(Pin, .1) && Ball->GetCupLocations().Num() == 1 && Ball->GetCupLocations()[0].Equals(Pin, .1));
                    Test.TestTrue(TEXT("The overhead image keeps its original anchor"), OriginalPin.Equals(Range->GetOriginalCup().Position, .1));
                    CheckTarget(Test, *Drill, *Range, Tee, Pin);
                }
                Test.TestTrue(TEXT("The drill starts aimed at the pin"), FMath::IsNearlyEqual(Player->GetAimTargetDistanceCm(), 25 * 91.44, .1));
                Player->AimToward(FVector2D(Tee) + FVector2D(3000, 100));
                Test.TestTrue(TEXT("A map aim leaves the pin in place"), FMath::IsNearlyEqual(FVector::Dist2D(Tee, Mode->GetCupLocation()), 25 * 91.44, .1));
                Test.TestTrue(TEXT("Hit a drill shot"), Ball->Launch(Chip));
                Player->ResetBallToTee();
                Test.TestTrue(TEXT("Manual reset leaves the shot in flight"), Ball->IsMoving());
                Test.TestFalse(TEXT("Pin locked in flight"), Drill->SetDistance(26));
                break;
            }
            case 2:
                if (Ball->IsMoving())
                    return false;
                Test.TestEqual(TEXT("First run keeps both finished shots"), Session->FinishedBallCount(), 2);
                Test.TestEqual(TEXT("The drill shot is scored"), CastChecked<UDistancePractice>(Session->Active())->GetSet().Shots, 1);
                Test.TestFalse(TEXT("The yardage locks once a set starts"), CastChecked<UDistancePractice>(Session->Active())->SetDistance(30));
                Session->ExitDrill();
                Test.TestTrue(TEXT("Exit restores the original pin"), OriginalPin.Equals(Mode->GetCupLocation(), .1));
                CheckSwitchGames(Test, *Session, *Mode, *Player, Tee);
                if (!Test.TestNotNull(TEXT("A second Distance Practice run"), Cast<UDistancePractice>(Session->Active())))
                    return true;
                CastChecked<UDistancePractice>(Session->Active())->SetDistance(40);
                if (GolfSettings::BypassStartupMenu())
                {
                    SavedPreferences = GolfSettings::Get();
                    auto Settings = SavedPreferences.GetValue();
                    Settings.AutoTee = 0;
                    Test.TestTrue(TEXT("Turn off automatic tee-up"), GolfSettings::Commit(Settings, World));
                }
                Mode->SetRecordShots(true);
                Test.TestTrue(TEXT("Hit a second-run shot"), Ball->Launch(Chip));
                break;
            default:
                if (Ball->IsMoving())
                    return false;
                if (SavedPreferences)
                {
                    if (!RestAt) RestAt = World->GetTimeSeconds();
                    if (World->GetTimeSeconds() < RestAt.GetValue() + AUOpenGolfGameMode::ResetDelayS + .5) return false;
                    Test.TestFalse(TEXT("Auto-tee off leaves the live ball downrange"), Mode->IsBallAtAddress());
                }
                Test.TestEqual(TEXT("Three finished balls"), Session->FinishedBallCount(), 3);
                Session->RestartDrill();
                Test.TestEqual(TEXT("Restart clears only the current run"), Session->FinishedBallCount(), 2);
                Test.TestEqual(TEXT("Restart keeps selected distance"), CastChecked<UDistancePractice>(Session->Active())->GetDistance(), 40);
                Test.TestEqual(TEXT("Restart starts a fresh set"), CastChecked<UDistancePractice>(Session->Active())->GetSet().Shots, 0);
                FOpenGolfPermissionTestAccess::Set(World);
                Player->HandleEscape();
                Player->InjectReplayShot();
                Test.TestFalse(TEXT("A monitor event during the drill dialog cannot launch the ball"), Ball->IsMoving());
                Player->CloseDrillDialog();
                Player->InjectReplayShot();
                Test.TestTrue(TEXT("The next monitor event starts the next shot"), Ball->IsMoving());
                Test.TestTrue(TEXT("The shot starts on the tee"), FVector::Dist2D(Ball->GetActorLocation(), Tee) < 1);
                {
                    const int32 InFlight = Ball->GetShotNumber();
                    Test.TestEqual(TEXT("A drill waits for the ball to stop"), Session->RejectReason(), FString(TEXT("Wait for the ball to stop")));
                    Player->InjectReplayShot();
                    Test.TestEqual(TEXT("A monitor event cannot interrupt a drill shot"), Ball->GetShotNumber(), InFlight);
                }
                Session->ExitDrill();
                Test.TestFalse(TEXT("Exit cancels the moving shot"), Ball->IsMoving());
                Test.TestEqual(TEXT("Interrupted shot adds no ball"), Session->FinishedBallCount(), 2);
                Mode->SetPlayMode(EOpenGolfPlayMode::Hole);
                Test.TestNull(TEXT("Course round has no Training session"), Mode->GetTrainingSession());
                Mode->SetPlayMode(EOpenGolfPlayMode::Range);
                Test.TestEqual(TEXT("A fresh Training session starts empty"), Mode->GetTrainingSession()->FinishedBallCount(), 0);
                return true;
            }
            ++Step;
            return false;
        }

    private:
        FAutomationTestBase& Test;
        int32 Step = 0;
        FVector OriginalPin;
        TOptional<double> RestAt;
        TOptional<GolfSettings::FPreferences> SavedPreferences;
    };

    /** Scores both games from rest reports placed around the pin, so no shot has to be tuned to stop inside 3 ft. */
    class FScoreTargetGames : public IAutomationLatentCommand
    {
    public:
        explicit FScoreTargetGames(FAutomationTestBase& InTest) : Test(InTest) {}
        bool Update() override
        {
            if (GetCurrentRunTime() > 60)
            {
                Test.AddError(FString::Printf(TEXT("Timed out at scoring step %d"), Step));
                return true;
            }
            UWorld* World = GEditor->PlayWorld;
            Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
            Player = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
            if (!Mode || !Player || !Mode->GetRange() || !Mode->GetTrainingSession() || !Mode->GetBall() || Mode->GetBall()->IsMoving() ||
                World->GetTimeSeconds() < WaitUntil)
                return false;
            Session = Mode->GetTrainingSession();
            Tee = Mode->GetRange()->GetTeeLocation();
            const bool bDone = RunStep(*World);
            ++Step;
            return bDone;
        }

    private:
        /** A ball reported at rest Offset cm past the pin, along the line from the tee. */
        void Rest(double OffsetCm)
        {
            if (!NextShot) NextShot = Mode->GetBall()->GetShotNumber() + 1;
            FOpenFairwayShotResult Result;
            Result.ShotNumber = NextShot++;
            Result.bAtRest = true;
            Result.TerminationReason = EOpenFairwayShotTermination::Rest;
            const FVector Pin = Mode->GetCupLocation();
            Session->Active()->OnShotAtRest(*Session, Result, Pin + (Pin - Tee).GetSafeNormal2D() * OffsetCm);
        }

        void WaitForDelay(const UWorld& World) { WaitUntil = World.GetTimeSeconds() + AUOpenGolfGameMode::AddressDelayS + .3; }

        bool RunStep(const UWorld& World)
        {
            switch (Step)
            {
            case 0: return ScorePractice(World);
            case 1: return FinishPractice();
            case 2: return ClimbLadder(World);
            case 3: return DropToFloor();
            case 4: return CompleteLadder(World);
            default: return FinishLadder();
            }
        }

        bool ScorePractice(const UWorld& World)
        {
            if (!Test.TestTrue(TEXT("Start a 3-try set"), Session->StartDrill(UDistancePractice::StaticClass(), 3)))
                return true;
            auto* Drill = CastChecked<UDistancePractice>(Session->Active());
            Test.TestTrue(TEXT("Pick 30 yd"), Drill->SetDistance(30));
            Session->Active()->OnShotAtRest(*Session, Mode->GetBall()->GetResult(), Mode->GetCupLocation());
            Test.TestEqual(TEXT("A ball from before the drill is not a try"), Drill->GetSet().Shots, 0);
            Rest(60);
            --NextShot;
            Rest(60);
            Test.TestEqual(TEXT("A repeated report scores once"), Drill->GetSet().Shots, 1);
            Rest(150);
            Rest(400);
            const auto& Set = Drill->GetSet();
            Test.TestTrue(TEXT("One inside 3 ft, two inside 6 ft, three shots"), Set.Shots == 3 && Set.Inside3 == 1 && Set.Inside6 == 2);
            Test.TestTrue(TEXT("Three of three completes the set"), Drill->IsComplete());
            Test.TestFalse(TEXT("The result is reported"), Drill->ResultText().IsEmpty());
            Test.TestFalse(TEXT("A finished set takes no shots"), Session->RejectReason().IsEmpty());
            FOpenGolfPermissionTestAccess::Set(Mode->GetWorld());
            Player->InjectReplayShot();
            Test.TestFalse(TEXT("A monitor event after the set does not launch"), Mode->GetBall()->IsMoving());
            Test.TestFalse(TEXT("The result waits for the player to see the last ball"), Player->IsDrillDialogOpen());
            WaitForDelay(World);
            return false;
        }

        bool FinishPractice()
        {
            Test.TestTrue(TEXT("The finished set opens the drill dialog"), Player->IsDrillDialogOpen());
            Player->CloseDrillDialog();
            Session->RestartDrill();
            auto* Drill = CastChecked<UDistancePractice>(Session->Active());
            Test.TestTrue(TEXT("Play Again starts a fresh set at the same yardage"), !Drill->IsComplete() && Drill->GetSet().Shots == 0 && Drill->GetDistance() == 30);
            Test.TestTrue(TEXT("Play Again keeps 3 tries"), Drill->GetTries() == 3);
            Session->ExitDrill();
            return false;
        }

        bool ClimbLadder(const UWorld& World)
        {
            if (!Test.TestTrue(TEXT("Start the ladder"), Session->StartDrill(UDistanceLadder::StaticClass(), 3)))
                return true;
            auto* Ladder = CastChecked<UDistanceLadder>(Session->Active());
            Test.TestEqual(TEXT("The ladder starts at 15 yd"), PinYards(*Mode, Tee), 15., .01);
            CheckTarget(Test, *Ladder, *Mode->GetRange(), Tee, Mode->GetCupLocation());
            Rest(80);
            Test.TestTrue(TEXT("Inside 3 ft moves on to 25 yd"), Ladder->TargetYards() == 25 && Ladder->GetPendingYards() == TOptional<int32>(25));
            Test.TestEqual(TEXT("The pin waits while the player sees the ball"), PinYards(*Mode, Tee), 15., .01);
            Session->BeforeShot();
            Test.TestEqual(TEXT("The next shot moves the pin at once"), PinYards(*Mode, Tee), 25., .01);
            Test.TestFalse(TEXT("Nothing left pending"), Ladder->GetPendingYards().IsSet());
            for (int32 Miss = 0; Miss < 3; ++Miss)
                Rest(300);
            Test.TestEqual(TEXT("Three misses drop back to 15 yd"), Ladder->TargetYards(), 15);
            WaitForDelay(World);
            return false;
        }

        bool DropToFloor()
        {
            auto* Ladder = CastChecked<UDistanceLadder>(Session->Active());
            Test.TestEqual(TEXT("The pin moves back after the delay"), PinYards(*Mode, Tee), 15., .01);
            for (int32 Miss = 0; Miss < 3; ++Miss)
                Rest(300);
            Test.TestTrue(TEXT("15 yd is the floor"), Ladder->TargetYards() == 15 && !Ladder->GetPendingYards().IsSet());
            Test.TestTrue(TEXT("The tries start again"), Ladder->GetState().TriesUsed == 0);
            return false;
        }

        bool CompleteLadder(const UWorld& World)
        {
            auto* Ladder = CastChecked<UDistanceLadder>(Session->Active());
            for (int32 Stage = 0; Stage < 14; ++Stage)
            {
                Rest(0);
                Session->BeforeShot();
            }
            Test.TestEqual(TEXT("The last target is 150 yd"), PinYards(*Mode, Tee), 150., .01);
            CheckTarget(Test, *Ladder, *Mode->GetRange(), Tee, Mode->GetCupLocation());
            Rest(-85);
            Test.TestTrue(TEXT("Inside 3 ft at 150 yd completes the ladder"), Ladder->IsComplete());
            Test.TestEqual(TEXT("Every shot counted"), Ladder->GetState().TotalShots, 22);
            Test.TestEqual(TEXT("Result"), Ladder->ResultText().ToString(), FString(TEXT("Cleared 150 yd in 22 shots")));
            Test.TestFalse(TEXT("A finished ladder takes no shots"), Session->RejectReason().IsEmpty());
            WaitForDelay(World);
            return false;
        }

        bool FinishLadder()
        {
            Test.TestTrue(TEXT("The finished ladder opens the drill dialog"), Player->IsDrillDialogOpen());
            Player->CloseDrillDialog();
            Session->RestartDrill();
            auto* Ladder = CastChecked<UDistanceLadder>(Session->Active());
            Test.TestTrue(TEXT("Play Again starts at 15 yd"), !Ladder->IsComplete() && Ladder->GetState().TotalShots == 0);
            Test.TestEqual(TEXT("The pin returns to 15 yd"), PinYards(*Mode, Tee), 15., .01);
            Session->ExitDrill();
            Test.TestTrue(TEXT("Exit restores the range's pin"), Mode->GetCupLocation().Equals(Mode->GetRange()->GetOriginalCup().Position, .1));
            return true;
        }

        FAutomationTestBase& Test;
        int32 Step = 0;
        int32 NextShot = 0;
        double WaitUntil = 0;
        FVector Tee;
        AUOpenGolfGameMode* Mode = nullptr;
        AUOpenGolfPlayerController* Player = nullptr;
        ATrainingSession* Session = nullptr;
    };
} // namespace TrainingGamesTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingGamesTest, "UOpenGolf.Range.TrainingGames", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTrainingGamesTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
        return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<TrainingGamesTestDetail::FPlayDistancePractice>(*this));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<TrainingGamesTestDetail::FScoreTargetGames>(*this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
