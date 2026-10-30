// SPDX-License-Identifier: MIT
// The saved hole's contoured green: gentle grades, ball contact that follows the contour, a ball that rests on it and a
// putt that breaks downhill. Closes Phase 3's "ball-actor contact on contoured terrain".
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenGolfTerrain.h"
#include "UOpenGolfRange.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGreenContourTest, "UOpenGolf.Range.GreenContour",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace GreenContourTestDetail
{
    /** Grades as the hole layout limits them (Plugins/OpenGolfSavanna/Scripts/savanna_hole.py). */
    constexpr double MaxGreenGrade = 0.025;
    constexpr double MaxPinGrade = 0.01;
    constexpr double SampleSpacingM = 1.0;
    constexpr float FrameSeconds = 1.0f / 60.0f;

    double Grade(const FVector& Normal)
    {
        return FVector2D(Normal.X, Normal.Y).Size() / FMath::Max(Normal.Z, 1e-6);
    }

    /** Runs the ball's own tick until it rests (or 30 s pass); returns false if it never stopped. */
    bool RunToRest(AOpenFairwayBall& Ball)
    {
        for (int32 Frame = 0; Frame < 30 * 60 && Ball.IsMoving(); ++Frame)
            Ball.Tick(FrameSeconds);
        return !Ball.IsMoving();
    }
} // namespace GreenContourTestDetail

bool FGreenContourTest::RunTest(const FString& Parameters)
{
    using namespace GreenContourTestDetail;
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
        return false;
    UWorld* World = GEditor->GetEditorWorldContext().World();
    AUOpenGolfRange* Range = nullptr;
    for (TActorIterator<AUOpenGolfRange> It(World); It; ++It)
        Range = *It;
    if (!TestNotNull(TEXT("Saved range"), Range) || !TestNotNull(TEXT("Range terrain"), Range->Terrain.Get()) ||
        !TestNotNull(TEXT("Terrain definition"), Range->Terrain->Definition.Get()))
        return false;
    const AOpenGolfTerrainArea& Area = *Range->Terrain;
    const UOpenGolfTerrainDefinition& Definition = *Area.Definition;
    const FOpenGolfTerrainRegion* Green = Definition.Regions.FindByPredicate([](const FOpenGolfTerrainRegion& Region)
        { return Region.Surface && Region.Surface->Surface == EOpenFairwaySurface::Green; });
    if (!TestNotNull(TEXT("The hole has a green"), Green))
        return false;
    const FVector2D Pin(AUOpenGolfRange::HoleDistanceM, 0.0);
    TestTrue(TEXT("The pin is on the green"), UOpenGolfTerrainDefinition::Contains(Green->OutlineM, Pin));

    // Grades over the green, from the same height function the ground mesh was built from.
    FBox2D Bounds(Green->OutlineM);
    double Steepest = 0.0;
    FVector2D SteepestAt = Pin;
    int32 Contacts = 0;
    double WorstContactDeg = 0.0;
    for (double X = Bounds.Min.X; X <= Bounds.Max.X; X += SampleSpacingM)
        for (double Y = Bounds.Min.Y; Y <= Bounds.Max.Y; Y += SampleSpacingM)
        {
            const FVector2D Point(X, Y);
            if (!UOpenGolfTerrainDefinition::Contains(Green->OutlineM, Point))
                continue;
            const FVector Normal = Definition.NormalAt(Point);
            if (Grade(Normal) > Steepest)
            {
                Steepest = Grade(Normal);
                SteepestAt = Point;
            }
            // The ball's contact normal comes from the built mesh; it must follow the contour.
            FVector Ground, ContactNormal;
            if (Area.GroundAt(Area.GetActorTransform().TransformPosition(FVector(X * 100, Y * 100, 0)), Ground, ContactNormal))
            {
                ++Contacts;
                WorstContactDeg = FMath::Max(WorstContactDeg, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(ContactNormal | Normal, -1.0, 1.0))));
            }
        }
    AddInfo(FString::Printf(TEXT("Steepest green grade %.2f%% at %s; contact normals within %.2f deg over %d samples"), Steepest * 100, *SteepestAt.ToString(),
        WorstContactDeg, Contacts));
    TestTrue(TEXT("The green is contoured"), Steepest > 0.005);
    TestTrue(TEXT("The green stays gentle"), Steepest <= MaxGreenGrade + 1e-4);
    TestTrue(TEXT("The pin sits on near-flat ground"), Grade(Definition.NormalAt(Pin)) <= MaxPinGrade);
    TestTrue(TEXT("Every green sample has ground"), Contacts > 100);
    TestTrue(TEXT("Contact normals follow the contour"), WorstContactDeg < 0.6);

    auto* Ball = World->SpawnActor<AOpenFairwayBall>();
    if (!TestNotNull(TEXT("Ball"), Ball))
        return false;
    const FVector Steep = Area.GetActorTransform().TransformPosition(FVector(SteepestAt.X * 100, SteepestAt.Y * 100, 0));
    // A ball set down on the steepest part of the green stays there: the green holds slopes up to about 7 degrees.
    Ball->PlaceAt(Steep + FVector(0, 0, 10));
    const FVector Placed = Ball->GetActorLocation();
    for (int32 Frame = 0; Frame < 120; ++Frame)
        Ball->Tick(FrameSeconds);
    TestFalse(TEXT("A ball placed on the contour stays at rest"), Ball->IsMoving());
    TestTrue(TEXT("It does not creep"), Ball->GetActorLocation().Equals(Placed, 0.1));

    // A putt across the slope breaks downhill.
    const FVector Normal = Definition.NormalAt(SteepestAt);
    const FVector2D Downhill = FVector2D(Normal.X, Normal.Y).GetSafeNormal();
    const FVector2D Across(-Downhill.Y, Downhill.X);
    Ball->AimYawDeg = FMath::RadiansToDegrees(FMath::Atan2(Across.Y, Across.X));
    Ball->PlaceAt(Steep + FVector(0, 0, 10) - FVector(Across.X, Across.Y, 0) * 150);
    const FVector Start = Ball->GetActorLocation();
    FOpenFairwayLaunch Putt;
    Putt.BallSpeedMph = 4.0f;
    Putt.bHasTotalSpin = true;
    TestTrue(TEXT("The putt launches"), Ball->Launch(Putt));
    TestTrue(TEXT("The putt comes to rest"), RunToRest(*Ball));
    const FVector2D Travel(Ball->GetActorLocation() - Start);
    const double Break = Travel | Downhill;
    AddInfo(FString::Printf(TEXT("Putt across a %.2f%% slope: %.2f m along, %.1f cm of break"), Steepest * 100, (Travel | Across) / 100, Break));
    TestTrue(TEXT("The putt rolls along its line"), (Travel | Across) > 100.0);
    TestTrue(TEXT("The putt breaks downhill"), Break > 1.0);
    Ball->Destroy();
    return true;
}

#endif
