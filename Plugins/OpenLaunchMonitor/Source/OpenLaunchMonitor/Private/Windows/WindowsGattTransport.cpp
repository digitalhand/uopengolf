// SPDX-License-Identifier: MIT
// Derived from open-lm-connector (MIT) addons/common/bluetooth/windows/WindowsBluetoothGattClient.cs
// at 92eb021abe28d8eb1abe43f87767d3eb73e5939e, ported to C++/WinRT with a link-loss event.
#include "Bluetooth/GattTransport.h"
#include "Bluetooth/GattOperationWait.h"
#include "HAL/PlatformTime.h"

#if PLATFORM_WINDOWS

#include "Algo/AllOf.h"
#include "Async/Async.h"
#include "Containers/Queue.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "OpenLaunchMonitorLog.h"

#include <atomic>
#include <string>

#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
    namespace WinBluetooth = winrt::Windows::Devices::Bluetooth;
    namespace WinAdvertisement = winrt::Windows::Devices::Bluetooth::Advertisement;
    namespace WinGatt = winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
    namespace WinStreams = winrt::Windows::Storage::Streams;

    constexpr int32 OpenAttempts = 12;
    constexpr int32 ServiceDiscoveryAttempts = 12;
    constexpr float RetryDelaySeconds = 1.0f;

    winrt::guid ToGuid(const FString& Uuid)
    {
        return winrt::guid(std::string_view(TCHAR_TO_ANSI(*Uuid)));
    }

    bool NameMatches(const winrt::hstring& Name, const FString& Prefix)
    {
        const FString Trimmed = FString(Name.c_str()).TrimStartAndEnd();
        return !Trimmed.IsEmpty() && Trimmed.StartsWith(Prefix, ESearchCase::IgnoreCase);
    }

    TArray<uint8> ReadBuffer(const WinStreams::IBuffer& Buffer)
    {
        TArray<uint8> Bytes;
        const WinStreams::DataReader Reader = WinStreams::DataReader::FromBuffer(Buffer);
        Bytes.SetNumUninitialized(static_cast<int32>(Reader.UnconsumedBufferLength()));
        Reader.ReadBytes(winrt::array_view<uint8_t>(Bytes.GetData(), Bytes.GetData() + Bytes.Num()));
        return Bytes;
    }

    /** Runs a worker job, converting WinRT and standard exceptions into an error message. */
    FString RunGuarded(TFunctionRef<FString()> Body)
    {
        try
        {
            return Body();
        }
        catch (const winrt::hresult_error& Error)
        {
            const FString Message = FString(Error.message().c_str()).TrimStartAndEnd();
            return FString::Printf(TEXT("%s (0x%08X)"), Message.IsEmpty() ? TEXT("Bluetooth error") : *Message,
                static_cast<uint32>(Error.code().value));
        }
        catch (const std::exception& Error)
        {
            return UTF8_TO_TCHAR(Error.what());
        }
    }
} // namespace

/**
 * Bluetooth LE GATT client over WinRT. Every WinRT call runs on one worker thread
 * in the multithreaded apartment with cancellable, bounded waits; results and
 * device events are posted back to the game thread with connection-generation guards.
 */
class FWindowsGattTransport final : public IGattTransport, private FRunnable
{
public:
    static TSharedRef<FWindowsGattTransport> Create()
    {
        const TSharedRef<FWindowsGattTransport> Transport = MakeShared<FWindowsGattTransport>();
        Transport->WeakThis = Transport;
        return Transport;
    }

    FWindowsGattTransport()
    {
        WakeEvent = FPlatformProcess::GetSynchEventFromPool(false);
        Thread = FRunnableThread::Create(this, TEXT("OpenLaunchMonitorBluetooth"));
    }

    virtual ~FWindowsGattTransport() override
    {
        if (Thread)
        {
            Stop();
            Thread->WaitForCompletion();
            delete Thread;
        }
        FPlatformProcess::ReturnSynchEventToPool(WakeEvent);
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
        Enqueue([this]()
            { StopScanOnWorker(); }, true);
    }

    virtual void Connect(const FString& DeviceId, const TArray<FString>& RequiredUuids, const TArray<FString>& OptionalUuids,
        FCompletion OnComplete) override
    {
        ++ConnectionSerial;
        Enqueue([this, DeviceId, RequiredUuids, OptionalUuids, OnComplete = MoveTemp(OnComplete)]() mutable
            {
            JobDeadline = FPlatformTime::Seconds() + 30.0;
            const FString Error = RunGuarded([&]() { return ConnectOnWorker(DeviceId, RequiredUuids, OptionalUuids); });
            JobDeadline = TNumericLimits<double>::Max();
            if (!Error.IsEmpty())
            {
                RunGuarded([this]() { DisconnectOnWorker(); return FString(); });
            }
            Complete(MoveTemp(OnComplete), Error); });
    }

    virtual void Disconnect() override
    {
        // Invalidate pending link-loss events now; the worker releases the device.
        ++ConnectionSerial;
        ++ScanSerial;
        WakeEvent->Trigger();
        Enqueue([this]()
            { RunGuarded([this]()
                  { StopScanOnWorker(); DisconnectOnWorker(); return FString(); }); }, true);
    }

    virtual void Read(const FString& Uuid, TFunction<void(const TArray<uint8>&)> OnComplete) override
    {
        Enqueue([this, Uuid, OnComplete = MoveTemp(OnComplete)]() mutable
            {
            TArray<uint8> Value;
            const FString Error = RunGuarded([&]()
            {
                if (const WinGatt::GattCharacteristic* Characteristic = Characteristics.Find(Uuid))
                {
                    const WinGatt::GattReadResult Result = Await(Characteristic->ReadValueAsync(WinBluetooth::BluetoothCacheMode::Uncached));
                    if (Result.Status() == WinGatt::GattCommunicationStatus::Success)
                    {
                        Value = ReadBuffer(Result.Value());
                    }
                }
                return FString();
            });
            // As on Linux, an unreadable value completes empty instead of failing the connection.
            if (!Error.IsEmpty() && !Cancelled())
            {
                UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("Bluetooth read of %s failed: %s"), *Uuid, *Error);
            }
            PostToGameThread([OnComplete = MoveTemp(OnComplete), Value = MoveTemp(Value)](FWindowsGattTransport&) { OnComplete(Value); }); });
    }

    virtual void Subscribe(const FString& Uuid, FCompletion OnComplete) override
    {
        Enqueue([this, Uuid, OnComplete = MoveTemp(OnComplete)]() mutable
            { Complete(MoveTemp(OnComplete), RunGuarded([&]()
                                                 { return SubscribeOnWorker(Uuid); })); });
    }

    virtual void Write(const FString& Uuid, const TArray<uint8>& Value, FCompletion OnComplete) override
    {
        Enqueue([this, Uuid, Value, OnComplete = MoveTemp(OnComplete)]() mutable
            { Complete(MoveTemp(OnComplete), RunGuarded([&]()
                                                 {
                const WinGatt::GattCharacteristic* Characteristic = Characteristics.Find(Uuid);
                if (!Characteristic)
                {
                    return FString::Printf(TEXT("Bluetooth characteristic %s is not available."), *Uuid);
                }
                const WinStreams::DataWriter Writer;
                Writer.WriteBytes(winrt::array_view<const uint8_t>(Value.GetData(), Value.GetData() + Value.Num()));
                const WinGatt::GattWriteResult Result =
                    Await(Characteristic->WriteValueWithResultAsync(Writer.DetachBuffer(), WinGatt::GattWriteOption::WriteWithResponse));
                return Result.Status() == WinGatt::GattCommunicationStatus::Success
                           ? FString()
                           : FString::Printf(TEXT("Bluetooth write returned status %d."), static_cast<int32>(Result.Status())); })); });
    }

private:
    // FRunnable
    virtual uint32 Run() override
    {
        InitError = RunGuarded([]()
            {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            return FString(); });
        if (!InitError.IsEmpty())
        {
            UE_LOG(LogOpenLaunchMonitor, Error, TEXT("Bluetooth worker could not initialize WinRT: %s"), *InitError);
        }
        while (!bStopping)
        {
            WakeEvent->Wait();
            TUniqueFunction<void()> Job;
            while (!bStopping && Jobs.Dequeue(Job))
            {
                Job();
            }
        }
        if (InitError.IsEmpty())
        {
            RunGuarded([this]()
                { StopScanOnWorker(); DisconnectOnWorker(); return FString(); });
            winrt::uninit_apartment();
        }
        return 0;
    }

    virtual void Stop() override
    {
        bStopping = true;
        ++ConnectionSerial;
        ++ScanSerial;
        WakeEvent->Trigger();
    }

    bool Cancelled() const { return bStopping || ConnectionSerial != JobGeneration; }

    template <typename OperationType>
    auto Await(const OperationType& Operation)
    {
        const double Deadline = FMath::Min(JobDeadline, FPlatformTime::Seconds() + 10.0);
        const auto Result = GattOperationWait::Wait(
            [&] { return Operation.Status() != winrt::Windows::Foundation::AsyncStatus::Started; },
            [this] { return Cancelled(); }, [] { return FPlatformTime::Seconds(); },
            [this](double Seconds) { WakeEvent->Wait(static_cast<uint32>(FMath::Max(1.0, Seconds * 1000.0))); }, Deadline);
        if (Result != GattOperationWait::EResult::Completed)
        {
            RunGuarded([&] { Operation.Cancel(); return FString(); });
            if (Result == GattOperationWait::EResult::Cancelled) throw winrt::hresult_canceled();
            throw winrt::hresult_error(winrt::hresult(static_cast<int32>(0x800705B4)), L"Bluetooth operation timed out");
        }
        return Operation.GetResults();
    }

    void RetryDelay()
    {
        const double Until = FMath::Min(JobDeadline, FPlatformTime::Seconds() + RetryDelaySeconds);
        while (!Cancelled() && FPlatformTime::Seconds() < Until) WakeEvent->Wait(25);
        if (Cancelled()) throw winrt::hresult_canceled();
        if (FPlatformTime::Seconds() >= JobDeadline)
            throw winrt::hresult_error(winrt::hresult(static_cast<int32>(0x800705B4)), L"Bluetooth connection timed out");
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
        WakeEvent->Trigger();
    }

    template <typename FunctorType>
    void PostToGameThread(FunctorType&& Functor)
    {
        AsyncTask(ENamedThreads::GameThread, [WeakSelf = WeakThis, Generation = JobGeneration, Functor = Forward<FunctorType>(Functor)]() mutable
        {
            if (const auto Self = WeakSelf.Pin(); Self && !Self->bStopping && Self->ConnectionSerial == Generation)
                Functor(*Self);
        });
    }

    void PostError(const FString& Error)
    {
        PostToGameThread([Error](FWindowsGattTransport& Self) { if (Self.OnError) Self.OnError(Error); });
    }

    void Complete(FCompletion OnComplete, const FString& Error)
    {
        PostToGameThread([OnComplete = MoveTemp(OnComplete), Error](FWindowsGattTransport&)
            { OnComplete(Error.IsEmpty(), Error); });
    }

    void StartScanOnWorker(const FString& Prefix, uint32 Scan)
    {
        StopScanOnWorker();
        const FString Error = RunGuarded([&]()
            {
            const TWeakPtr<FWindowsGattTransport> WeakSelf = WeakThis;
            AdvertisementWatcher = WinAdvertisement::BluetoothLEAdvertisementWatcher();
            AdvertisementWatcher.ScanningMode(WinAdvertisement::BluetoothLEScanningMode::Active);
            AdvertisementToken = AdvertisementWatcher.Received(
                [WeakSelf, Prefix, Scan](const WinAdvertisement::BluetoothLEAdvertisementWatcher&, const WinAdvertisement::BluetoothLEAdvertisementReceivedEventArgs& Args)
            {
                const winrt::hstring Name = Args.Advertisement().LocalName();
                if (NameMatches(Name, Prefix))
                {
                    PostDiscovered(WeakSelf, Scan, {FString::Printf(TEXT("%llu"), Args.BluetoothAddress()), FString(Name.c_str()).TrimStartAndEnd(),
                                                 static_cast<int32>(Args.RawSignalStrengthInDBm())});
                }
            });
            AdvertisementWatcher.Start();
            return FString(); });
        if (!Error.IsEmpty())
        {
            PostError(Error);
        }
    }

    static void PostDiscovered(const TWeakPtr<FWindowsGattTransport>& WeakSelf, uint32 Scan, FGattDeviceInfo Device)
    {
        AsyncTask(ENamedThreads::GameThread, [WeakSelf, Scan, Device = MoveTemp(Device)]()
            {
            const TSharedPtr<FWindowsGattTransport> Self = WeakSelf.Pin();
            if (Self && !Self->bStopping && Self->ScanSerial == Scan && Self->OnDeviceDiscovered)
            {
                Self->OnDeviceDiscovered(Device);
            } });
    }

    void StopScanOnWorker()
    {
        RunGuarded([this]()
            {
            if (AdvertisementWatcher)
            {
                AdvertisementWatcher.Received(AdvertisementToken);
                if (AdvertisementWatcher.Status() == WinAdvertisement::BluetoothLEAdvertisementWatcherStatus::Started)
                {
                    AdvertisementWatcher.Stop();
                }
                AdvertisementWatcher = nullptr;
            }
            return FString(); });
    }

    WinBluetooth::BluetoothLEDevice OpenDevice(const FString& DeviceId)
    {
        const bool bIsAddress = !DeviceId.IsEmpty() && Algo::AllOf(DeviceId, [](TCHAR Character)
                                                           { return FChar::IsDigit(Character); });
        return bIsAddress ? Await(WinBluetooth::BluetoothLEDevice::FromBluetoothAddressAsync(FCString::Strtoui64(*DeviceId, nullptr, 10)))
                          : Await(WinBluetooth::BluetoothLEDevice::FromIdAsync(winrt::hstring(*DeviceId)));
    }

    FString ConnectOnWorker(const FString& DeviceId, const TArray<FString>& RequiredUuids, const TArray<FString>& OptionalUuids)
    {
        StopScanOnWorker();
        DisconnectOnWorker();

        for (int32 Attempt = 1; Attempt <= OpenAttempts && !Device && !Cancelled(); ++Attempt)
        {
            Device = OpenDevice(DeviceId);
            if (!Device && Attempt < OpenAttempts)
            {
                RetryDelay();
            }
        }
        if (!Device)
        {
            return TEXT("The selected Bluetooth device is not ready yet.");
        }

        // Pairing is not attempted; the Square Home accepts unpaired connections.
        Session = Await(WinGatt::GattSession::FromDeviceIdAsync(Device.BluetoothDeviceId()));
        Session.MaintainConnection(true);

        WinGatt::GattDeviceServicesResult Services = nullptr;
        for (int32 Attempt = 1; Attempt <= ServiceDiscoveryAttempts && !Cancelled(); ++Attempt)
        {
            const WinBluetooth::BluetoothCacheMode CacheMode =
                Attempt == 1 ? WinBluetooth::BluetoothCacheMode::Cached : WinBluetooth::BluetoothCacheMode::Uncached;
            Services = Await(Device.GetGattServicesAsync(CacheMode));
            if (Services.Status() == WinGatt::GattCommunicationStatus::Success)
            {
                break;
            }
            if (Attempt < ServiceDiscoveryAttempts)
            {
                RetryDelay();
            }
        }
        if (!Services || Services.Status() != WinGatt::GattCommunicationStatus::Success)
        {
            return FString::Printf(TEXT("Bluetooth service discovery returned status %d."), Services ? static_cast<int32>(Services.Status()) : -1);
        }

        auto Resolve = [&](const FString& Uuid) -> bool
        {
            for (const WinGatt::GattDeviceService& Service : Services.Services())
            {
                const WinGatt::GattCharacteristicsResult Result =
                    Await(Service.GetCharacteristicsForUuidAsync(ToGuid(Uuid), WinBluetooth::BluetoothCacheMode::Uncached));
                if (Result.Status() == WinGatt::GattCommunicationStatus::Success && Result.Characteristics().Size() > 0)
                {
                    Characteristics.Add(Uuid, Result.Characteristics().GetAt(0));
                    return true;
                }
            }
            return false;
        };
        for (const FString& Uuid : RequiredUuids)
        {
            if (!Resolve(Uuid))
            {
                return FString::Printf(TEXT("Missing Bluetooth characteristic %s."), *Uuid);
            }
        }
        for (const FString& Uuid : OptionalUuids)
        {
            Resolve(Uuid);
        }

        const uint32 ThisConnection = JobGeneration;
        const TWeakPtr<FWindowsGattTransport> WeakSelf = WeakThis;
        ConnectionStatusToken = Device.ConnectionStatusChanged([WeakSelf, ThisConnection](const WinBluetooth::BluetoothLEDevice& Sender, const auto&)
            {
            if (Sender.ConnectionStatus() != WinBluetooth::BluetoothConnectionStatus::Disconnected)
            {
                return;
            }
            AsyncTask(ENamedThreads::GameThread, [WeakSelf, ThisConnection]()
            {
                const TSharedPtr<FWindowsGattTransport> Self = WeakSelf.Pin();
                if (Self && Self->ConnectionSerial == ThisConnection && Self->OnDisconnected)
                {
                    Self->OnDisconnected();
                }
            }); });
        UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Bluetooth device opened; %d characteristic(s) resolved."), Characteristics.Num());
        return FString();
    }

    FString SubscribeOnWorker(const FString& Uuid)
    {
        if (Subscriptions.Contains(Uuid))
        {
            return FString();
        }
        const WinGatt::GattCharacteristic* Characteristic = Characteristics.Find(Uuid);
        if (!Characteristic)
        {
            return FString::Printf(TEXT("Bluetooth characteristic %s is not available."), *Uuid);
        }

        const TWeakPtr<FWindowsGattTransport> WeakSelf = WeakThis;
        const uint32 ThisConnection = JobGeneration;
        const winrt::event_token Token = Characteristic->ValueChanged([WeakSelf, Uuid, ThisConnection](const WinGatt::GattCharacteristic&, const WinGatt::GattValueChangedEventArgs& Args)
            {
            TArray<uint8> Value = ReadBuffer(Args.CharacteristicValue());
            AsyncTask(ENamedThreads::GameThread, [WeakSelf, Uuid, ThisConnection, Value = MoveTemp(Value)]()
            {
                const TSharedPtr<FWindowsGattTransport> Self = WeakSelf.Pin();
                if (Self && !Self->bStopping && Self->ConnectionSerial == ThisConnection && Self->OnNotification)
                {
                    Self->OnNotification(Uuid, Value);
                }
            }); });
        Subscriptions.Add(Uuid, Token);

        const bool bNotify = (Characteristic->CharacteristicProperties() & WinGatt::GattCharacteristicProperties::Notify) ==
                             WinGatt::GattCharacteristicProperties::Notify;
        const WinGatt::GattCommunicationStatus Status =
            Await(Characteristic->WriteClientCharacteristicConfigurationDescriptorAsync(
                bNotify ? WinGatt::GattClientCharacteristicConfigurationDescriptorValue::Notify
                        : WinGatt::GattClientCharacteristicConfigurationDescriptorValue::Indicate));
        if (Status != WinGatt::GattCommunicationStatus::Success)
        {
            Characteristic->ValueChanged(Token);
            Subscriptions.Remove(Uuid);
            return FString::Printf(TEXT("Bluetooth notification setup returned status %d."), static_cast<int32>(Status));
        }
        return FString();
    }

    void DisconnectOnWorker()
    {
        for (const TPair<FString, winrt::event_token>& Subscription : Subscriptions)
        {
            if (const WinGatt::GattCharacteristic* Characteristic = Characteristics.Find(Subscription.Key))
            {
                RunGuarded([&] { Characteristic->ValueChanged(Subscription.Value); return FString(); });
            }
        }
        Subscriptions.Empty();
        Characteristics.Empty();
        if (Device && ConnectionStatusToken.value != 0)
        {
            RunGuarded([&] { Device.ConnectionStatusChanged(ConnectionStatusToken); return FString(); });
            ConnectionStatusToken = {};
        }
        if (Session)
        {
            RunGuarded([&] { Session.Close(); return FString(); });
            Session = nullptr;
        }
        if (Device)
        {
            RunGuarded([&] { Device.Close(); return FString(); });
            Device = nullptr;
        }
    }

    /**
     * Set once by Create, before any job is queued. The worker posts through this instead of
     * AsWeak(), which asserts while the destructor waits for an in-flight job to finish.
     */
    TWeakPtr<FWindowsGattTransport> WeakThis;
    FRunnableThread* Thread = nullptr;
    FEvent* WakeEvent = nullptr;
    std::atomic<bool> bStopping = false;
    TQueue<TUniqueFunction<void()>, EQueueMode::Mpsc> Jobs;
    /** Identifies the current connection so stale link-loss events are ignored. */
    std::atomic<uint32> ConnectionSerial = 0;
    std::atomic<uint32> ScanSerial = 0;
    uint32 JobGeneration = 0;
    double JobDeadline = TNumericLimits<double>::Max();
    FString InitError;

    // Worker-thread state.
    WinAdvertisement::BluetoothLEAdvertisementWatcher AdvertisementWatcher = nullptr;
    winrt::event_token AdvertisementToken;
    WinBluetooth::BluetoothLEDevice Device = nullptr;
    winrt::event_token ConnectionStatusToken{};
    WinGatt::GattSession Session = nullptr;
    TMap<FString, WinGatt::GattCharacteristic> Characteristics;
    TMap<FString, winrt::event_token> Subscriptions;
};

TSharedPtr<IGattTransport> CreateWindowsGattTransport()
{
    return FWindowsGattTransport::Create();
}

#endif // PLATFORM_WINDOWS
