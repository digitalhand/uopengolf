// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Common/TcpSocketBuilder.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformProcess.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "OpenConnect/OpenConnectProtocol.h"
#include "OpenConnect/OpenConnectServer.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "UObject/Package.h"

namespace
{
    // An example Open Connect v1 shot.
    const TCHAR* SpecShot = TEXT(R"({"DeviceID":"Open Connect LM 1.1","Units":"Yards","ShotNumber":13,"APIversion":"1",
        "BallData":{"Speed":147.5,"SpinAxis":-13.2,"TotalSpin":3250.0,"BackSpin":2500.0,"SideSpin":-800.0,"HLA":2.3,"VLA":14.3,"CarryDistance":256.5},
        "ClubData":{"Speed":0.0,"AngleOfAttack":0.0,"FaceToTarget":0.0,"Lie":0.0,"Loft":0.0,"Path":0.0,"SpeedAtImpact":0.0,"VerticalFaceImpact":0.0,"HorizontalFaceImpact":0.0,"ClosureRate":0.0},
        "ShotDataOptions":{"ContainsBallData":true,"ContainsClubData":false,"LaunchMonitorIsReady":true,"LaunchMonitorBallDetected":true,"IsHeartBeat":false}})");

    TArray<uint8> Utf8(const FString& Text)
    {
        const FTCHARToUTF8 Converted(*Text);
        return TArray<uint8>(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    }

    TSharedPtr<FJsonObject> ParseObject(const FString& Json)
    {
        TSharedPtr<FJsonObject> Object;
        FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Json), Object);
        return Object;
    }

    bool Has(ELaunchMonitorValueSource Source, ELaunchMonitorValueSource Expected) { return Source == Expected; }

    /** A blocking TCP client standing in for a connector app. */
    struct FTestConnector
    {
        FSocket* Socket = nullptr;
        FOpenConnectFramer Replies;

        ~FTestConnector() { Close(); }

        bool Connect(int32 Port)
        {
            ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
            Socket = FTcpSocketBuilder(TEXT("OpenConnectTestConnector")).AsBlocking().Build();
            const TSharedRef<FInternetAddr> Address = Sockets->CreateInternetAddr();
            bool bValid = false;
            Address->SetIp(TEXT("127.0.0.1"), bValid);
            Address->SetPort(Port);
            return Socket && bValid && Socket->Connect(*Address);
        }

        bool Send(const FString& Text)
        {
            const TArray<uint8> Bytes = Utf8(Text);
            int32 Sent = 0;
            return Socket && Socket->Send(Bytes.GetData(), Bytes.Num(), Sent) && Sent == Bytes.Num();
        }

        /** Reads available bytes without blocking; returns the next complete reply, if any. */
        bool Poll(FString& OutReply)
        {
            if (Replies.Next(OutReply))
                return true;
            uint32 Pending = 0;
            while (Socket && Socket->HasPendingData(Pending) && Pending > 0)
            {
                uint8 Buffer[1024];
                int32 Read = 0;
                if (!Socket->Recv(Buffer, FMath::Min<int32>(sizeof(Buffer), Pending), Read) || Read <= 0)
                    break;
                Replies.Append(Buffer, Read);
            }
            return Replies.Next(OutReply);
        }

        /** Sends without waiting from now on; a flood test must keep pumping while the receiver pushes back. */
        void MakeNonBlocking()
        {
            if (Socket)
                Socket->SetNonBlocking(true);
        }

        void Close()
        {
            if (Socket)
            {
                Socket->Close();
                ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
                Socket = nullptr;
            }
        }
    };

    /** Ticks the core ticker (which drives the listener's events) until Done or about five seconds pass. */
    bool PumpUntil(TFunctionRef<bool()> Done)
    {
        for (int32 Step = 0; Step < 500; ++Step)
        {
            FTSTicker::GetCoreTicker().Tick(0.01f);
            if (Done())
                return true;
            FPlatformProcess::Sleep(0.01f);
        }
        return false;
    }
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenConnectProtocolTest, "UOpenGolf.LaunchMonitor.OpenConnect.Protocol",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenConnectProtocolTest::RunTest(const FString& Parameters)
{
    // Framing: TCP may split one message across reads and join several in one read.
    {
        FOpenConnectFramer Framer;
        FString Json;
        const TArray<uint8> Bytes = Utf8(TEXT("junk {\"a\":\"} {\\\"\",\"b\":{\"c\":1}}{\"d\":2}{\"e\""));
        Framer.Append(Bytes.GetData(), 20);
        TestFalse(TEXT("A partial object is not returned"), Framer.Next(Json));
        Framer.Append(Bytes.GetData() + 20, Bytes.Num() - 20);
        TestTrue(TEXT("First object completes"), Framer.Next(Json));
        TestEqual(TEXT("Junk dropped; braces and escaped quotes inside strings ignored"), Json, FString(TEXT("{\"a\":\"} {\\\"\",\"b\":{\"c\":1}}")));
        TestTrue(TEXT("Coalesced second object"), Framer.Next(Json));
        TestEqual(TEXT("Second object"), Json, FString(TEXT("{\"d\":2}")));
        TestFalse(TEXT("Trailing partial object waits"), Framer.Next(Json));
        TestTrue(TEXT("Partial object stays buffered"), Framer.BufferedBytes() > 0);

        FOpenConnectFramer Overflow;
        TArray<uint8> Open = Utf8(TEXT("{\"x\":\""));
        Open.AddZeroed(FOpenConnectFramer::MaxBufferBytes);
        TestFalse(TEXT("An object larger than the limit overflows"), Overflow.Append(Open.GetData(), Open.Num()));
        TestEqual(TEXT("Overflow clears the buffer"), Overflow.BufferedBytes(), 0);
    }

    // The spec's example shot: mph to m/s, every spin component measured, club data ignored when not flagged.
    {
        FOpenConnectMessage Message;
        FString Error;
        TestTrue(TEXT("Spec example decodes"), OpenConnectProtocol::Decode(SpecShot, Message, Error));
        TestEqual(TEXT("DeviceID"), Message.DeviceId, FString(TEXT("Open Connect LM 1.1")));
        TestEqual(TEXT("Device shot number"), Message.DeviceShotNumber, 13);
        TestTrue(TEXT("Ready flag"), Message.bReady.Get(false));
        if (TestTrue(TEXT("Shot present"), Message.Shot.IsSet()))
        {
            const FLaunchMonitorShot& Shot = Message.Shot.GetValue();
            TestEqual(TEXT("Speed mph round trip"), Shot.GetBallSpeedMph(), 147.5f, 0.001f);
            TestEqual(TEXT("Speed m/s"), Shot.BallSpeedMps, 65.9384f, 0.001f);
            TestEqual(TEXT("VLA"), Shot.VerticalLaunchDeg, 14.3f, 0.001f);
            TestEqual(TEXT("HLA keeps right positive"), Shot.HorizontalLaunchDeg, 2.3f, 0.001f);
            TestEqual(TEXT("Total spin"), Shot.TotalSpinRpm, 3250);
            TestEqual(TEXT("Spin axis keeps left negative"), Shot.SpinAxisDeg, -13.2f, 0.001f);
            TestEqual(TEXT("Back spin as sent"), Shot.BackSpinRpm, 2500);
            TestEqual(TEXT("Side spin as sent"), Shot.SideSpinRpm, -800);
            TestTrue(TEXT("All measured"), Has(Shot.BackSpinSource, ELaunchMonitorValueSource::Measured) &&
                Has(Shot.SideSpinSource, ELaunchMonitorValueSource::Measured) && Has(Shot.TotalSpinSource, ELaunchMonitorValueSource::Measured));
        }
        TestFalse(TEXT("ClubData ignored when ContainsClubData is false"), Message.Club.IsSet());
    }

    // Spin the connector leaves out is derived and marked so, never invented.
    {
        FOpenConnectMessage Message;
        FString Error;
        TestTrue(TEXT("Total and axis only"), OpenConnectProtocol::Decode(TEXT(R"({"BallData":{"Speed":100,"VLA":12,"TotalSpin":3000,"SpinAxis":10},"ShotDataOptions":{"ContainsBallData":true}})"), Message, Error));
        const FLaunchMonitorShot& Shot = Message.Shot.GetValue();
        TestEqual(TEXT("Derived back spin"), Shot.BackSpinRpm, FMath::RoundToInt32(3000 * FMath::Cos(FMath::DegreesToRadians(10.0f))));
        TestEqual(TEXT("Derived side spin positive right"), Shot.SideSpinRpm, FMath::RoundToInt32(3000 * FMath::Sin(FMath::DegreesToRadians(10.0f))));
        TestTrue(TEXT("Back and side marked derived"), Has(Shot.BackSpinSource, ELaunchMonitorValueSource::Derived) && Has(Shot.SideSpinSource, ELaunchMonitorValueSource::Derived));
        TestTrue(TEXT("Missing HLA stays missing"), Has(Shot.HorizontalLaunchSource, ELaunchMonitorValueSource::Missing));

        TestTrue(TEXT("Back and side only"), OpenConnectProtocol::Decode(TEXT(R"({"BallData":{"Speed":100,"VLA":12,"BackSpin":3000,"SideSpin":-500}})"), Message, Error));
        const FLaunchMonitorShot& Components = Message.Shot.GetValue();
        TestEqual(TEXT("Derived total"), Components.TotalSpinRpm, 3041);
        TestEqual(TEXT("Derived axis negative left"), Components.SpinAxisDeg, FMath::RadiansToDegrees(FMath::Atan2(-500.0f, 3000.0f)), 0.001f);
        TestTrue(TEXT("Total and axis marked derived"), Has(Components.TotalSpinSource, ELaunchMonitorValueSource::Derived) && Has(Components.SpinAxisSource, ELaunchMonitorValueSource::Derived));
    }

    // Club data when flagged: only the keys sent are present.
    {
        FOpenConnectMessage Message;
        FString Error;
        TestTrue(TEXT("Shot with club data"), OpenConnectProtocol::Decode(TEXT(R"({"BallData":{"Speed":90,"VLA":20},"ClubData":{"Path":-2.5,"Loft":24},"ShotDataOptions":{"ContainsBallData":true,"ContainsClubData":true}})"), Message, Error));
        if (TestTrue(TEXT("Club present"), Message.Club.IsSet()))
        {
            const FLaunchMonitorClubData& Club = Message.Club.GetValue();
            TestTrue(TEXT("Path"), Club.bHasPath && FMath::IsNearlyEqual(Club.PathDeg, -2.5f));
            TestTrue(TEXT("Loft is dynamic loft"), Club.bHasDynamicLoft && FMath::IsNearlyEqual(Club.DynamicLoftDeg, 24.0f));
            TestFalse(TEXT("Face not sent"), Club.bHasFaceToTarget);
            TestFalse(TEXT("Impact not sent"), Club.bHasImpactHorizontal || Club.bHasImpactVertical);
        }
    }

    // Heartbeats and status updates carry no shot.
    {
        FOpenConnectMessage Message;
        FString Error;
        TestTrue(TEXT("Heartbeat"), OpenConnectProtocol::Decode(TEXT(R"({"DeviceID":"LM","ShotDataOptions":{"ContainsBallData":false,"IsHeartBeat":true,"LaunchMonitorIsReady":false}})"), Message, Error));
        TestTrue(TEXT("Heartbeat flag"), Message.bHeartbeat);
        TestFalse(TEXT("Not ready"), Message.bReady.Get(true));
        TestFalse(TEXT("No shot"), Message.Shot.IsSet());
        TestTrue(TEXT("Heartbeat with stale BallData"), OpenConnectProtocol::Decode(TEXT(R"({"BallData":{"Speed":100,"VLA":10},"ShotDataOptions":{"ContainsBallData":true,"IsHeartBeat":true}})"), Message, Error));
        TestFalse(TEXT("A heartbeat never flies"), Message.Shot.IsSet());
    }

    // Unusable messages are rejected with a reason.
    {
        FOpenConnectMessage Message;
        FString Error;
        TestFalse(TEXT("Bad JSON"), OpenConnectProtocol::Decode(TEXT("{\"BallData\":"), Message, Error));
        TestFalse(TEXT("Missing VLA"), OpenConnectProtocol::Decode(TEXT(R"({"BallData":{"Speed":100}})"), Message, Error));
        TestEqual(TEXT("Missing VLA reason"), Error, FString(TEXT("BallData needs Speed and VLA")));
        TestFalse(TEXT("Flagged without BallData"), OpenConnectProtocol::Decode(TEXT(R"({"ShotDataOptions":{"ContainsBallData":true}})"), Message, Error));
        TestFalse(TEXT("Implausible speed"), OpenConnectProtocol::Decode(TEXT(R"({"BallData":{"Speed":600,"VLA":10}})"), Message, Error));
        TestFalse(TEXT("Implausible spin"), OpenConnectProtocol::Decode(TEXT(R"({"BallData":{"Speed":100,"VLA":10,"TotalSpin":40000}})"), Message, Error));
        TestFalse(TEXT("Zero speed"), OpenConnectProtocol::Decode(TEXT(R"({"BallData":{"Speed":0,"VLA":10}})"), Message, Error));
    }

    // Replies.
    {
        const TSharedPtr<FJsonObject> Ok = ParseObject(OpenConnectProtocol::Response(200, TEXT("Shot received successfully")));
        TestTrue(TEXT("200 reply"), Ok.IsValid() && Ok->GetIntegerField(TEXT("Code")) == 200 && Ok->GetStringField(TEXT("Message")) == TEXT("Shot received successfully"));
        const TSharedPtr<FJsonObject> Player = ParseObject(OpenConnectProtocol::PlayerInformation(true, ELaunchMonitorClub::Putter));
        const TSharedPtr<FJsonObject>* PlayerObject = nullptr;
        TestTrue(TEXT("201 reply"), Player.IsValid() && Player->GetIntegerField(TEXT("Code")) == 201 && Player->TryGetObjectField(TEXT("Player"), PlayerObject));
        if (PlayerObject)
        {
            TestEqual(TEXT("Handedness"), (*PlayerObject)->GetStringField(TEXT("Handed")), FString(TEXT("LH")));
            TestEqual(TEXT("Putter code"), (*PlayerObject)->GetStringField(TEXT("Club")), FString(TEXT("PT")));
        }
        TestEqual(TEXT("Seven iron code"), FString(OpenConnectProtocol::ClubCode(ELaunchMonitorClub::Iron7)), FString(TEXT("I7")));
        TestEqual(TEXT("Driver code"), FString(OpenConnectProtocol::ClubCode(ELaunchMonitorClub::Driver)), FString(TEXT("DR")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenConnectValidationTest, "UOpenGolf.LaunchMonitor.OpenConnect.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenConnectValidationTest::RunTest(const FString&)
{
    FOpenConnectMessage Message;
    FString Error;
    auto Rejected = [&](const TCHAR* Json) { return !OpenConnectProtocol::Decode(Json, Message, Error) && !Error.IsEmpty(); };

    // Every ball number is bounded before it is rounded or flown.
    TestTrue(TEXT("Huge HLA rejected"), Rejected(TEXT(R"({"BallData":{"Speed":100,"VLA":12,"HLA":1e30}})")));
    TestTrue(TEXT("Infinite speed rejected"), Rejected(TEXT(R"({"BallData":{"Speed":1e999,"VLA":12}})")));
    TestTrue(TEXT("Huge total spin rejected"), Rejected(TEXT(R"({"BallData":{"Speed":100,"VLA":12,"TotalSpin":1e12,"SpinAxis":0}})")));
    TestTrue(TEXT("Negative total spin rejected"), Rejected(TEXT(R"({"BallData":{"Speed":100,"VLA":12,"TotalSpin":-5,"SpinAxis":0}})")));
    TestTrue(TEXT("Huge back spin rejected"), Rejected(TEXT(R"({"BallData":{"Speed":100,"VLA":12,"BackSpin":-40000,"SideSpin":0}})")));
    TestTrue(TEXT("Huge side spin rejected"), Rejected(TEXT(R"({"BallData":{"Speed":100,"VLA":12,"BackSpin":3000,"SideSpin":2147483648}})")));
    TestTrue(TEXT("Spin axis past a half turn rejected"), Rejected(TEXT(R"({"BallData":{"Speed":100,"VLA":12,"TotalSpin":3000,"SpinAxis":200}})")));
    TestTrue(TEXT("Back and side spin that derive an implausible total rejected"),
        Rejected(TEXT(R"({"BallData":{"Speed":100,"VLA":12,"BackSpin":29000,"SideSpin":29000}})")));

    // Club data is optional: implausible fields are dropped and the shot still flies.
    TestTrue(TEXT("Shot with bad club fields decodes"), OpenConnectProtocol::Decode(TEXT(R"({"DeviceID":"LM","BallData":{"Speed":90,"VLA":20},
        "ClubData":{"Path":1e9,"FaceToTarget":2.0,"AngleOfAttack":-90,"Loft":120,"HorizontalFaceImpact":500,"VerticalFaceImpact":5},
        "ShotDataOptions":{"ContainsBallData":true,"ContainsClubData":true}})"), Message, Error));
    if (TestTrue(TEXT("Club present"), Message.Club.IsSet()))
    {
        const FLaunchMonitorClubData& Club = Message.Club.GetValue();
        TestFalse(TEXT("Huge path dropped"), Club.bHasPath);
        TestTrue(TEXT("Plausible face kept"), Club.bHasFaceToTarget && FMath::IsNearlyEqual(Club.FaceToTargetDeg, 2.0f));
        TestFalse(TEXT("Steep attack dropped"), Club.bHasAngleOfAttack);
        TestFalse(TEXT("Loft past 90 dropped"), Club.bHasDynamicLoft);
        TestFalse(TEXT("Impact off the face dropped"), Club.bHasImpactHorizontal);
        TestTrue(TEXT("Plausible impact kept"), Club.bHasImpactVertical);
    }

    // DeviceID is shown, logged and recorded: it is cut short and loses control characters.
    const FString LongName = FString::ChrN(200, TEXT('A'));
    TestTrue(TEXT("Long DeviceID decodes"), OpenConnectProtocol::Decode(
        FString::Printf(TEXT(R"({"DeviceID":"Evil\nLine\u202E%s","ShotDataOptions":{"IsHeartBeat":true}})"), *LongName), Message, Error));
    TestEqual(TEXT("DeviceID capped"), Message.DeviceId.Len(), 64);
    TestTrue(TEXT("DeviceID has no newline"), !Message.DeviceId.Contains(TEXT("\n")) && !Message.DeviceId.Contains(TEXT("\x202E")));
    TestTrue(TEXT("DeviceID keeps its text"), Message.DeviceId.StartsWith(TEXT("EvilLineAAA")));
    TestEqual(TEXT("Sanitize strips controls"), OpenConnectProtocol::SanitizeText(TEXT("a\r\nb\tc"), 10), FString(TEXT("abc")));

    // LAN mode accepts only local peers.
    TestTrue(TEXT("Loopback is local"), FOpenConnectServer::IsLocalNetworkAddress(0x7F000001));
    TestTrue(TEXT("10/8 is local"), FOpenConnectServer::IsLocalNetworkAddress(0x0A010203));
    TestTrue(TEXT("172.16/12 is local"), FOpenConnectServer::IsLocalNetworkAddress(0xAC1F0001));
    TestFalse(TEXT("172.32 is not local"), FOpenConnectServer::IsLocalNetworkAddress(0xAC200001));
    TestTrue(TEXT("192.168/16 is local"), FOpenConnectServer::IsLocalNetworkAddress(0xC0A80105));
    TestTrue(TEXT("Link-local is local"), FOpenConnectServer::IsLocalNetworkAddress(0xA9FE0001));
    TestFalse(TEXT("A public address is not local"), FOpenConnectServer::IsLocalNetworkAddress(0x08080808));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenConnectServerLimitsTest, "UOpenGolf.LaunchMonitor.OpenConnect.ServerLimits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenConnectServerLimitsTest::RunTest(const FString&)
{
    // The flood test logs its disconnect on purpose.
    AddExpectedMessage(TEXT("messages a second"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, false);
    // So do the oversized and junk streams.
    AddExpectedMessage(TEXT("without a complete message"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2, false);

    const TSharedRef<FOpenConnectServer> Server = MakeShared<FOpenConnectServer>();
    bool bListening = false;
    bool bFailed = false;
    int32 Connects = 0;
    int32 Disconnects = 0;
    int32 Messages = 0;
    Server->OnListening = [&] { bListening = true; };
    Server->OnListenFailed = [&](const FString&) { bFailed = true; };
    Server->OnClientConnected = [&](const FString&) { ++Connects; };
    Server->OnClientDisconnected = [&] { ++Disconnects; };
    Server->OnMessage = [&](const FString&) { ++Messages; };
    int32 Overflows = 0;
    Server->OnOverflow = [&] { ++Overflows; };
    auto Pump = [&](TFunctionRef<bool()> Done, double Seconds)
    {
        const double End = FPlatformTime::Seconds() + Seconds;
        while (FPlatformTime::Seconds() < End)
        {
            Server->ProcessEvents();
            if (Done())
                return true;
            FPlatformProcess::Sleep(0.01f);
        }
        return false;
    };

    FRandomStream Random(FPlatformTime::Cycles());
    int32 Port = 0;
    for (int32 Attempt = 0; Attempt < 5 && !bListening; ++Attempt)
    {
        Port = Random.RandRange(40000, 59999);
        bFailed = false;
        Server->Start(Port, false);
        Pump([&] { return bListening || bFailed; }, 5.0);
    }
    if (!TestTrue(TEXT("Listening"), bListening))
        return false;

    // Nobody else can bind the port while the game listens, even asking to reuse it.
    FSocket* Squatter = FTcpSocketBuilder(TEXT("OpenConnectSquatter")).AsReusable()
        .BoundToEndpoint(FIPv4Endpoint(FIPv4Address::InternalLoopback, static_cast<uint16>(Port))).Listening(1).Build();
    TestNull(TEXT("A second listener cannot take the port"), Squatter);
    if (Squatter)
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Squatter);

    // A connector restarting from the same address replaces its old connection.
    FTestConnector First;
    TestTrue(TEXT("First connector attaches"), First.Connect(Port) && Pump([&] { return Connects == 1; }, 5.0));
    FTestConnector Restarted;
    TestTrue(TEXT("Same-address connector replaces it"), Restarted.Connect(Port) && Pump([&] { return Connects == 2 && Disconnects == 1; }, 5.0));

    // A burst within the allowance is delivered in full.
    FString Burst;
    for (int32 Index = 0; Index < 30; ++Index)
        Burst += TEXT("{}");
    TestTrue(TEXT("Burst sent"), Restarted.Send(Burst));
    TestTrue(TEXT("Burst delivered"), Pump([&] { return Messages == 30; }, 5.0));

    // A sustained flood is held to the rate limit, then disconnected.
    Messages = 0;
    Restarted.MakeNonBlocking();
    FString Chunk;
    for (int32 Index = 0; Index < 200; ++Index)
        Chunk += TEXT("{}");
    const double FloodStart = FPlatformTime::Seconds();
    while (Disconnects < 2 && FPlatformTime::Seconds() - FloodStart < FOpenConnectServer::FloodDisconnectSeconds + 5.0)
    {
        Restarted.Send(Chunk);
        Server->ProcessEvents();
        FPlatformProcess::Sleep(0.01f);
    }
    const double Elapsed = FPlatformTime::Seconds() - FloodStart;
    TestEqual(TEXT("Flooding connector dropped"), Disconnects, 2);
    TestTrue(FString::Printf(TEXT("Flood held to the rate limit (%d messages in %.1f s)"), Messages, Elapsed),
        Messages <= FOpenConnectServer::MessageBurst + FOpenConnectServer::MessagesPerSecond * Elapsed + 5);
    TestTrue(TEXT("Dropped only after sustained abuse"), Elapsed >= FOpenConnectServer::FloodDisconnectSeconds - 0.5);

    // An object that never closes overflows (413) a few times, then the connector is dropped.
    const int32 StreamBytes = FOpenConnectServer::MaxBytesWithoutMessage + 64 * 1024;
    FTestConnector Oversized;
    TestTrue(TEXT("Oversized connector attaches"), Oversized.Connect(Port) && Pump([&] { return Connects == 3; }, 5.0));
    Oversized.Send(TEXT("{\"BallData\":\"") + FString::ChrN(StreamBytes, TEXT('a')));
    TestTrue(TEXT("Oversized stream dropped"), Pump([&] { return Disconnects == 3; }, 10.0));
    TestTrue(FString::Printf(TEXT("Oversized stream overflowed a bounded number of times (%d)"), Overflows), Overflows >= 1 && Overflows <= 5);

    // Bytes outside any object are discarded unseen, and are dropped the same way.
    Overflows = 0;
    FTestConnector Junk;
    TestTrue(TEXT("Junk connector attaches"), Junk.Connect(Port) && Pump([&] { return Connects == 4; }, 5.0));
    Junk.Send(FString::ChrN(StreamBytes, TEXT(' ')));
    TestTrue(TEXT("Junk stream dropped"), Pump([&] { return Disconnects == 4; }, 10.0));
    TestEqual(TEXT("Junk is not an oversized object"), Overflows, 0);
    Server->Stop();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenConnectLoopbackTest, "UOpenGolf.LaunchMonitor.OpenConnect.Loopback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenConnectLoopbackTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
    UOpenLaunchMonitorSubsystem* Subsystem = NewObject<UOpenLaunchMonitorSubsystem>(GameInstance);
    TArray<FLaunchMonitorShot> Shots;
    TArray<FLaunchMonitorClubData> Clubs;
    bool bRejectNext = false;
    Subsystem->OnShotReceivedNative.AddLambda([&](const FLaunchMonitorShot& Shot)
    {
        Shots.Add(Shot);
        if (bRejectNext)
            Subsystem->RejectShot(Shot.ShotNumber, TEXT("Menu open"));
    });
    Subsystem->OnClubDataReceivedNative.AddLambda([&Clubs](const FLaunchMonitorClubData& Club) { Clubs.Add(Club); });

    // A random high port, retried if something else holds it.
    FRandomStream Random(FPlatformTime::Cycles());
    int32 Port = 0;
    Subsystem->SetProvider(ELaunchMonitorProvider::OpenConnect);
    for (int32 Attempt = 0; Attempt < 5 && Subsystem->GetConnectionState() != ELaunchMonitorConnectionState::Connecting; ++Attempt)
    {
        Port = Random.RandRange(40000, 59999);
        Subsystem->SetOpenConnectEndpoint(Port, false);
        Subsystem->Connect();
        PumpUntil([&] { return Subsystem->GetConnectionState() == ELaunchMonitorConnectionState::Connecting || !Subsystem->GetLastError().IsEmpty(); });
    }
    if (!TestEqual(TEXT("Listening"), Subsystem->GetConnectionState(), ELaunchMonitorConnectionState::Connecting))
        return false;
    TestEqual(TEXT("Address"), Subsystem->GetOpenConnectAddress(), FString::Printf(TEXT("127.0.0.1:%d"), Port));

    FTestConnector Connector;
    if (!TestTrue(TEXT("Connector connects"), Connector.Connect(Port)))
        return false;
    TestTrue(TEXT("Attached"), PumpUntil([&] { return Subsystem->GetConnectionState() == ELaunchMonitorConnectionState::Connected; }));
    FString Reply;
    TestTrue(TEXT("Player information on attach"), PumpUntil([&] { return Connector.Poll(Reply); }));
    TestTrue(TEXT("201 with the selected club"), Reply.Contains(TEXT("\"Code\":201")) && Reply.Contains(TEXT("\"Club\":\"DR\"")));

    // A shot split across two writes, with club data.
    const FString Shot = TEXT(R"({"DeviceID":"Test LM","ShotNumber":1,"BallData":{"Speed":120.0,"VLA":11.5,"HLA":-1.5,"TotalSpin":2800,"SpinAxis":4.0},"ClubData":{"Path":1.5},"ShotDataOptions":{"ContainsBallData":true,"ContainsClubData":true}})");
    TestTrue(TEXT("First half sent"), Connector.Send(Shot.Left(40)));
    FPlatformProcess::Sleep(0.05f);
    TestTrue(TEXT("Second half sent"), Connector.Send(Shot.RightChop(40)));
    TestTrue(TEXT("Shot delivered"), PumpUntil([&] { return Shots.Num() == 1; }));
    if (Shots.Num() == 1)
    {
        TestEqual(TEXT("Source names the connector"), Shots[0].Source, FString(TEXT("Open Connect: Test LM")));
        TestEqual(TEXT("Speed"), Shots[0].GetBallSpeedMph(), 120.0f, 0.001f);
        TestEqual(TEXT("HLA"), Shots[0].HorizontalLaunchDeg, -1.5f, 0.001f);
        TestEqual(TEXT("Ready after a shot"), Subsystem->GetConnectionState(), ELaunchMonitorConnectionState::Ready);
        TestEqual(TEXT("Device name from DeviceID"), Subsystem->GetDeviceName(), FString(TEXT("Test LM")));
    }
    TestTrue(TEXT("Club data follows its shot"), Clubs.Num() == 1 && Shots.Num() == 1 && Clubs[0].ShotNumber == Shots[0].ShotNumber && Clubs[0].bHasPath);
    TestTrue(TEXT("Shot acknowledged"), PumpUntil([&] { return Connector.Poll(Reply); }));
    TestTrue(TEXT("200 for an accepted shot"), Reply.Contains(TEXT("\"Code\":200")));

    // A shot the game refuses is answered with 501, so the connector doesn't count it.
    bRejectNext = true;
    TestTrue(TEXT("Second shot sent"), Connector.Send(TEXT(R"({"BallData":{"Speed":80,"VLA":20},"ShotDataOptions":{"ContainsBallData":true}})")));
    TestTrue(TEXT("Rejected shot acknowledged"), PumpUntil([&] { return Connector.Poll(Reply); }));
    TestTrue(TEXT("501 with the reason"), Reply.Contains(TEXT("\"Code\":501")) && Reply.Contains(TEXT("Menu open")));
    bRejectNext = false;

    // Malformed data and a heartbeat.
    TestTrue(TEXT("Bad shot sent"), Connector.Send(TEXT(R"({"BallData":{"Speed":80}})")));
    TestTrue(TEXT("Bad shot answered"), PumpUntil([&] { return Connector.Poll(Reply); }));
    TestTrue(TEXT("501 for missing VLA"), Reply.Contains(TEXT("\"Code\":501")));
    TestEqual(TEXT("Bad shot not delivered"), Shots.Num(), 2);
    TestTrue(TEXT("Heartbeat sent"), Connector.Send(TEXT(R"({"ShotDataOptions":{"ContainsBallData":false,"IsHeartBeat":true,"LaunchMonitorIsReady":true}})")));
    TestTrue(TEXT("Heartbeat answered"), PumpUntil([&] { return Connector.Poll(Reply); }));
    TestTrue(TEXT("200 for a heartbeat"), Reply.Contains(TEXT("\"Code\":200")));

    // Club changes are sent to the connector.
    Subsystem->SetClub(ELaunchMonitorClub::Iron7);
    TestTrue(TEXT("Club change sent"), PumpUntil([&] { return Connector.Poll(Reply); }));
    TestTrue(TEXT("201 with the new club"), Reply.Contains(TEXT("\"Club\":\"I7\"")));

    // The connector leaving returns to listening; stopping closes the port.
    Connector.Close();
    TestTrue(TEXT("Back to listening"), PumpUntil([&] { return Subsystem->GetConnectionState() == ELaunchMonitorConnectionState::Connecting; }));
    Subsystem->Disconnect();
    TestEqual(TEXT("Stopped"), Subsystem->GetConnectionState(), ELaunchMonitorConnectionState::Disconnected);
    TestFalse(TEXT("Not listening"), Subsystem->IsOpenConnectListening());
    FTestConnector Late;
    TestFalse(TEXT("The port is closed after stopping"), Late.Connect(Port));

    // Switching to Square Home closes the port at once, including with a connector attached.
    Subsystem->Connect();
    TestTrue(TEXT("Listening again"), PumpUntil([&] { return Subsystem->GetConnectionState() == ELaunchMonitorConnectionState::Connecting; }));
    FTestConnector Attached;
    TestTrue(TEXT("A connector attaches before the switch"), Attached.Connect(Port) &&
        PumpUntil([&] { return Subsystem->GetConnectionState() == ELaunchMonitorConnectionState::Connected; }));
    Subsystem->SetProvider(ELaunchMonitorProvider::SquareHome);
    TestFalse(TEXT("Provider switch stops listening"), Subsystem->IsOpenConnectListening());
    TestEqual(TEXT("Square Home starts disconnected"), Subsystem->GetConnectionState(), ELaunchMonitorConnectionState::Disconnected);
    FTestConnector AfterSwitch;
    TestFalse(TEXT("Nothing listens on the port after switching to Square Home"), AfterSwitch.Connect(Port));
    // The attached connector's connection was closed too: a read reports the closed connection.
    uint8 Byte = 0;
    int32 Read = 0;
    Attached.Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(2));
    TestFalse(TEXT("The attached connector is disconnected"), Attached.Socket->Recv(&Byte, 1, Read));
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenConnectReconnectTest, "UOpenGolf.LaunchMonitor.OpenConnect.ReconnectIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenConnectReconnectTest::RunTest(const FString&)
{
    const auto Server = MakeShared<FOpenConnectServer>();
    auto Wait = [](TFunctionRef<bool()> Done)
    {
        const double End = FPlatformTime::Seconds() + 3.0;
        while (FPlatformTime::Seconds() < End)
        {
            if (Done()) return true;
            FPlatformProcess::Sleep(0.005f);
        }
        return false;
    };
    const int32 Port = FRandomStream(FPlatformTime::Cycles()).RandRange(40000, 59999);
    bool Listening = false;
    Server->OnListening = [&] { Listening = true; };
    Server->Start(Port, false);
    if (!TestTrue(TEXT("Listener starts"), Wait([&] { Server->ProcessEvents(); return Listening; }))) return false;
    FTestConnector First;
    if (!TestTrue(TEXT("First connects"), First.Connect(Port))) return false;
    TestTrue(TEXT("First admitted"), Wait([&] { return Server->Channel->ClientGeneration.load() != 0; }));
    const uint64 OldGeneration = Server->Channel->ClientGeneration.load();
    Server->ProcessEvents();
    First.Send(TEXT("{}"));
    TestTrue(TEXT("Old request queued"), Wait([&] { return Server->Channel->PendingEvents.load() > 0; }));
    FTestConnector Replacement;
    if (!TestTrue(TEXT("Replacement connects"), Replacement.Connect(Port))) return false;
    TestTrue(TEXT("Replacement admitted before dispatch"), Wait([&] { return Server->Channel->ClientGeneration.load() > OldGeneration; }));
    Server->OnMessage = [&](const FString&) { Server->Send(TEXT("{\"old\":true}")); };
    Server->OnClientConnected = [&](const FString&) { Server->Send(TEXT("{\"new\":true}")); };
    Server->ProcessEvents();
    FString Reply;
    TestTrue(TEXT("Replacement receives a reply"), Wait([&] { return Replacement.Poll(Reply); }));
    TestEqual(TEXT("Only replacement acknowledgement delivered"), Reply, FString(TEXT("{\"new\":true}")));

    // Rapid replacements must share the listener's rate allowance.
    int32 Messages = 0;
    Server->OnMessage = [&](const FString&) { ++Messages; };
    Server->OnClientConnected = nullptr;
    FString Burst;
    for (int32 Index = 0; Index < 80; ++Index) Burst += TEXT("{}");
    const double RateStart = FPlatformTime::Seconds();
    for (int32 Index = 0; Index < 8; ++Index)
    {
        FTestConnector Churn;
        Churn.Connect(Port);
        Churn.Send(Burst);
        const double Until = FPlatformTime::Seconds() + 0.1;
        while (FPlatformTime::Seconds() < Until)
        {
            Server->ProcessEvents();
            FPlatformProcess::Sleep(0.005f);
        }
    }
    Server->ProcessEvents();
    TestTrue(TEXT("Reconnects cannot replenish rate allowance"),
        Messages <= FOpenConnectServer::MessageBurst + FOpenConnectServer::MessagesPerSecond * (FPlatformTime::Seconds() - RateStart) + 2);

    // Queue saturation must refuse replacements rather than grow the lifecycle backlog.
    Server->OnClientConnected = nullptr;
    Server->OnMessage = nullptr;
    for (int32 Index = 0; Index < 80; ++Index)
    {
        FTestConnector Churn;
        Churn.Connect(Port);
        FPlatformProcess::Sleep(0.005f);
        TestTrue(TEXT("Lifecycle backlog bounded"), Server->Channel->PendingEvents.load() <= FOpenConnectServer::MaxPendingEvents);
    }
    const double Start = FPlatformTime::Seconds();
    Server->Stop();
    TestTrue(TEXT("Shutdown stays prompt during churn"), FPlatformTime::Seconds() - Start < 2.0);

    // Restarting the listener inside a request callback must not redirect its reply.
    Listening = false;
    Server->Start(Port, false);
    TestTrue(TEXT("Listener restarts"), Wait([&] { Server->ProcessEvents(); return Listening; }));
    FTestConnector BeforeRestart;
    BeforeRestart.Connect(Port);
    TestTrue(TEXT("Client before restart admitted"), Wait([&] { return Server->Channel->ClientGeneration.load() != 0; }));
    Server->ProcessEvents();
    bool Restarted = false;
    FTestConnector AfterRestart;
    Server->OnMessage = [&](const FString&)
    {
        Server->Start(Port, false);
        Wait([&] { return Server->Channel->PendingEvents.load() > 0; });
        AfterRestart.Connect(Port);
        Wait([&] { return Server->Channel->ClientGeneration.load() != 0; });
        Server->Send(TEXT("{\"stale\":true}"));
        Restarted = true;
    };
    BeforeRestart.Send(TEXT("{}"));
    TestTrue(TEXT("Callback restarted listener"), Wait([&] { Server->ProcessEvents(); return Restarted; }));
    Server->Send(TEXT("{\"fresh\":true}"));
    TestTrue(TEXT("New listener responds"), Wait([&] { return AfterRestart.Poll(Reply); }));
    TestEqual(TEXT("Old callback cannot reply through new listener"), Reply, FString(TEXT("{\"fresh\":true}")));
    Server->Stop();
    return true;
}

#endif
