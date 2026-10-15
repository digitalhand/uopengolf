// SPDX-License-Identifier: MIT
// Derived from open-lm-connector (MIT) addons/common/bluetooth/windows/WindowsBluetoothGattClient.cs
// at 92eb021abe28d8eb1abe43f87767d3eb73e5939e, ported to C++/WinRT with a link-loss event.
#include "Bluetooth/GattTransport.h"

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
 * in the multithreaded apartment (blocking `.get()` is not allowed on the game
 * thread's STA); results and device events are posted back to the game thread.
 */
class FWindowsGattTransport final : public IGattTransport, public TSharedFromThis<FWindowsGattTransport>, private FRunnable
{
public:
    FWindowsGattTransport()
    {
        WakeEvent = FPlatformProcess::GetSynchEventFromPool(false);
        Thread = FRunnableThread::Create(this, TEXT("OpenLaunchMonitorBluetooth"));
    }

    virtual ~FWindowsGattTransport() override
    {
        if (Thread)
        {
            Thread->Kill(true);
            delete Thread;
        }
        FPlatformProcess::ReturnSynchEventToPool(WakeEvent);
    }

    virtual void StartScan(const FString& NamePrefix) override
    {
        Enqueue([this, NamePrefix]()
            { StartScanOnWorker(NamePrefix); });
    }

    virtual void StopScan() override
    {
        Enqueue([this]()
            { StopScanOnWorker(); });
    }

    virtual void Connect(const FString& DeviceId, const TArray<FString>& RequiredUuids, const TArray<FString>& OptionalUuids,
        FCompletion OnComplete) override
    {
        Enqueue([this, DeviceId, RequiredUuids, OptionalUuids, OnComplete = MoveTemp(OnComplete)]() mutable
            {
            const FString Error = RunGuarded([&]() { return ConnectOnWorker(DeviceId, RequiredUuids, OptionalUuids); });
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
        Enqueue([this]()
            { RunGuarded([this]()
                  { DisconnectOnWorker(); return FString(); }); });
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
                    const WinGatt::GattReadResult Result = Characteristic->ReadValueAsync(WinBluetooth::BluetoothCacheMode::Uncached).get();
                    if (Result.Status() == WinGatt::GattCommunicationStatus::Success)
                    {
                        Value = ReadBuffer(Result.Value());
                    }
                }
                return FString();
            });
            if (!Error.IsEmpty())
            {
                UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("Reading %s failed: %s"), *Uuid, *Error);
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
                    Characteristic->WriteValueWithResultAsync(Writer.DetachBuffer(), WinGatt::GattWriteOption::WriteWithResponse).get();
                return Result.Status() == WinGatt::GattCommunicationStatus::Success
                           ? FString()
                           : FString::Printf(TEXT("Bluetooth write returned status %d."), static_cast<int32>(Result.Status())); })); });
    }

private:
    // FRunnable
    virtual uint32 Run() override
    {
        const FString InitError = RunGuarded([]()
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
        RunGuarded([this]()
            {
            StopScanOnWorker();
            DisconnectOnWorker();
            return FString(); });
        winrt::uninit_apartment();
        return 0;
    }

    virtual void Stop() override
    {
        bStopping = true;
        WakeEvent->Trigger();
    }

    void Enqueue(TUniqueFunction<void()> Job)
    {
        Jobs.Enqueue(MoveTemp(Job));
        WakeEvent->Trigger();
    }

    template <typename FunctorType>
    void PostToGameThread(FunctorType&& Functor)
    {
        AsyncTask(ENamedThreads::GameThread, [WeakSelf = AsWeak(), Functor = Forward<FunctorType>(Functor)]() mutable
            {
            if (const TSharedPtr<FWindowsGattTransport> Self = WeakSelf.Pin())
            {
                Functor(*Self);
            } });
    }

    void Complete(FCompletion OnComplete, const FString& Error)
    {
        PostToGameThread([OnComplete = MoveTemp(OnComplete), Error](FWindowsGattTransport&)
            { OnComplete(Error.IsEmpty(), Error); });
    }

    void StartScanOnWorker(const FString& Prefix)
    {
        StopScanOnWorker();
        const FString Error = RunGuarded([&]()
            {
            const TWeakPtr<FWindowsGattTransport> WeakSelf = AsWeak();
            AdvertisementWatcher = WinAdvertisement::BluetoothLEAdvertisementWatcher();
            AdvertisementWatcher.ScanningMode(WinAdvertisement::BluetoothLEScanningMode::Active);
            AdvertisementToken = AdvertisementWatcher.Received(
                [WeakSelf, Prefix](const WinAdvertisement::BluetoothLEAdvertisementWatcher&, const WinAdvertisement::BluetoothLEAdvertisementReceivedEventArgs& Args)
            {
                const winrt::hstring Name = Args.Advertisement().LocalName();
                if (NameMatches(Name, Prefix))
                {
                    PostDiscovered(WeakSelf, {FString::Printf(TEXT("%llu"), Args.BluetoothAddress()), FString(Name.c_str()).TrimStartAndEnd(),
                                                 static_cast<int32>(Args.RawSignalStrengthInDBm())});
                }
            });
            AdvertisementWatcher.Start();
            return FString(); });
        if (!Error.IsEmpty())
        {
            UE_LOG(LogOpenLaunchMonitor, Error, TEXT("Bluetooth scan could not start: %s"), *Error);
        }
    }

    static void PostDiscovered(const TWeakPtr<FWindowsGattTransport>& WeakSelf, FGattDeviceInfo Device)
    {
        AsyncTask(ENamedThreads::GameThread, [WeakSelf, Device = MoveTemp(Device)]()
            {
            const TSharedPtr<FWindowsGattTransport> Self = WeakSelf.Pin();
            if (Self && Self->OnDeviceDiscovered)
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
        return bIsAddress ? WinBluetooth::BluetoothLEDevice::FromBluetoothAddressAsync(FCString::Strtoui64(*DeviceId, nullptr, 10)).get()
                          : WinBluetooth::BluetoothLEDevice::FromIdAsync(winrt::hstring(*DeviceId)).get();
    }

    FString ConnectOnWorker(const FString& DeviceId, const TArray<FString>& RequiredUuids, const TArray<FString>& OptionalUuids)
    {
        DisconnectOnWorker();

        for (int32 Attempt = 1; Attempt <= OpenAttempts && !Device && !bStopping; ++Attempt)
        {
            Device = OpenDevice(DeviceId);
            if (!Device && Attempt < OpenAttempts)
            {
                FPlatformProcess::Sleep(RetryDelaySeconds);
            }
        }
        if (!Device)
        {
            return TEXT("The selected Bluetooth device is not ready yet.");
        }

        // Pairing is not attempted; the Square Home accepts unpaired connections.
        Session = WinGatt::GattSession::FromDeviceIdAsync(Device.BluetoothDeviceId()).get();
        Session.MaintainConnection(true);

        WinGatt::GattDeviceServicesResult Services = nullptr;
        for (int32 Attempt = 1; Attempt <= ServiceDiscoveryAttempts && !bStopping; ++Attempt)
        {
            const WinBluetooth::BluetoothCacheMode CacheMode =
                Attempt == 1 ? WinBluetooth::BluetoothCacheMode::Cached : WinBluetooth::BluetoothCacheMode::Uncached;
            Services = Device.GetGattServicesAsync(CacheMode).get();
            if (Services.Status() == WinGatt::GattCommunicationStatus::Success)
            {
                break;
            }
            FPlatformProcess::Sleep(RetryDelaySeconds);
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
                    Service.GetCharacteristicsForUuidAsync(ToGuid(Uuid), WinBluetooth::BluetoothCacheMode::Uncached).get();
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

        const uint32 ThisConnection = ++ConnectionSerial;
        const TWeakPtr<FWindowsGattTransport> WeakSelf = AsWeak();
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

        const TWeakPtr<FWindowsGattTransport> WeakSelf = AsWeak();
        const winrt::event_token Token = Characteristic->ValueChanged([WeakSelf, Uuid](const WinGatt::GattCharacteristic&, const WinGatt::GattValueChangedEventArgs& Args)
            {
            TArray<uint8> Value = ReadBuffer(Args.CharacteristicValue());
            AsyncTask(ENamedThreads::GameThread, [WeakSelf, Uuid, Value = MoveTemp(Value)]()
            {
                const TSharedPtr<FWindowsGattTransport> Self = WeakSelf.Pin();
                if (Self && Self->OnNotification)
                {
                    Self->OnNotification(Uuid, Value);
                }
            }); });
        Subscriptions.Add(Uuid, Token);

        const bool bNotify = (Characteristic->CharacteristicProperties() & WinGatt::GattCharacteristicProperties::Notify) ==
                             WinGatt::GattCharacteristicProperties::Notify;
        const WinGatt::GattCommunicationStatus Status =
            Characteristic
                ->WriteClientCharacteristicConfigurationDescriptorAsync(bNotify ? WinGatt::GattClientCharacteristicConfigurationDescriptorValue::Notify
                                                                                : WinGatt::GattClientCharacteristicConfigurationDescriptorValue::Indicate)
                .get();
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
        const bool bLinkUp = Device && Device.ConnectionStatus() == WinBluetooth::BluetoothConnectionStatus::Connected;
        for (const TPair<FString, winrt::event_token>& Subscription : Subscriptions)
        {
            if (const WinGatt::GattCharacteristic* Characteristic = Characteristics.Find(Subscription.Key))
            {
                Characteristic->ValueChanged(Subscription.Value);
                if (bLinkUp)
                {
                    RunGuarded([Characteristic]()
                        {
                        Characteristic
                            ->WriteClientCharacteristicConfigurationDescriptorAsync(WinGatt::GattClientCharacteristicConfigurationDescriptorValue::None)
                            .get();
                        return FString(); });
                }
            }
        }
        Subscriptions.Empty();
        Characteristics.Empty();
        if (Device)
        {
            Device.ConnectionStatusChanged(ConnectionStatusToken);
        }
        if (Session)
        {
            Session.Close();
            Session = nullptr;
        }
        if (Device)
        {
            Device.Close();
            Device = nullptr;
        }
    }

    FRunnableThread* Thread = nullptr;
    FEvent* WakeEvent = nullptr;
    std::atomic<bool> bStopping = false;
    TQueue<TUniqueFunction<void()>, EQueueMode::Mpsc> Jobs;
    /** Identifies the current connection so stale link-loss events are ignored. */
    std::atomic<uint32> ConnectionSerial = 0;

    // Worker-thread state.
    WinAdvertisement::BluetoothLEAdvertisementWatcher AdvertisementWatcher = nullptr;
    winrt::event_token AdvertisementToken;
    WinBluetooth::BluetoothLEDevice Device = nullptr;
    winrt::event_token ConnectionStatusToken;
    WinGatt::GattSession Session = nullptr;
    TMap<FString, WinGatt::GattCharacteristic> Characteristics;
    TMap<FString, winrt::event_token> Subscriptions;
};

TSharedPtr<IGattTransport> CreateWindowsGattTransport()
{
    return MakeShared<FWindowsGattTransport>();
}

#endif // PLATFORM_WINDOWS
