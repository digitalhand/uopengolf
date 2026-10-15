// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Square/SquareProtocol.h"
#include "Square/SquareScheduler.h"
#include "Square/SquareSession.h"

namespace
{
    /** Completes every request immediately and records written commands as hex. */
    class FFakeGattTransport final : public IGattTransport
    {
    public:
        virtual void StartScan(const FString& NamePrefix) override { ScanPrefix = NamePrefix; bScanning = true; }
        virtual void StopScan() override { bScanning = false; }
        virtual void Connect(const FString& DeviceId, const TArray<FString>&, const TArray<FString>&, FCompletion OnComplete) override
        {
            ConnectedId = DeviceId;
            OnComplete(true, FString());
        }
        virtual void Disconnect() override { ++DisconnectCalls; }
        virtual void Read(const FString& Uuid, TFunction<void(const TArray<uint8>&)> OnComplete) override
        {
            const FSquareConnectionOptions Options;
            if (Uuid == Options.BatteryCharacteristicUuid)
            {
                OnComplete({85});
            }
            else
            {
                const FTCHARToUTF8 Firmware(TEXT("{\"lm\":\"1.4.2\"}"));
                OnComplete(TArray<uint8>(reinterpret_cast<const uint8*>(Firmware.Get()), Firmware.Length()));
            }
        }
        virtual void Subscribe(const FString& Uuid, FCompletion OnComplete) override { OnComplete(true, FString()); }
        virtual void Write(const FString& Uuid, const TArray<uint8>& Value, FCompletion OnComplete) override
        {
            Writes.Add(SquareProtocol::ToHex(Value));
            OnComplete(true, FString());
        }

        void Notify(const TCHAR* Hex)
        {
            TArray<uint8> Bytes;
            verify(SquareProtocol::FromHex(Hex, Bytes));
            OnNotification(FSquareConnectionOptions().EventCharacteristicUuid, Bytes);
        }

        int32 CountWrites(const TCHAR* Prefix) const
        {
            return Writes.FilterByPredicate([Prefix](const FString& Write) { return Write.StartsWith(Prefix); }).Num();
        }

        FString ScanPrefix;
        bool bScanning = false;
        FString ConnectedId;
        int32 DisconnectCalls = 0;
        TArray<FString> Writes;
    };

    class FManualScheduler final : public ISquareScheduler
    {
    public:
        virtual void Schedule(float DelaySeconds, TFunction<void()> Work) override { Pending.Add({Now + DelaySeconds, MoveTemp(Work)}); }

        void Advance(float Seconds)
        {
            const double End = Now + Seconds;
            while (true)
            {
                int32 Next = INDEX_NONE;
                for (int32 Index = 0; Index < Pending.Num(); ++Index)
                {
                    if (Pending[Index].Key <= End && (Next == INDEX_NONE || Pending[Index].Key < Pending[Next].Key))
                    {
                        Next = Index;
                    }
                }
                if (Next == INDEX_NONE)
                {
                    break;
                }
                TPair<double, TFunction<void()>> Item = MoveTemp(Pending[Next]);
                Pending.RemoveAt(Next);
                Now = Item.Key;
                Item.Value();
            }
            Now = End;
        }

    private:
        double Now = 0.0;
        TArray<TPair<double, TFunction<void()>>> Pending;
    };

    const TCHAR* ShotHex = TEXT("1102370A0A9A09C8001A16E8FF7C15D0FF");
    const TCHAR* SecondShotHex = TEXT("1102370A0B9A09C8001A16E8FF7C15D0FF");
    const TCHAR* ClubHex = TEXT("11070FC0FB1A00A1FEFB0C");
    const TCHAR* UnreadableShotHex = TEXT("11023700809A09C8001A16E8FF7C15D0FF");
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaunchMonitorSessionTest, "UOpenGolf.LaunchMonitor.Session",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLaunchMonitorSessionTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FFakeGattTransport> Transport = MakeShared<FFakeGattTransport>();
    const TSharedRef<FManualScheduler> Scheduler = MakeShared<FManualScheduler>();
    const TSharedRef<FSquareSession> Session = MakeShared<FSquareSession>(Transport, Scheduler);
    Session->Initialize();

    TArray<ESquareSessionStatus> Statuses;
    TArray<int32> ShotIds;
    TArray<int32> ClubShotIds;
    TArray<bool> ReadyChanges;
    int32 Battery = -1;
    FString Firmware;
    int32 ConnectionLost = 0;
    Session->OnStatusChanged = [&Statuses](ESquareSessionStatus Status) { Statuses.Add(Status); };
    Session->OnShot = [&ShotIds](const FSquareShotMetrics&, int32 ShotId) { ShotIds.Add(ShotId); };
    Session->OnClub = [&ClubShotIds](const FSquareClubMetrics&, int32 ShotId) { ClubShotIds.Add(ShotId); };
    Session->OnReadyChanged = [&ReadyChanges](bool bReady) { ReadyChanges.Add(bReady); };
    Session->OnBatteryChanged = [&Battery](int32 Percent) { Battery = Percent; };
    Session->OnFirmwareChanged = [&Firmware](const FString& Value) { Firmware = Value; };
    Session->OnConnectionLost = [&ConnectionLost]() { ++ConnectionLost; };

    // Scan, then connect to the first matching device.
    Session->Connect();
    TestTrue(TEXT("Scanning"), Session->GetStatus() == ESquareSessionStatus::Scanning && Transport->bScanning);
    TestEqual(TEXT("Scan prefix"), Transport->ScanPrefix, FString(TEXT("SquareGolf")));
    Transport->OnDeviceDiscovered({TEXT("123456"), TEXT("SquareGolf-Home"), -60});
    TestEqual(TEXT("Connected device"), Transport->ConnectedId, FString(TEXT("123456")));
    TestFalse(TEXT("Scan stopped"), Transport->bScanning);
    TestEqual(TEXT("Battery read"), Battery, 85);
    TestEqual(TEXT("Firmware read"), Firmware, FString(TEXT("1.4.2")));

    // Handshake: heartbeat now, club after 2 s, arm after 3 more seconds.
    TestTrue(TEXT("Connected before arming"), Session->GetStatus() == ESquareSessionStatus::Connected);
    TestEqual(TEXT("First write is heartbeat"), Transport->Writes, TArray<FString>{TEXT("1183000000000000")});
    Scheduler->Advance(2.0f);
    TestEqual(TEXT("Club after 2 s"), Transport->Writes.Last(), FString(TEXT("118201020400000000")));
    Scheduler->Advance(2.9f);
    TestTrue(TEXT("Not armed before 3 s"), Session->GetStatus() == ESquareSessionStatus::Connected);
    Scheduler->Advance(0.1f);
    TestEqual(TEXT("Arm after 3 s"), Transport->Writes.Last(), FString(TEXT("118102011100000000")));
    TestTrue(TEXT("Ready"), Session->GetStatus() == ESquareSessionStatus::Ready);
    TestTrue(TEXT("Status sequence"), Statuses == TArray<ESquareSessionStatus>{ESquareSessionStatus::Scanning, ESquareSessionStatus::Connecting,
                                                     ESquareSessionStatus::Connected, ESquareSessionStatus::Ready});

    // Heartbeat every 5 s.
    Scheduler->Advance(5.0f);
    TestEqual(TEXT("Periodic heartbeat"), Transport->CountWrites(TEXT("1183")), 2);

    // A shot is reported once even though the device repeats it, and requests club data.
    Transport->Notify(ShotHex);
    Transport->Notify(ShotHex);
    TestEqual(TEXT("One shot from duplicate frames"), ShotIds, TArray<int32>{1});
    TestEqual(TEXT("Club data requested"), Transport->CountWrites(TEXT("1187")), 1);
    TestFalse(TEXT("Not ready after shot"), ReadyChanges.Last());

    // The club frame is accepted once and tied to the shot.
    Transport->Notify(ClubHex);
    Transport->Notify(ClubHex);
    TestEqual(TEXT("Club for shot 1 once"), ClubShotIds, TArray<int32>{1});

    // Re-arm 3 s after the shot.
    const int32 ArmsBeforeRearm = Transport->CountWrites(TEXT("1181"));
    Scheduler->Advance(3.0f);
    TestEqual(TEXT("Re-armed after shot"), Transport->CountWrites(TEXT("1181")), ArmsBeforeRearm + 1);

    // A club frame after re-arm cannot be tied to a shot.
    Transport->Notify(ClubHex);
    TestEqual(TEXT("Late club frame ignored"), ClubShotIds.Num(), 1);

    // An unreadable strike is not reported but still re-arms the device.
    Transport->Notify(UnreadableShotHex);
    TestEqual(TEXT("Unreadable shot not reported"), ShotIds.Num(), 1);
    const int32 ArmsBeforeUnreadable = Transport->CountWrites(TEXT("1181"));
    Scheduler->Advance(3.0f);
    TestEqual(TEXT("Re-armed after unreadable shot"), Transport->CountWrites(TEXT("1181")), ArmsBeforeUnreadable + 1);

    // A different frame is a new shot.
    Transport->Notify(SecondShotHex);
    TestEqual(TEXT("Second shot"), ShotIds, TArray<int32>({1, 2}));

    // Sensor frames drive ball readiness.
    Transport->Notify(TEXT("1101000101000000000000000000000000"));
    TestTrue(TEXT("Ball detected"), ReadyChanges.Last());

    // Losing the link disconnects and is reported once.
    Transport->OnDisconnected();
    TestTrue(TEXT("Disconnected after link loss"), Session->GetStatus() == ESquareSessionStatus::Disconnected);
    TestEqual(TEXT("Connection lost reported"), ConnectionLost, 1);
    TestFalse(TEXT("Not ready after link loss"), ReadyChanges.Last());
    const int32 WritesAfterLoss = Transport->Writes.Num();
    Scheduler->Advance(30.0f);
    TestEqual(TEXT("No writes after link loss"), Transport->Writes.Num(), WritesAfterLoss);
    Transport->Notify(SecondShotHex);
    TestEqual(TEXT("Frames ignored after link loss"), ShotIds.Num(), 2);
    Transport->OnDisconnected();
    TestEqual(TEXT("Link loss not repeated"), ConnectionLost, 1);

    // Selecting the putter after reconnecting drops club angles.
    Session->SetClub(SquareProtocol::PutterClubCode);
    Session->ConnectToDevice({TEXT("123456"), TEXT("SquareGolf-Home"), 0});
    Scheduler->Advance(5.0f);
    TestTrue(TEXT("Ready after reconnect"), Session->GetStatus() == ESquareSessionStatus::Ready);
    const int32 LastClubCommand = Transport->Writes.FindLastByPredicate([](const FString& Write) { return Write.StartsWith(TEXT("1182")); });
    TestTrue(TEXT("Putter club command"), LastClubCommand != INDEX_NONE && Transport->Writes[LastClubCommand].Mid(6, 4) == TEXT("0107"));
    FSquareClubMetrics PutterClub;
    Session->OnClub = [&PutterClub](const FSquareClubMetrics& Club, int32) { PutterClub = Club; };
    Transport->Notify(TEXT("1102138101E6FF91010000000000000000"));
    Transport->Notify(ClubHex);
    TestFalse(TEXT("Putter club angles dropped"), PutterClub.HasReading());

    // Scanning gives up after the timeout.
    Session->Connect();
    Scheduler->Advance(30.0f);
    TestTrue(TEXT("Scan timeout disconnects"), Session->GetStatus() == ESquareSessionStatus::Disconnected);
    TestFalse(TEXT("Scan stopped after timeout"), Transport->bScanning);
    return true;
}

#endif
