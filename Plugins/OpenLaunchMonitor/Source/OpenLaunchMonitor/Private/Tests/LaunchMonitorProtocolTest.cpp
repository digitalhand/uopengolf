// SPDX-License-Identifier: MIT
// Expected values mirror open-lm-connector's xUnit tests (MIT) at 92eb021, including
// frames that test suite records as Square Home captures.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Square/SquareProtocol.h"

namespace
{
    TArray<uint8> Hex(const TCHAR* Text)
    {
        TArray<uint8> Bytes;
        verify(SquareProtocol::FromHex(Text, Bytes));
        return Bytes;
    }

    TArray<uint8> ShotFrame(uint8 Kind, int16 Speed, int16 Vertical, int16 Horizontal, int16 Total, int16 Axis, int16 Back, int16 Side)
    {
        TArray<uint8> Frame = {0x11, 0x02, Kind};
        for (const int16 Value : {Speed, Vertical, Horizontal, Total, Axis, Back, Side})
        {
            Frame.Add(static_cast<uint8>(Value & 0xFF));
            Frame.Add(static_cast<uint8>((static_cast<uint16>(Value) >> 8) & 0xFF));
        }
        return Frame;
    }

    constexpr float Tolerance = 0.001f;
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaunchMonitorProtocolTest, "UOpenGolf.LaunchMonitor.Protocol",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLaunchMonitorProtocolTest::RunTest(const FString& Parameters)
{
    {
        // Square Home capture: 25.70 m/s full swing.
        FSquareShotMetrics Shot;
        TestTrue(TEXT("Captured shot parses"), SquareProtocol::TryParseShot(Hex(TEXT("1102370A0A9A09C8001A16E8FF7C15D0FF")), Shot));
        TestEqual(TEXT("Captured speed"), Shot.BallSpeedMps, 25.70f, Tolerance);
        TestEqual(TEXT("Captured VLA"), Shot.VerticalAngleDeg, 24.58f, Tolerance);
        TestEqual(TEXT("Captured HLA"), Shot.HorizontalAngleDeg, 2.00f, Tolerance);
        TestEqual(TEXT("Captured total spin"), Shot.TotalSpinRpm, 5658);
        TestEqual(TEXT("Captured spin axis"), Shot.SpinAxisDeg, 0.24f, Tolerance);
        TestEqual(TEXT("Captured back spin"), Shot.BackSpinRpm, 5500);
        TestEqual(TEXT("Captured side spin"), Shot.SideSpinRpm, 48);
        TestTrue(TEXT("Captured full swing"), Shot.ShotType == ESquareShotType::Full);
    }
    {
        // Synthesized replay of the reference wood shot.
        FSquareShotMetrics Shot;
        TestTrue(TEXT("Wood shot parses"), SquareProtocol::TryParseShot(Hex(TEXT("110237FD13B702C1FF8D078E008C073000")), Shot));
        TestEqual(TEXT("Wood speed"), Shot.BallSpeedMps, 51.17f, Tolerance);
        TestEqual(TEXT("Wood VLA"), Shot.VerticalAngleDeg, 6.95f, Tolerance);
        TestEqual(TEXT("Wood HLA"), Shot.HorizontalAngleDeg, -0.63f, Tolerance);
        TestEqual(TEXT("Wood total spin"), Shot.TotalSpinRpm, 1933);
        TestEqual(TEXT("Wood spin axis"), Shot.SpinAxisDeg, -1.42f, Tolerance);
        TestEqual(TEXT("Wood back spin"), Shot.BackSpinRpm, 1932);
        TestEqual(TEXT("Wood side spin"), Shot.SideSpinRpm, -48);
    }
    {
        // Device spin signs are positive-left; HLA is already positive-right.
        FSquareShotMetrics Shot;
        TestTrue(TEXT("Signed shot parses"), SquareProtocol::TryParseShot(ShotFrame(0x37, 5000, 1500, 200, 3000, -500, 2900, -261), Shot));
        TestEqual(TEXT("Signed axis"), Shot.SpinAxisDeg, 5.0f, Tolerance);
        TestEqual(TEXT("Signed side spin"), Shot.SideSpinRpm, 261);
        TestEqual(TEXT("Positive HLA is preserved"), Shot.HorizontalAngleDeg, 2.0f, Tolerance);
        TestTrue(TEXT("Negative HLA parses"), SquareProtocol::TryParseShot(ShotFrame(0x37, 5000, 1500, -97, 3000, -500, 2900, -261), Shot));
        TestEqual(TEXT("Negative HLA is preserved"), Shot.HorizontalAngleDeg, -0.97f, Tolerance);
        TestTrue(TEXT("Measured back spin"), Shot.BackSpinSource == ESquareValueSource::Packet);
    }
    {
        // Missing spin components are derived from total spin and axis.
        FSquareShotMetrics Shot;
        const int16 Missing = static_cast<int16>(0x8000);
        TestTrue(TEXT("Derived shot parses"), SquareProtocol::TryParseShot(ShotFrame(0x37, 5000, 1500, 200, 3000, -500, Missing, Missing), Shot));
        TestEqual(TEXT("Derived back spin"), Shot.BackSpinRpm, 2989);
        TestEqual(TEXT("Derived side spin"), Shot.SideSpinRpm, 261);
        TestTrue(TEXT("Back spin marked derived"), Shot.BackSpinSource == ESquareValueSource::Derived);
        TestTrue(TEXT("Side spin marked derived"), Shot.SideSpinSource == ESquareValueSource::Derived);

        TestTrue(TEXT("Missing VLA still parses"), SquareProtocol::TryParseShot(ShotFrame(0x37, 5000, Missing, 200, 3000, -500, 2900, -261), Shot));
        TestTrue(TEXT("Missing VLA marked missing"), Shot.VerticalAngleSource == ESquareValueSource::Missing);
        TestEqual(TEXT("Missing VLA is not a measurement"), Shot.VerticalAngleDeg, 0.0f, Tolerance);
    }
    {
        // Square Home putt capture: launched slightly downward.
        FSquareShotMetrics Shot;
        TestTrue(TEXT("Putt parses"), SquareProtocol::TryParseShot(Hex(TEXT("1102138101E6FF91010000000000000000")), Shot));
        TestTrue(TEXT("Putt type"), Shot.ShotType == ESquareShotType::Putt);
        TestEqual(TEXT("Putt speed"), Shot.BallSpeedMps, 3.85f, Tolerance);
        TestEqual(TEXT("Putt VLA"), Shot.VerticalAngleDeg, -0.26f, Tolerance);
        TestEqual(TEXT("Putt HLA"), Shot.HorizontalAngleDeg, 4.01f, Tolerance);
        TestEqual(TEXT("Putt spin"), Shot.TotalSpinRpm, 0);
    }
    {
        FSquareShotMetrics Shot;
        TestFalse(TEXT("Unreadable ball speed is rejected"), SquareProtocol::TryParseShot(Hex(TEXT("11023700809A09C8001A16E8FF7C15D0FF")), Shot));
        TestFalse(TEXT("Excessive spin is rejected"), SquareProtocol::TryParseShot(ShotFrame(0x37, 5000, 1500, 0, 31000, 0, 0, 0), Shot));
        TestFalse(TEXT("Short frame is rejected"), SquareProtocol::TryParseShot(Hex(TEXT("1102370A0A")), Shot));
        TArray<uint8> Zeroes;
        Zeroes.SetNumZeroed(17);
        TestFalse(TEXT("Foreign frame is rejected"), SquareProtocol::TryParseShot(Zeroes, Shot));
    }
    {
        FSquareSensorData Sensor;
        TArray<uint8> Frame = {0x11, 0x01, 0x00, 0x01, 0x01, 10, 0, 0, 0, 0xEC, 0xFF, 0xFF, 0xFF, 30, 0, 0, 0};
        TestTrue(TEXT("Sensor parses"), SquareProtocol::TryParseSensor(Frame, Sensor));
        TestTrue(TEXT("Sensor ready"), Sensor.bBallReady);
        TestTrue(TEXT("Sensor detected"), Sensor.bBallDetected);
        TestEqual(TEXT("Sensor X"), Sensor.PositionX, 10);
        TestEqual(TEXT("Sensor Y"), Sensor.PositionY, -20);
        TestEqual(TEXT("Sensor Z"), Sensor.PositionZ, 30);
        Frame[3] = 0x03;
        SquareProtocol::TryParseSensor(Frame, Sensor);
        TestFalse(TEXT("Sensor not ready"), Sensor.bBallReady);
    }
    {
        // Square Home capture of a tracked club: angles only.
        FSquareClubMetrics Club;
        TestTrue(TEXT("Tracked club parses"), SquareProtocol::TryParseClub(Hex(TEXT("11070FC0FB1A00A1FEFB0C")), Club));
        TestEqual(TEXT("Club path"), Club.PathDeg.Get(0.0f), -10.88f, Tolerance);
        TestEqual(TEXT("Club face"), Club.FaceDeg.Get(0.0f), 0.26f, Tolerance);
        TestEqual(TEXT("Club attack"), Club.AttackDeg.Get(0.0f), -3.51f, Tolerance);
        TestEqual(TEXT("Club loft"), Club.DynamicLoftDeg.Get(0.0f), 33.23f, Tolerance);
        TestFalse(TEXT("Home sends no impact"), Club.ImpactHorizontalMm.IsSet() || Club.ImpactVerticalMm.IsSet());

        TestTrue(TEXT("Untracked Home club parses"), SquareProtocol::TryParseClub(Hex(TEXT("110700FFFFFFFFFFFFFFFF")), Club));
        TestFalse(TEXT("Untracked club has no reading"), Club.HasReading());
        TestTrue(TEXT("Bare club frame parses"), SquareProtocol::TryParseClub(Hex(TEXT("110700")), Club));
        TestFalse(TEXT("Bare club frame has no reading"), Club.HasReading());
        TestFalse(TEXT("Short club frame is rejected"), SquareProtocol::TryParseClub(Hex(TEXT("1107")), Club));

        // Mask bit 1 cleared: face ignored even though its bytes hold a value.
        TestTrue(TEXT("Masked club parses"), SquareProtocol::TryParseClub(Hex(TEXT("11070DA000C4FFCAFECA08")), Club));
        TestFalse(TEXT("Unflagged face is missing"), Club.FaceDeg.IsSet());
        TestEqual(TEXT("Flagged path kept"), Club.PathDeg.Get(0.0f), 1.6f, Tolerance);

        // Impact must be flagged and on the face.
        TestTrue(TEXT("Impact club parses"), SquareProtocol::TryParseClub(Hex(TEXT("11073F0000000000000000550366FE")), Club));
        TestEqual(TEXT("Impact horizontal"), Club.ImpactHorizontalMm.Get(0.0f), 8.53f, Tolerance);
        TestEqual(TEXT("Impact vertical"), Club.ImpactVerticalMm.Get(0.0f), -4.10f, Tolerance);
        TestTrue(TEXT("Off-face impact parses"), SquareProtocol::TryParseClub(Hex(TEXT("11073F00000000000000009C180000")), Club));
        TestFalse(TEXT("Off-face impact dropped"), Club.ImpactHorizontalMm.IsSet());
    }
    {
        TestEqual(TEXT("Heartbeat"), SquareProtocol::ToHex(SquareProtocol::Heartbeat(0x2A)), FString(TEXT("11832A0000000000")));
        TestEqual(TEXT("Detect ball"), SquareProtocol::ToHex(SquareProtocol::DetectBall(0x05, 1, 1)), FString(TEXT("118105011100000000")));
        TestEqual(TEXT("Club"), SquareProtocol::ToHex(SquareProtocol::Club(0x07, TEXT("0204"), 0)), FString(TEXT("118207020400000000")));
        TestEqual(TEXT("Left-handed wedge"), SquareProtocol::ToHex(SquareProtocol::Club(0xFF, TEXT("0a06"), 1)), FString(TEXT("1182FF0A0601000000")));
        TestEqual(TEXT("Lob wedge (experimental code)"), SquareProtocol::ToHex(SquareProtocol::Club(0x07, TEXT("0d06"), 0)), FString(TEXT("1182070D0600000000")));
        TestEqual(TEXT("Request club"), SquareProtocol::ToHex(SquareProtocol::RequestClubMetrics(0x2A)), FString(TEXT("11872A000000000000")));
    }
    {
        const FString Json = TEXT("{\"lm\":\"1.4.2\",\"ble\":\"2.0\"}");
        const FTCHARToUTF8 Utf8(*Json);
        TestEqual(TEXT("JSON firmware"), SquareProtocol::ParseFirmware(TConstArrayView<uint8>(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length())),
            FString(TEXT("1.4.2")));
        const uint8 Plain[] = {'v', '3', '.', '1'};
        TestEqual(TEXT("Plain firmware"), SquareProtocol::ParseFirmware(Plain), FString(TEXT("v3.1")));
    }
    return true;
}

#endif
