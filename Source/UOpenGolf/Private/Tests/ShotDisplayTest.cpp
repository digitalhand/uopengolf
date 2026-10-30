// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/ShotDisplay.h"

namespace
{
    FString ShotTileValue(const TArray<ShotDisplay::FTile>& Tiles, const TCHAR* Label)
    {
        const ShotDisplay::FTile* Tile = Tiles.FindByPredicate([Label](const ShotDisplay::FTile& Candidate)
            { return Candidate.Label == Label; });
        return Tile ? Tile->Value : FString(TEXT("<no tile>"));
    }

    TArray<FString> ShotTileLabels(const TArray<ShotDisplay::FTile>& Tiles)
    {
        TArray<FString> Labels;
        for (const ShotDisplay::FTile& Tile : Tiles)
        {
            Labels.Add(Tile.Label);
        }
        return Labels;
    }
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShotDisplayTest, "UOpenGolf.Range.ShotDisplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShotDisplayTest::RunTest(const FString& Parameters)
{
    using ESource = ELaunchMonitorValueSource;
    using ShotDisplay::ETileGroup;
    const FString Dash(TEXT("—"));

    // Version shown in the bottom-left corner comes from the project settings.
    TestEqual(TEXT("Version label"), ShotDisplay::VersionLabel(TEXT(" 0.0.1 ")), FString(TEXT("v0.0.1")));
    TestEqual(TEXT("No version, no label"), ShotDisplay::VersionLabel(FString()), FString());
    TestFalse(TEXT("Project version is set"), ShotDisplay::ProjectVersion().IsEmpty());

    // Layout: distances first, the raw launch-monitor values last in the requested order.
    const TArray<ShotDisplay::FTile> Empty = ShotDisplay::BuildTiles(nullptr, nullptr, nullptr);
    const TArray<FString> Labels = ShotTileLabels(Empty);
    if (!TestTrue(TEXT("Enough tiles"), Labels.Num() >= 12))
    {
        return false;
    }
    TestEqual(TEXT("Top row"), Labels[0] + TEXT(" | ") + Labels[1], FString(TEXT("Total (yd) | Carry (yd)")));
    TestEqual(TEXT("Second row"), Labels[2] + TEXT(" | ") + Labels[3], FString(TEXT("Roll (yd) | Apex (ft)")));
    const TArray<FString> LaunchOrder = {TEXT("Ball Speed (mph)"), TEXT("VLA"), TEXT("Total Spin (rpm)"), TEXT("HLA"),
        TEXT("Back Spin (rpm)"), TEXT("Side Spin (rpm)"), TEXT("Spin Axis")};
    const int32 FirstLaunch = Labels.Num() - LaunchOrder.Num();
    for (int32 Index = 0; Index < Empty.Num(); ++Index)
    {
        TestEqual(FString::Printf(TEXT("Launch tiles last (%s)"), *Labels[Index]), Empty[Index].Group == ETileGroup::Launch, Index >= FirstLaunch);
        if (Index >= FirstLaunch)
        {
            TestEqual(TEXT("Launch order"), Labels[Index], LaunchOrder[Index - FirstLaunch]);
        }
        TestEqual(FString::Printf(TEXT("No data is a dash (%s)"), *Labels[Index]), Empty[Index].Value, Dash);
    }

    FOpenFairwayShotResult Interrupted;
    Interrupted.bAtRest = true;
    Interrupted.TerminationReason = EOpenFairwayShotTermination::SafetyStop;
    Interrupted.CarryM = 900.0f;
    Interrupted.TotalM = 1000.0f;
    const auto InvalidTiles = ShotDisplay::BuildTiles(&Interrupted, nullptr, nullptr);
    TestEqual(TEXT("Interrupted total is not a completed distance"), ShotTileValue(InvalidTiles, TEXT("Total (yd)")), Dash);
    TestEqual(TEXT("Interrupted carry is not invented"), ShotTileValue(InvalidTiles, TEXT("Carry (yd)")), Dash);

    // In flight, with launch-monitor and partial club data.
    FOpenFairwayShotResult Flight;
    Flight.ShotNumber = 3;
    Flight.TotalM = 50.0f;
    Flight.LateralM = -3.0f;
    FLaunchMonitorShot Shot;
    Shot.ShotType = ELaunchMonitorShotType::Full;
    Shot.HorizontalLaunchSource = ESource::Measured;
    Shot.HorizontalLaunchDeg = -0.97f;
    Shot.SpinAxisSource = ESource::Derived;
    Shot.SpinAxisDeg = -1.42f;
    Shot.SideSpinSource = ESource::Measured;
    Shot.SideSpinRpm = -48;
    Shot.VerticalLaunchSource = ESource::Measured;
    Shot.VerticalLaunchDeg = -0.04f;
    FLaunchMonitorClubData Club;
    Club.bHasPath = true;
    Club.PathDeg = 2.04f;
    TArray<ShotDisplay::FTile> Tiles = ShotDisplay::BuildTiles(&Flight, &Shot, &Club);
    TestEqual(TEXT("Same labels with data"), ShotTileLabels(Tiles), Labels);
    TestEqual(TEXT("Live total"), ShotTileValue(Tiles, TEXT("Total (yd)")), FString(TEXT("54.7")));
    TestEqual(TEXT("Carry pending in flight"), ShotTileValue(Tiles, TEXT("Carry (yd)")), FString(TEXT("…")));
    TestEqual(TEXT("Roll pending in flight"), ShotTileValue(Tiles, TEXT("Roll (yd)")), FString(TEXT("…")));
    TestEqual(TEXT("Offline left"), ShotTileValue(Tiles, TEXT("Offline (yd)")), FString(TEXT("L3.3")));
    TestEqual(TEXT("Negative HLA is left"), ShotTileValue(Tiles, TEXT("HLA")), FString(TEXT("L1.0°")));
    TestEqual(TEXT("Derived marker retained"), ShotTileValue(Tiles, TEXT("Spin Axis")), FString(TEXT("L1.4°*")));
    TestEqual(TEXT("Side spin left"), ShotTileValue(Tiles, TEXT("Side Spin (rpm)")), FString(TEXT("L48")));
    TestEqual(TEXT("Rounded zero is not negative"), ShotTileValue(Tiles, TEXT("VLA")), FString(TEXT("0.0°")));
    TestEqual(TEXT("Missing launch value"), ShotTileValue(Tiles, TEXT("Ball Speed (mph)")), Dash);
    TestEqual(TEXT("Club path"), ShotTileValue(Tiles, TEXT("Club Path")), FString(TEXT("2.0°")));
    TestEqual(TEXT("Missing club value"), ShotTileValue(Tiles, TEXT("Face to Target")), Dash);
    for (const TCHAR* Removed : {TEXT("Lie"), TEXT("Response"), TEXT("Shot Type"), TEXT("Impact H (mm)"), TEXT("Impact V (mm)")})
    {
        TestFalse(FString::Printf(TEXT("No %s tile"), Removed), Labels.Contains(Removed));
    }
    TestEqual(TEXT("Heading in flight"), ShotDisplay::Heading(&Flight), FString(TEXT("Shot 3 · in flight")));
    TestEqual(TEXT("Heading before a shot"), ShotDisplay::Heading(nullptr), FString(TEXT("Waiting for a shot")));

    Shot.HorizontalLaunchDeg = 0.97f;
    TestEqual(TEXT("Positive HLA is right"), ShotTileValue(ShotDisplay::BuildTiles(&Flight, &Shot, &Club), TEXT("HLA")), FString(TEXT("R1.0°")));
    Shot.HorizontalLaunchDeg = -0.001f;
    TestEqual(TEXT("Rounded zero HLA has no side"), ShotTileValue(ShotDisplay::BuildTiles(&Flight, &Shot, &Club), TEXT("HLA")), FString(TEXT("0.0°")));

    // At rest after landing; without a landing, landing values stay dashes.
    FOpenFairwayShotResult Rest = Flight;
    Rest.bLanded = true;
    Rest.bAtRest = true;
    Rest.CarryM = 100.0f;
    Rest.RollM = 10.0f;
    Tiles = ShotDisplay::BuildTiles(&Rest, &Shot, &Club);
    TestEqual(TEXT("Carry at rest"), ShotTileValue(Tiles, TEXT("Carry (yd)")), FString(TEXT("109.4")));
    TestEqual(TEXT("Roll at rest"), ShotTileValue(Tiles, TEXT("Roll (yd)")), FString(TEXT("10.9")));
    TestEqual(TEXT("Heading at rest"), ShotDisplay::Heading(&Rest), FString(TEXT("Shot 3 · at rest")));
    Rest.bLanded = false;
    TestEqual(TEXT("No landing, no roll"), ShotTileValue(ShotDisplay::BuildTiles(&Rest, &Shot, &Club), TEXT("Roll (yd)")), Dash);

    using EState = ELaunchMonitorConnectionState;
    TestEqual(TEXT("Disconnected tip"), ShotDisplay::StatusTip(EState::Disconnected, TEXT(""), 0, TEXT("")), FString(TEXT("Connect [C]")));
    TestEqual(TEXT("Connected tip"), ShotDisplay::StatusTip(EState::Ready, TEXT(""), 0, TEXT("")),
        FString(TEXT("Disconnect [T]  ·  Record [F9]")));
    TestEqual(TEXT("Launch-monitor error persists while disconnected"),
        ShotDisplay::StatusTip(EState::Disconnected, TEXT("Scan timed out"), 0, TEXT("")), FString(TEXT("Scan timed out  ·  Connect [C]")));
    TestEqual(TEXT("Launch-monitor error shown alone while reconnecting"),
        ShotDisplay::StatusTip(EState::Scanning, TEXT("Link lost"), 0, TEXT("")), FString(TEXT("Link lost")));
    TestEqual(TEXT("Recording error outranks launch-monitor error"),
        ShotDisplay::StatusTip(EState::Disconnected, TEXT("Link lost"), 2, TEXT("Disk full")), FString(TEXT("Disk full - retrying")));

    return true;
}
#endif
