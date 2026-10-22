// SPDX-License-Identifier: MIT
// Derived from open-lm-connector (MIT) addons/common/bluetooth/linux/BlueZMapper.cs at 655fd96
// (the Windows port pinned 92eb021, which is not in the local checkout).
#include "Bluetooth/BlueZModel.h"

namespace BlueZModel
{
    namespace
    {
        const FString* GetString(const FProperties& Properties, const TCHAR* Name)
        {
            const FValue* Value = Properties.Find(Name);
            return Value ? Value->TryGet<FString>() : nullptr;
        }

        const FString* GetNonEmptyString(const FProperties& Properties, const TCHAR* Name)
        {
            const FString* Value = GetString(Properties, Name);
            return Value && !Value->IsEmpty() ? Value : nullptr;
        }

        bool IsOneOf(const FString& ErrorName, std::initializer_list<const TCHAR*> Names)
        {
            for (const TCHAR* Name : Names)
            {
                if (ErrorName.Equals(Name, ESearchCase::CaseSensitive))
                {
                    return true;
                }
            }
            return false;
        }

        FString AddressFromDevicePath(const FString& DevicePath)
        {
            const int32 Index = DevicePath.Find(TEXT("/dev_"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
            return Index == INDEX_NONE ? FString() : DevicePath.Mid(Index + 5);
        }
    } // namespace

    FProperties CreateDiscoveryFilter(const FString& NamePrefix)
    {
        FProperties Filter;
        Filter.Add(TEXT("Transport"), FValue(TInPlaceType<FString>(), TEXT("le")));
        Filter.Add(TEXT("Pattern"), FValue(TInPlaceType<FString>(), NamePrefix));
        Filter.Add(TEXT("DuplicateData"), FValue(TInPlaceType<bool>(), false));
        return Filter;
    }

    FProperties CreateWriteOptions()
    {
        FProperties Options;
        Options.Add(TEXT("type"), FValue(TInPlaceType<FString>(), TEXT("request")));
        return Options;
    }

    bool IsDeviceNameMatch(const FString& Name, const FString& Prefix)
    {
        const FString Trimmed = Name.TrimStartAndEnd();
        return !Trimmed.IsEmpty() && Trimmed.StartsWith(Prefix, ESearchCase::IgnoreCase);
    }

    bool TryCreateDevice(const FString& Path, const FInterfaces& Interfaces, const FString& Prefix, FGattDeviceInfo& Out)
    {
        const FProperties* Properties = Interfaces.Find(DeviceInterface);
        if (!Properties)
        {
            return false;
        }
        const FString* Name = GetNonEmptyString(*Properties, TEXT("Name"));
        if (!Name)
        {
            Name = GetNonEmptyString(*Properties, TEXT("Alias"));
        }
        if (!Name || !IsDeviceNameMatch(*Name, Prefix))
        {
            return false;
        }
        const FValue* Rssi = Properties->Find(TEXT("RSSI"));
        const int32* RssiValue = Rssi ? Rssi->TryGet<int32>() : nullptr;
        Out = {Path, Name->TrimStartAndEnd(), RssiValue ? *RssiValue : 0};
        return true;
    }

    FString FindAdapterPath(const FManagedObjects& Objects)
    {
        for (const TPair<FString, FInterfaces>& Object : Objects)
        {
            if (Object.Value.Contains(AdapterInterface))
            {
                return Object.Key;
            }
        }
        return FString();
    }

    bool TryFindDevicePath(const FManagedObjects& Objects, const FString& DeviceId, FString& OutPath)
    {
        FString Address;
        if (DeviceId.StartsWith(TEXT("/org/bluez/"), ESearchCase::CaseSensitive))
        {
            const FInterfaces* Direct = Objects.Find(DeviceId);
            if (Direct && Direct->Contains(DeviceInterface))
            {
                OutPath = DeviceId;
                return true;
            }
            Address = NormalizeAddress(AddressFromDevicePath(DeviceId));
        }
        else
        {
            Address = NormalizeAddress(DeviceId);
        }
        if (Address.IsEmpty())
        {
            return false;
        }

        const FString PathSuffix = TEXT("/dev_") + Address.Replace(TEXT(":"), TEXT("_"));
        for (const TPair<FString, FInterfaces>& Object : Objects)
        {
            const FProperties* Properties = Object.Value.Find(DeviceInterface);
            if (!Properties)
            {
                continue;
            }
            const FString* DeviceAddress = GetString(*Properties, TEXT("Address"));
            if ((DeviceAddress && NormalizeAddress(*DeviceAddress).Equals(Address, ESearchCase::IgnoreCase)) ||
                Object.Key.EndsWith(PathSuffix, ESearchCase::IgnoreCase))
            {
                OutPath = Object.Key;
                return true;
            }
        }
        return false;
    }

    FString FindCharacteristicPath(const FManagedObjects& Objects, const FString& DevicePath, const FString& Uuid)
    {
        const FString Prefix = DevicePath + TEXT("/");
        const FString Wanted = Uuid.TrimStartAndEnd();
        for (const TPair<FString, FInterfaces>& Object : Objects)
        {
            if (!Object.Key.StartsWith(Prefix, ESearchCase::CaseSensitive))
            {
                continue;
            }
            const FProperties* Properties = Object.Value.Find(CharacteristicInterface);
            const FString* CharacteristicUuid = Properties ? GetString(*Properties, TEXT("UUID")) : nullptr;
            if (CharacteristicUuid && CharacteristicUuid->TrimStartAndEnd().Equals(Wanted, ESearchCase::IgnoreCase))
            {
                return Object.Key;
            }
        }
        return FString();
    }

    bool HasBattery(const FManagedObjects& Objects, const FString& DevicePath)
    {
        const FInterfaces* Interfaces = Objects.Find(DevicePath);
        return Interfaces && Interfaces->Contains(BatteryInterface);
    }

    FString NormalizeAddress(const FString& Value)
    {
        FString Normalized = Value.TrimStartAndEnd().Replace(TEXT("-"), TEXT(":")).Replace(TEXT("_"), TEXT(":"));
        if (Normalized.Len() == 12 && !Normalized.Contains(TEXT(":")))
        {
            FString Separated;
            for (int32 Index = 0; Index < 12; Index += 2)
            {
                Separated += (Index > 0 ? TEXT(":") : TEXT("")) + Normalized.Mid(Index, 2);
            }
            return Separated;
        }
        return Normalized;
    }

    bool IsTransientConnectFailure(const FString& ErrorName, const FString& ErrorMessage)
    {
        if (ErrorName.Equals(TEXT("org.bluez.Error.InProgress"), ESearchCase::CaseSensitive))
        {
            return true;
        }
        return ErrorName.Equals(TEXT("org.bluez.Error.Failed"), ESearchCase::CaseSensitive) &&
               ErrorMessage.Contains(TEXT("le-connection-abort-by-local"), ESearchCase::IgnoreCase);
    }

    bool IsObjectGoneError(const FString& ErrorName)
    {
        return IsOneOf(ErrorName, {TEXT("org.freedesktop.DBus.Error.UnknownObject"), TEXT("org.freedesktop.DBus.Error.UnknownMethod")});
    }

    bool IsIgnorableStopDiscoveryError(const FString& ErrorName)
    {
        return IsOneOf(ErrorName, {TEXT("org.bluez.Error.NotReady"), TEXT("org.bluez.Error.Failed"), TEXT("org.bluez.Error.NotAuthorized")});
    }

    bool IsIgnorableStopNotifyError(const FString& ErrorName)
    {
        return IsOneOf(ErrorName, {TEXT("org.bluez.Error.Failed"), TEXT("org.bluez.Error.NotPermitted"), TEXT("org.bluez.Error.NotSupported")}) ||
               IsObjectGoneError(ErrorName);
    }

    bool IsIgnorableDisconnectError(const FString& ErrorName)
    {
        return IsOneOf(ErrorName, {TEXT("org.bluez.Error.NotConnected"), TEXT("org.bluez.Error.Failed")}) || IsObjectGoneError(ErrorName);
    }

    bool IsLinkLost(const FString& Interface, const FProperties& Changed)
    {
        const FValue* Connected = Interface == DeviceInterface ? Changed.Find(TEXT("Connected")) : nullptr;
        const bool* bConnected = Connected ? Connected->TryGet<bool>() : nullptr;
        return bConnected && !*bConnected;
    }

    TOptional<TArray<uint8>> NotificationValue(const FProperties& Changed)
    {
        const FValue* Value = Changed.Find(TEXT("Value"));
        const TArray<uint8>* Bytes = Value ? Value->TryGet<TArray<uint8>>() : nullptr;
        return Bytes && Bytes->Num() > 0 ? TOptional<TArray<uint8>>(*Bytes) : TOptional<TArray<uint8>>();
    }

    TOptional<uint8> BatteryPercentage(const FProperties& Changed)
    {
        const FValue* Value = Changed.Find(TEXT("Percentage"));
        const int32* Percent = Value ? Value->TryGet<int32>() : nullptr;
        return Percent ? TOptional<uint8>(static_cast<uint8>(FMath::Clamp(*Percent, 0, 100))) : TOptional<uint8>();
    }

    FString FriendlyError(const FString& ErrorName, const FString& Message)
    {
        if (ErrorName == TEXT("org.bluez.Error.NotReady"))
        {
            return TEXT("Bluetooth is turned off or blocked. Turn it on and try again.");
        }
        if (ErrorName == TimeoutError)
        {
            return TEXT("The Bluetooth operation timed out.");
        }
        if (IsObjectGoneError(ErrorName))
        {
            return TEXT("The selected Bluetooth device is no longer known to BlueZ. Scan again before connecting.");
        }
        if (ErrorName == TEXT("org.freedesktop.DBus.Error.AccessDenied"))
        {
            return TEXT("Bluetooth access was denied. Check that your user may use BlueZ (for example the bluetooth group).");
        }
        if (ErrorName == TEXT("org.freedesktop.DBus.Error.ServiceUnknown") || ErrorName == TEXT("org.freedesktop.DBus.Error.NameHasNoOwner"))
        {
            return TEXT("The Bluetooth service isn't running. Start it with: sudo systemctl start bluetooth");
        }
        if (ErrorName == TEXT("org.freedesktop.systemd1.NoSuchUnit") || ErrorName == TEXT("org.freedesktop.systemd1.UnitMasked"))
        {
            return TEXT("The Bluetooth service is disabled. Enable it with: sudo systemctl enable --now bluetooth");
        }
        return Message.IsEmpty() ? ErrorName : Message;
    }

    FString FriendlyConnectError(const FString& ErrorName, const FString& Message)
    {
        if (ErrorName == TimeoutError)
        {
            return TEXT("The Square didn't answer. If another computer or app is connected to it, disconnect it there and press C again.");
        }
        return FriendlyError(ErrorName, Message);
    }

    bool IsExpectedOutcome(const FString& ErrorName)
    {
        return ErrorName == CancelledError;
    }

    bool IsDisconnectPending(const FString& PendingPath, double PendingUntil, const FString& Path, double Now)
    {
        return !PendingPath.IsEmpty() && PendingPath == Path && Now < PendingUntil;
    }
}
