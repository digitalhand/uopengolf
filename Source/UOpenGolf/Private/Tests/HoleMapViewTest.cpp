// SPDX-License-Identifier: MIT
// The minimap's view framing and label text: pure functions, no world.
#if WITH_DEV_AUTOMATION_TESTS
#include "HoleMap/HoleMapLabels.h"
#include "HoleMapFrame.h"
#include "Misc/AutomationTest.h"
#include "UI/Hud/HoleMapOverlay.h"

namespace HoleMapViewTestDetail
{
    constexpr double Tolerance = 0.01;

    /** A 400 yd hole along +X, framed as the range frames it. */
    FHoleMapFrame CaptureFrame()
    {
        return FHoleMapFrame::FromHole(FVector(0, 0, 100), FVector(36576, 0, 300), 3200, 18000, 0);
    }

    bool Inside(const FHoleMapFrame& Frame, FVector2D World)
    {
        const FVector2D UV = Frame.ToUV(World);
        return UV.X >= -1e-6 && UV.X <= 1 + 1e-6 && UV.Y >= -1e-6 && UV.Y <= 1 + 1e-6;
    }

} // namespace HoleMapViewTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoleMapViewFitTest, "UOpenGolf.Minimap.ViewFit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHoleMapViewFitTest::RunTest(const FString&)
{
    using namespace HoleMapViewTestDetail;
    const FHoleMapFrame Capture = CaptureFrame();
    const FVector2D Ball(25000, 1500), Cup(36576, 0);
    const FVector2D Points[] = {Ball, Cup};
    const FHoleMapFrame View = FHoleMapFrame::Fit(Capture, Points, 2500, 14000);

    TestTrue(TEXT("The view keeps the capture's up"), View.Up.Equals(Capture.Up, 1e-9));
    TestTrue(TEXT("The view is smaller than the capture"), View.Length < Capture.Length);
    TestTrue(TEXT("The view holds the minimum length"), View.Length >= 14000 - Tolerance);
    for (const FVector2D& Point : Points)
    {
        TestTrue(TEXT("The view shows the ball and the cup"), Inside(View, Point));
        // The margin keeps both clear of the edges.
        TestTrue(TEXT("The margin stays around each point"), Inside(View, Point + Capture.Up * 2400) && Inside(View, Point - Capture.Up * 2400));
    }
    TestTrue(TEXT("The view lies inside the capture"), Inside(Capture, View.ToWorld({0, 0})) && Inside(Capture, View.ToWorld({1, 1})));

    const FVector2D Tee(0, 0);
    const FVector2D WholeHole[] = {Tee, Cup};
    const FHoleMapFrame Whole = FHoleMapFrame::Fit(Capture, WholeHole, 2500, 14000);
    TestTrue(TEXT("From the tee the view shows the whole hole"), Inside(Whole, Tee) && Inside(Whole, Cup));
    TestTrue(TEXT("The view never grows past the capture"), Whole.Length <= Capture.Length + Tolerance);

    const FVector2D FarSide[] = {FVector2D(36000, 0), Cup};
    const FHoleMapFrame Near = FHoleMapFrame::Fit(Capture, FarSide, 2500, 14000);
    TestEqual(TEXT("Close to the cup the view zooms to its minimum"), Near.Length, 14000.0, Tolerance);
    TestTrue(TEXT("A view at the capture's end is moved back inside it"), Inside(Capture, Near.ToWorld({0, 0})) && Inside(Capture, Near.ToWorld({1, 1})));

    const FHoleMapFrame Empty = FHoleMapFrame::Fit(Capture, TArrayView<const FVector2D>(), 2500, 14000);
    TestTrue(TEXT("No points shows the whole capture"), FVector2D(Empty.Center).Equals(FVector2D(Capture.Center), Tolerance)
        && FMath::IsNearlyEqual(Empty.Length, Capture.Length, Tolerance));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoleMapViewRegionTest, "UOpenGolf.Minimap.ViewRegion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHoleMapViewRegionTest::RunTest(const FString&)
{
    using namespace HoleMapViewTestDetail;
    const FHoleMapFrame Capture = CaptureFrame();
    const FBox2D Whole = Capture.UVRegionWithin(Capture);
    TestTrue(TEXT("The capture shows all of its own picture"), Whole.Min.Equals(FVector2D(0, 0), 1e-9) && Whole.Max.Equals(FVector2D(1, 1), 1e-9));

    const FVector2D Points[] = {FVector2D(20000, -3000), FVector2D(36576, 0)};
    const FHoleMapFrame View = FHoleMapFrame::Fit(Capture, Points, 2500, 14000);
    const FBox2D Region = View.UVRegionWithin(Capture);
    // Any point of the view is drawn from the matching point of the capture's picture.
    for (const FVector2D ViewUV : {FVector2D(0, 0), FVector2D(1, 1), FVector2D(.25, .8), FVector2D(.9, .1)})
    {
        const FVector2D Expected = Region.Min + (Region.Max - Region.Min) * ViewUV;
        TestTrue(TEXT("The region maps view UVs onto the capture's picture"), Capture.ToUV(View.ToWorld(ViewUV)).Equals(Expected, 1e-6));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoleMapViewLerpTest, "UOpenGolf.Minimap.ViewLerp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHoleMapViewLerpTest::RunTest(const FString&)
{
    using namespace HoleMapViewTestDetail;
    const FHoleMapFrame Capture = CaptureFrame();
    const FVector2D Points[] = {FVector2D(30000, 0), FVector2D(36576, 0)};
    const FHoleMapFrame View = FHoleMapFrame::Fit(Capture, Points, 2500, 14000);
    const FHoleMapFrame Start = FHoleMapFrame::Lerp(Capture, View, 0);
    const FHoleMapFrame End = FHoleMapFrame::Lerp(Capture, View, 1);
    const FHoleMapFrame Half = FHoleMapFrame::Lerp(Capture, View, .5);
    TestTrue(TEXT("Alpha 0 is the first frame"), Start.Center.Equals(Capture.Center, Tolerance) && FMath::IsNearlyEqual(Start.Length, Capture.Length, Tolerance));
    TestTrue(TEXT("Alpha 1 is the second frame"), End.Center.Equals(View.Center, Tolerance) && FMath::IsNearlyEqual(End.Length, View.Length, Tolerance));
    TestEqual(TEXT("Halfway is between the lengths"), Half.Length, (Capture.Length + View.Length) / 2, Tolerance);
    TestTrue(TEXT("Frames share their up"), Capture.HasSameAxis(View) && !Capture.HasSameAxis(FHoleMapFrame::FromHole(FVector::ZeroVector, FVector(0, 100, 0), 0, 0, 0)));

    // Equal within a centimetre, so a recapture of the same hole is not a new frame.
    FHoleMapFrame Nudged = Capture;
    Nudged.Center.X += 0.5;
    Nudged.Length += 0.5;
    TestTrue(TEXT("Frames within a centimetre are equal"), Capture.Equals(Nudged));
    FHoleMapFrame Moved = Capture;
    Moved.Center.Y += 5;
    TestFalse(TEXT("A frame moved further is not equal"), Capture.Equals(Moved));
    FHoleMapFrame Grown = Capture;
    Grown.Length += 5;
    TestFalse(TEXT("A longer frame is not equal"), Capture.Equals(Grown));
    FHoleMapFrame Turned = Capture;
    Turned.Up = FVector2D(0, 1);
    TestFalse(TEXT("A frame facing another way is not equal"), Capture.Equals(Turned));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoleMapLabelsTest, "UOpenGolf.Minimap.Labels",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHoleMapLabelsTest::RunTest(const FString&)
{
    // Feet and inches, as golfers read a slope; whole feet drop the inches.
    TestEqual(TEXT("Uphill in feet and inches"), HoleMapLabels::HeightChange(1.55, false), FString(TEXT("▲ 5' 1\"")));
    TestEqual(TEXT("Whole feet"), HoleMapLabels::HeightChange(1.8288, false), FString(TEXT("▲ 6'")));
    TestEqual(TEXT("Downhill in feet"), HoleMapLabels::HeightChange(-0.62, false), FString(TEXT("▼ 2'")));
    TestEqual(TEXT("Under a foot in inches"), HoleMapLabels::HeightChange(0.12, false), FString(TEXT("▲ 5\"")));
    TestEqual(TEXT("The savanna pin, 8 cm up"), HoleMapLabels::HeightChange(0.082, false), FString(TEXT("▲ 3\"")));
    TestEqual(TEXT("Under half an inch reads level"), HoleMapLabels::HeightChange(0.01, false), FString());
    TestEqual(TEXT("Metres keep a decimal"), HoleMapLabels::HeightChange(1.55, true), FString(TEXT("▲ 1.6 m")));
    TestEqual(TEXT("Under 0.1 m reads level"), HoleMapLabels::HeightChange(-0.04, true), FString());

    // The footer always says something: an arrow and an amount, or Level.
    TestEqual(TEXT("The footer shows a rise"), HoleMapLabels::ElevationText(0.082, false), FString(TEXT("▲ 3\"")));
    TestEqual(TEXT("The footer reads level"), HoleMapLabels::ElevationText(0.01, false), FString(TEXT("Level")));

    TestEqual(TEXT("Sand is named as a golfer says it"), FString(HoleMapLabels::SurfaceName(EOpenFairwaySurface::Sand)), FString(TEXT("Bunker")));
    TestEqual(TEXT("Soft fairway is named"), FString(HoleMapLabels::SurfaceName(EOpenFairwaySurface::FairwaySoft)), FString(TEXT("Soft fairway")));
    TestEqual(TEXT("Green is named"), FString(HoleMapLabels::SurfaceName(EOpenFairwaySurface::Green)), FString(TEXT("Green")));

    // The header names the lie only, never the ball's motion: holed, on the tee box, or the surface it is on.
    TestEqual(TEXT("A holed ball is holed"), FString(HoleMapLabels::LieText(true, false, EOpenFairwaySurface::Green)), FString(TEXT("Holed")));
    TestEqual(TEXT("A ball on the tee box is on the tee"), FString(HoleMapLabels::LieText(false, true, EOpenFairwaySurface::Fairway)), FString(TEXT("Tee")));
    for (const EOpenFairwaySurface Surface : {EOpenFairwaySurface::Fairway, EOpenFairwaySurface::Rough, EOpenFairwaySurface::Sand,
             EOpenFairwaySurface::Green, EOpenFairwaySurface::Fringe})
        TestEqual(TEXT("A ball off the tee names its surface"), FString(HoleMapLabels::LieText(false, false, Surface)),
            FString(HoleMapLabels::SurfaceName(Surface)));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoleMapChipPlacementTest, "UOpenGolf.Minimap.ChipPlacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHoleMapChipPlacementTest::RunTest(const FString&)
{
    const FVector2f Map(187.5f, 300.0f), Chip(70, 18);
    const FVector2f Right = HoleMapOverlay::PlaceChip({50, 100}, 10, Chip, Map);
    TestTrue(TEXT("A chip sits right of its mark, centred on it"), Right.Equals({60, 91}, 1e-4f));
    const FVector2f Left = HoleMapOverlay::PlaceChip({170, 100}, 10, Chip, Map);
    TestTrue(TEXT("Near the right edge it moves to the left"), Left.Equals({90, 91}, 1e-4f));
    const FVector2f Top = HoleMapOverlay::PlaceChip({50, 3}, 10, Chip, Map);
    TestTrue(TEXT("It stays inside the map"), Top.Y >= 0 && Top.Y + Chip.Y <= Map.Y && Top.X >= 0 && Top.X + Chip.X <= Map.X);
    return true;
}

#endif
