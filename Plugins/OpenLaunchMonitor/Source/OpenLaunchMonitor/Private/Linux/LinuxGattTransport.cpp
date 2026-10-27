// SPDX-License-Identifier: MIT
// Derived from open-lm-connector (MIT) addons/common/bluetooth/linux/LinuxBluetoothGattClient.cs at 655fd96
// (the Windows port pinned 92eb021, which is not in the local checkout), ported to sd-bus with a link-loss event.
#include "Bluetooth/GattTransport.h"

#if PLATFORM_LINUX

#include "Async/Async.h"
#include "Bluetooth/BlueZModel.h"
#include "Containers/Queue.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Linux/BlueZBus.h"
#include "OpenLaunchMonitorLog.h"

#include <atomic>

namespace LinuxGatt
{
using namespace BlueZModel;

namespace
{
    constexpr int32 ConnectAttempts = 12;
    constexpr int32 ServiceResolveAttempts = 12;
    constexpr double RetryDelaySeconds = 1.0;
    constexpr double ConnectSeconds = 30.0;
    constexpr double CleanupSeconds = 2.0;
    /** Longer than BlueZ's 2 s disconnect timer, after which a released link is really down. */
    constexpr double DisconnectSettleSeconds = 3.0;
    constexpr double PollSeconds = 0.025;

    bool IsBatteryLevel(const FString& Uuid)
    {
        return Uuid.Equals(BatteryLevelUuid, ESearchCase::IgnoreCase);
    }

    void LogReply(const FBlueZReply& Reply)
    {
        if (IsExpectedOutcome(Reply.ErrorName))
        {
            UE_LOG(LogOpenLaunchMonitor, Log, TEXT("BlueZ call ended: %s %s"), *Reply.ErrorName, *Reply.ErrorMessage);
        }
        else
        {
            UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("BlueZ error %s: %s"), *Reply.ErrorName, *Reply.ErrorMessage);
        }
    }

    FString Describe(const FBlueZReply& Reply)
    {
        LogReply(Reply);
        return FriendlyError(Reply.ErrorName, Reply.ErrorMessage);
    }

    /** Describe for Device1.Connect and the services wait. */
    FString DescribeConnect(const FBlueZReply& Reply)
    {
        LogReply(Reply);
        return FriendlyConnectError(Reply.ErrorName, Reply.ErrorMessage);
    }
} // namespace

/**
 * Bluetooth LE GATT client over BlueZ. One worker thread owns the sd-bus connection and runs
 * every D-Bus call with cancellable, bounded waits; results and device events are posted back
 * to the game thread with connection-generation guards, as in the Windows transport.
 */
class FLinuxGattTransport final : public IGattTransport, private FRunnable
{
public:
    static TSharedRef<FLinuxGattTransport> Create()
    {
        const TSharedRef<FLinuxGattTransport> Transport = MakeShared<FLinuxGattTransport>();
        Transport->WeakThis = Transport;
        return Transport;
    }

    FLinuxGattTransport()
    {
        Thread = FRunnableThread::Create(this, TEXT("OpenLaunchMonitorBluetooth"));
    }

    virtual ~FLinuxGattTransport() override
    {
        if (Thread)
        {
            Stop();
            Thread->WaitForCompletion();
            delete Thread;
        }
    }

    virtual void StartScan(const FString& NamePrefix) override
    {
        const uint32 Scan = ++ScanSerial;
        Enqueue([this, NamePrefix, Scan]()
            { if (ScanSerial == Scan) StartScanOnWorker(NamePrefix, Scan); });
    }

    virtual void StopScan() override
    {
        ++ScanSerial;
        Enqueue([this]() { StopScanOnWorker(); }, true);
    }

    virtual void Connect(const FString& DeviceId, const TArray<FString>& RequiredUuids, const TArray<FString>& OptionalUuids,
        FCompletion OnComplete) override
    {
        ++ConnectionSerial;
        Enqueue([this, DeviceId, RequiredUuids, OptionalUuids, OnComplete = MoveTemp(OnComplete)]() mutable
            {
            // Deviation from upstream: one 30 s deadline bounds the whole connection, as on Windows.
            JobDeadline = FPlatformTime::Seconds() + ConnectSeconds;
            const FString Error = ConnectOnWorker(DeviceId, RequiredUuids, OptionalUuids);
            JobDeadline = TNumericLimits<double>::Max();
            if (!Error.IsEmpty())
            {
                DisconnectOnWorker();
            }
            Complete(MoveTemp(OnComplete), Error); });
    }

    virtual void Disconnect() override
    {
        // Invalidate pending link-loss events now; the worker releases the device.
        ++ConnectionSerial;
        ++ScanSerial;
        Enqueue([this]() { StopScanOnWorker(); DisconnectOnWorker(); }, true);
    }

    virtual void Read(const FString& Uuid, TFunction<void(const TArray<uint8>&)> OnComplete) override
    {
        Enqueue([this, Uuid, OnComplete = MoveTemp(OnComplete)]() mutable
            {
            TArray<uint8> Value = ReadOnWorker(Uuid);
            PostToGameThread([OnComplete = MoveTemp(OnComplete), Value = MoveTemp(Value)](FLinuxGattTransport&) { OnComplete(Value); }); });
    }

    virtual void Subscribe(const FString& Uuid, FCompletion OnComplete) override
    {
        Enqueue([this, Uuid, OnComplete = MoveTemp(OnComplete)]() mutable
            { Complete(MoveTemp(OnComplete), SubscribeOnWorker(Uuid)); });
    }

    virtual void Write(const FString& Uuid, const TArray<uint8>& Value, FCompletion OnComplete) override
    {
        Enqueue([this, Uuid, Value, OnComplete = MoveTemp(OnComplete)]() mutable
            { Complete(MoveTemp(OnComplete), WriteOnWorker(Uuid, Value)); });
    }

private:
    // FRunnable
    virtual uint32 Run() override
    {
        FString Error;
        Bus = FBlueZBus::Open(Error);
        if (!Bus)
        {
            InitError = Error;
            UE_LOG(LogOpenLaunchMonitor, Error, TEXT("Bluetooth worker could not open BlueZ: %s"), *InitError);
        }
        while (!bStopping)
        {
            TUniqueFunction<void()> Job;
            while (!bStopping && Jobs.Dequeue(Job))
            {
                Job();
            }
            if (Bus)
            {
                Bus->Pump();
                Bus->WaitForActivity(PollSeconds);
                ReportBusFailure();
            }
            else
            {
                FPlatformProcess::Sleep(PollSeconds);
            }
        }
        if (Bus)
        {
            StopScanOnWorker();
            DisconnectOnWorker();
            Bus.Reset();
        }
        return 0;
    }

    virtual void Stop() override
    {
        bStopping = true;
        ++ConnectionSerial;
        ++ScanSerial;
    }

    bool Cancelled() const { return bStopping || ConnectionSerial != JobGeneration; }

    /**
     * A failed system-bus connection is not reopened: report it once, and fail later jobs at once
     * with the same message (through InitError) instead of waiting for their deadlines.
     */
    void ReportBusFailure()
    {
        if (!InitError.IsEmpty() || Bus->GetFailure().IsEmpty())
            return;
        InitError = FString::Printf(TEXT("Bluetooth stopped working: %s. Restart the game."), *Bus->GetFailure());
        PostError(InitError);
    }

    FBlueZWait OperationWait() const
    {
        return {[this] { return Cancelled(); }, JobDeadline};
    }

    /** Cleanup is not cancellable, so a superseded connection still releases the device, but it is short. */
    static FBlueZWait CleanupWait(double Deadline)
    {
        return {nullptr, Deadline};
    }

    /** Waits between retries while pumping the bus; returns an error when cancelled or past the job deadline. */
    FString RetryDelay()
    {
        const double Until = FMath::Min(JobDeadline, FPlatformTime::Seconds() + RetryDelaySeconds);
        while (!Cancelled() && FPlatformTime::Seconds() < Until)
        {
            Bus->Pump();
            Bus->WaitForActivity(PollSeconds);
        }
        if (Cancelled()) return CancelledError;
        if (FPlatformTime::Seconds() >= JobDeadline) return TEXT("Bluetooth connection timed out.");
        return FString();
    }

    void Enqueue(TUniqueFunction<void()> Job, bool bCleanup = false)
    {
        const uint32 Generation = ConnectionSerial;
        if (!Thread)
        {
            if (bCleanup) return;
            AsyncTask(ENamedThreads::GameThread, [WeakSelf = WeakThis, Generation]
            {
                if (const auto Self = WeakSelf.Pin(); Self && Self->ConnectionSerial == Generation && Self->OnError)
                    Self->OnError(TEXT("Bluetooth worker could not start."));
            });
            return;
        }
        Jobs.Enqueue([this, Generation, bCleanup, Job = MoveTemp(Job)]() mutable
        {
            if (bStopping || ConnectionSerial != Generation) return;
            JobGeneration = Generation;
            if (!InitError.IsEmpty()) { if (!bCleanup) PostError(InitError); return; }
            Job();
        });
    }

    template <typename FunctorType>
    static void PostGuarded(const TWeakPtr<FLinuxGattTransport>& WeakSelf, uint32 Generation, FunctorType&& Functor)
    {
        AsyncTask(ENamedThreads::GameThread, [WeakSelf, Generation, Functor = Forward<FunctorType>(Functor)]() mutable
        {
            if (const auto Self = WeakSelf.Pin(); Self && !Self->bStopping && Self->ConnectionSerial == Generation)
                Functor(*Self);
        });
    }

    template <typename FunctorType>
    void PostToGameThread(FunctorType&& Functor)
    {
        PostGuarded(WeakThis, JobGeneration, Forward<FunctorType>(Functor));
    }

    void PostError(const FString& Error)
    {
        PostToGameThread([Error](FLinuxGattTransport& Self) { if (Self.OnError) Self.OnError(Error); });
    }

    void Complete(FCompletion OnComplete, const FString& Error)
    {
        PostToGameThread([OnComplete = MoveTemp(OnComplete), Error](FLinuxGattTransport&)
            { OnComplete(Error.IsEmpty(), Error); });
    }

    static void PostDiscovered(const TWeakPtr<FLinuxGattTransport>& WeakSelf, uint32 Scan, FGattDeviceInfo Device)
    {
        AsyncTask(ENamedThreads::GameThread, [WeakSelf, Scan, Device = MoveTemp(Device)]()
            {
            const TSharedPtr<FLinuxGattTransport> Self = WeakSelf.Pin();
            if (Self && !Self->bStopping && Self->ScanSerial == Scan && Self->OnDeviceDiscovered)
            {
                Self->OnDeviceDiscovered(Device);
            } });
    }

    static void PostNotification(const TWeakPtr<FLinuxGattTransport>& WeakSelf, uint32 Connection, const FString& Uuid, TArray<uint8> Value)
    {
        PostGuarded(WeakSelf, Connection, [Uuid, Value = MoveTemp(Value)](FLinuxGattTransport& Self)
            { if (Self.OnNotification) Self.OnNotification(Uuid, Value); });
    }

    void StartScanOnWorker(const FString& Prefix, uint32 Scan)
    {
        StopScanOnWorker();
        const FBlueZReply Objects = Bus->GetManagedObjects(OperationWait());
        if (!Objects.Ok())
        {
            PostError(Describe(Objects));
            return;
        }
        AdapterPath = FindAdapterPath(Objects.Objects);
        if (AdapterPath.IsEmpty())
        {
            PostError(TEXT("No BlueZ Bluetooth adapter was found."));
            return;
        }

        const TWeakPtr<FLinuxGattTransport> WeakSelf = WeakThis;
        InterfacesMatch = Bus->MatchInterfacesAdded([WeakSelf, Prefix, Scan](const FString& Path, const FInterfaces& Interfaces)
            {
            FGattDeviceInfo Device;
            if (TryCreateDevice(Path, Interfaces, Prefix, Device)) PostDiscovered(WeakSelf, Scan, MoveTemp(Device)); });
        for (const TPair<FString, FInterfaces>& Object : Objects.Objects)
        {
            FGattDeviceInfo Device;
            if (TryCreateDevice(Object.Key, Object.Value, Prefix, Device))
            {
                PostDiscovered(WeakSelf, Scan, MoveTemp(Device));
            }
        }

        FBlueZArgs Filter;
        Filter.Options = CreateDiscoveryFilter(Prefix);
        FBlueZReply Reply = Bus->Call(AdapterPath, AdapterInterface, TEXT("SetDiscoveryFilter"), Filter, OperationWait());
        if (Reply.Ok())
        {
            Reply = Bus->Call(AdapterPath, AdapterInterface, TEXT("StartDiscovery"), {}, OperationWait());
        }
        if (!Reply.Ok())
        {
            if (Reply.ErrorName != CancelledError) PostError(Describe(Reply));
        }
    }

    void StopScanOnWorker()
    {
        if (InterfacesMatch)
        {
            Bus->Unmatch(InterfacesMatch);
            InterfacesMatch = 0;
        }
        // As upstream, always stop when an adapter is known: a cancelled StartDiscovery may still
        // have started discovery for this client, and BlueZ then rejects the next scan as InProgress.
        if (!AdapterPath.IsEmpty())
        {
            const FBlueZReply Reply = Bus->Call(AdapterPath, AdapterInterface, TEXT("StopDiscovery"), {},
                CleanupWait(FPlatformTime::Seconds() + CleanupSeconds));
            if (!Reply.Ok() && !IsIgnorableStopDiscoveryError(Reply.ErrorName))
            {
                Describe(Reply);
            }
        }
    }

    FString ConnectOnWorker(const FString& DeviceId, const TArray<FString>& RequiredUuids, const TArray<FString>& OptionalUuids)
    {
        StopScanOnWorker();
        DisconnectOnWorker();

        FBlueZReply Objects = Bus->GetManagedObjects(OperationWait());
        if (!Objects.Ok()) return Describe(Objects);
        if (!TryFindDevicePath(Objects.Objects, DeviceId, DevicePath))
        {
            DevicePath.Reset();
            return TEXT("The selected Bluetooth device was not found by BlueZ. Scan again before connecting.");
        }
        if (const FString Error = WaitForPendingDisconnect(); !Error.IsEmpty()) return Error;

        // Pairing is not attempted; the Square Home accepts unpaired connections.
        if (const FString Error = ConnectDeviceWithRetry(); !Error.IsEmpty()) return Error;
        if (const FString Error = WaitForServicesResolved(); !Error.IsEmpty()) return Error;

        Objects = Bus->GetManagedObjects(OperationWait());
        if (!Objects.Ok()) return Describe(Objects);
        if (const FString Error = LoadCharacteristics(Objects.Objects, RequiredUuids, OptionalUuids); !Error.IsEmpty()) return Error;

        // Deviation from upstream: IGattTransport reports link loss, so watch Device1.Connected.
        const uint32 ThisConnection = JobGeneration;
        const TWeakPtr<FLinuxGattTransport> WeakSelf = WeakThis;
        DeviceMatch = Bus->MatchPropertiesChanged(DevicePath, [this, WeakSelf, ThisConnection](const FString& Interface, const FProperties& Changed)
            {
            if (IsLinkLost(Interface, Changed))
            {
                PostGuarded(WeakSelf, ThisConnection, [](FLinuxGattTransport& Self) { if (Self.OnDisconnected) Self.OnDisconnected(); });
                return;
            }
            const TOptional<uint8> Percent = Interface == BatteryInterface ? BatteryPercentage(Changed) : TOptional<uint8>();
            if (Percent.IsSet() && bBatterySubscribed)
            {
                PostNotification(WeakSelf, ThisConnection, BatteryFallbackUuid, {Percent.GetValue()});
            } });
        UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Bluetooth device opened; %d characteristic(s) resolved%s."), Characteristics.Num(),
            BatteryFallbackUuid.IsEmpty() ? TEXT("") : TEXT(", battery from BlueZ Battery1"));
        return FString();
    }

    FString ConnectDeviceWithRetry()
    {
        for (int32 Attempt = 1; Attempt <= ConnectAttempts; ++Attempt)
        {
            const FBlueZReply Reply = Bus->Call(DevicePath, DeviceInterface, TEXT("Connect"), {}, OperationWait());
            if (Reply.Ok() || Reply.ErrorName == TEXT("org.bluez.Error.AlreadyConnected"))
            {
                return FString();
            }
            if (!IsTransientConnectFailure(Reply.ErrorName, Reply.ErrorMessage))
            {
                return DescribeConnect(Reply);
            }
            TryCancelConnect();
            if (Attempt == ConnectAttempts)
            {
                return TEXT("The selected Bluetooth device is not ready yet. Wait a moment and try connecting again.");
            }
            if (const FString Error = RetryDelay(); !Error.IsEmpty()) return Error;
        }
        return FString();
    }

    void TryCancelConnect()
    {
        const FBlueZReply Reply = Bus->Call(DevicePath, DeviceInterface, TEXT("Disconnect"), {}, CleanupWait(FPlatformTime::Seconds() + CleanupSeconds));
        if (!Reply.Ok() && !IsIgnorableDisconnectError(Reply.ErrorName))
        {
            Describe(Reply);
        }
    }

    /** Waits until a device released moments ago reports Connected=false, so Connect starts a new link. */
    FString WaitForPendingDisconnect()
    {
        while (IsDisconnectPending(PendingDisconnectPath, PendingDisconnectUntil, DevicePath, FPlatformTime::Seconds()))
        {
            const FBlueZReply Reply = Bus->GetProperty(DevicePath, DeviceInterface, TEXT("Connected"), OperationWait());
            const bool* bConnected = Reply.Ok() ? Reply.Value.TryGet<bool>() : nullptr;
            if (!bConnected || !*bConnected) break;
            const double Until = FMath::Min(JobDeadline, FPlatformTime::Seconds() + 4 * PollSeconds);
            while (!Cancelled() && FPlatformTime::Seconds() < Until)
            {
                Bus->Pump();
                Bus->WaitForActivity(PollSeconds);
            }
            if (Cancelled()) return CancelledError;
            if (FPlatformTime::Seconds() >= JobDeadline) return TEXT("Bluetooth connection timed out.");
        }
        PendingDisconnectPath.Reset();
        return FString();
    }

    FString WaitForServicesResolved()
    {
        for (int32 Attempt = 1; Attempt <= ServiceResolveAttempts; ++Attempt)
        {
            const FBlueZReply Reply = Bus->GetProperty(DevicePath, DeviceInterface, TEXT("ServicesResolved"), OperationWait());
            if (!Reply.Ok()) return DescribeConnect(Reply);
            if (const bool* bResolved = Reply.Value.TryGet<bool>(); bResolved && *bResolved) return FString();
            if (Attempt < ServiceResolveAttempts)
            {
                if (const FString Error = RetryDelay(); !Error.IsEmpty()) return Error;
            }
        }
        return TEXT("BlueZ did not finish Bluetooth service discovery for the selected device.");
    }

    FString LoadCharacteristics(const FManagedObjects& Objects, const TArray<FString>& RequiredUuids, const TArray<FString>& OptionalUuids)
    {
        // Deviation from upstream: BlueZ's battery plugin can claim 2a19; read Battery1.Percentage then.
        auto Resolve = [&](const FString& Uuid)
        {
            const FString Path = FindCharacteristicPath(Objects, DevicePath, Uuid);
            if (!Path.IsEmpty())
            {
                Characteristics.Add(Uuid, Path);
                return true;
            }
            if (IsBatteryLevel(Uuid) && HasBattery(Objects, DevicePath))
            {
                BatteryFallbackUuid = Uuid;
                return true;
            }
            return false;
        };
        for (const FString& Uuid : RequiredUuids)
        {
            if (!Resolve(Uuid)) return FString::Printf(TEXT("Missing Bluetooth characteristic %s."), *Uuid);
        }
        for (const FString& Uuid : OptionalUuids)
        {
            Resolve(Uuid);
        }
        return FString();
    }

    TArray<uint8> ReadOnWorker(const FString& Uuid)
    {
        // Matching upstream, an unreadable value completes empty instead of failing.
        if (const FString* Path = Characteristics.Find(Uuid))
        {
            FBlueZArgs Args;
            Args.Options = FProperties();
            const FBlueZReply Reply = Bus->Call(*Path, CharacteristicInterface, TEXT("ReadValue"), Args, OperationWait());
            const TArray<uint8>* Bytes = Reply.Ok() ? Reply.Value.TryGet<TArray<uint8>>() : nullptr;
            if (!Reply.Ok()) Describe(Reply);
            return Bytes ? *Bytes : TArray<uint8>();
        }
        if (!BatteryFallbackUuid.IsEmpty() && Uuid.Equals(BatteryFallbackUuid, ESearchCase::IgnoreCase))
        {
            const FBlueZReply Reply = Bus->GetProperty(DevicePath, BatteryInterface, TEXT("Percentage"), OperationWait());
            const int32* Percent = Reply.Ok() ? Reply.Value.TryGet<int32>() : nullptr;
            return Percent ? TArray<uint8>({static_cast<uint8>(FMath::Clamp(*Percent, 0, 100))}) : TArray<uint8>();
        }
        return TArray<uint8>();
    }

    FString SubscribeOnWorker(const FString& Uuid)
    {
        if (Subscriptions.Contains(Uuid))
        {
            return FString();
        }
        if (!BatteryFallbackUuid.IsEmpty() && Uuid.Equals(BatteryFallbackUuid, ESearchCase::IgnoreCase))
        {
            // Battery1.Percentage changes arrive through the device watch; no StartNotify is needed.
            bBatterySubscribed = true;
            Subscriptions.Add(Uuid, 0);
            return FString();
        }
        const FString* Path = Characteristics.Find(Uuid);
        if (!Path)
        {
            return FString::Printf(TEXT("Bluetooth characteristic %s is not available."), *Uuid);
        }

        const TWeakPtr<FLinuxGattTransport> WeakSelf = WeakThis;
        const uint32 ThisConnection = JobGeneration;
        const uint64 Match = Bus->MatchPropertiesChanged(*Path, [WeakSelf, Uuid, ThisConnection](const FString& Interface, const FProperties& Changed)
            {
            if (Interface != CharacteristicInterface) return;
            if (TOptional<TArray<uint8>> Value = NotificationValue(Changed))
            {
                PostNotification(WeakSelf, ThisConnection, Uuid, MoveTemp(Value.GetValue()));
            } });
        if (!Match)
        {
            return TEXT("Bluetooth notification setup failed.");
        }
        const FBlueZReply Reply = Bus->Call(*Path, CharacteristicInterface, TEXT("StartNotify"), {}, OperationWait());
        if (!Reply.Ok())
        {
            Bus->Unmatch(Match);
            return Describe(Reply);
        }
        Subscriptions.Add(Uuid, Match);
        NotificationsStarted.Add(Uuid);
        return FString();
    }

    FString WriteOnWorker(const FString& Uuid, const TArray<uint8>& Value)
    {
        const FString* Path = Characteristics.Find(Uuid);
        if (!Path)
        {
            return FString::Printf(TEXT("Bluetooth characteristic %s is not available."), *Uuid);
        }
        FBlueZArgs Args;
        Args.Bytes = Value;
        Args.Options = CreateWriteOptions();
        const FBlueZReply Reply = Bus->Call(*Path, CharacteristicInterface, TEXT("WriteValue"), Args, OperationWait());
        return Reply.Ok() ? FString() : Describe(Reply);
    }

    // Deviation from upstream: cleanup doesn't wait for the device to answer (as on Windows), so D and
    // quitting never stall; the queued calls are flushed when the bus closes.
    void DisconnectOnWorker()
    {
        if (DeviceMatch)
        {
            Bus->Unmatch(DeviceMatch);
            DeviceMatch = 0;
        }
        for (const TPair<FString, uint64>& Subscription : Subscriptions)
        {
            if (Subscription.Value) Bus->Unmatch(Subscription.Value);
        }
        Subscriptions.Empty();
        for (const FString& Uuid : NotificationsStarted)
        {
            if (const FString* Path = Characteristics.Find(Uuid))
            {
                Bus->Post(*Path, CharacteristicInterface, TEXT("StopNotify"));
            }
        }
        NotificationsStarted.Empty();
        if (!DevicePath.IsEmpty())
        {
            if (Bus->Post(DevicePath, DeviceInterface, TEXT("Disconnect")))
            {
                PendingDisconnectPath = DevicePath;
                PendingDisconnectUntil = FPlatformTime::Seconds() + DisconnectSettleSeconds;
            }
        }
        DevicePath.Reset();
        Characteristics.Empty();
        BatteryFallbackUuid.Reset();
        bBatterySubscribed = false;
    }

    /**
     * Set once by Create, before any job is queued. The worker posts through this instead of
     * AsWeak(), which asserts while the destructor waits for an in-flight job to finish.
     */
    TWeakPtr<FLinuxGattTransport> WeakThis;
    FRunnableThread* Thread = nullptr;
    std::atomic<bool> bStopping = false;
    TQueue<TUniqueFunction<void()>, EQueueMode::Mpsc> Jobs;
    /** Identifies the current connection so stale link-loss events are ignored. */
    std::atomic<uint32> ConnectionSerial = 0;
    std::atomic<uint32> ScanSerial = 0;
    uint32 JobGeneration = 0;
    double JobDeadline = TNumericLimits<double>::Max();
    FString InitError;

    // Worker-thread state.
    TUniquePtr<FBlueZBus> Bus;
    FString AdapterPath;
    uint64 InterfacesMatch = 0;
    FString DevicePath;
    /** The device whose Disconnect was posted last, and until when a reconnect to it waits. */
    FString PendingDisconnectPath;
    double PendingDisconnectUntil = 0;
    uint64 DeviceMatch = 0;
    TMap<FString, FString> Characteristics;
    /** UUID -> PropertiesChanged match id (0 for the Battery1 fallback). */
    TMap<FString, uint64> Subscriptions;
    TSet<FString> NotificationsStarted;
    FString BatteryFallbackUuid;
    bool bBatterySubscribed = false;
};

} // namespace LinuxGatt

TSharedPtr<IGattTransport> CreateLinuxGattTransport()
{
    return LinuxGatt::FLinuxGattTransport::Create();
}

#endif // PLATFORM_LINUX
