// SPDX-License-Identifier: MIT
#include "Linux/BlueZBus.h"

#if PLATFORM_LINUX

#include "Bluetooth/GattOperationWait.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "OpenLaunchMonitorLog.h"

#include <cerrno>
#include <cstring>

namespace BlueZBusCodec
{
    using namespace BlueZModel;

    constexpr const char* BlueZService = "org.bluez";
    constexpr const TCHAR* BusError = TEXT("org.uopengolf.Error.Bus");
    constexpr double OperationSeconds = 10.0;

    /** One in-flight method call; sd-bus fills it from OnReply during Pump. */
    struct FPending
    {
        const FSdBusApi* Api = nullptr;
        bool bDone = false;
        sd_bus_message* Reply = nullptr;
        FString ErrorName;
        FString ErrorMessage;
    };

    int OnReply(sd_bus_message* Message, void* UserData, sd_bus_error*)
    {
        FPending& Pending = *static_cast<FPending*>(UserData);
        Pending.bDone = true;
        const sd_bus_error* Error = Pending.Api->message_get_error(Message);
        if (Error && Error->name)
        {
            Pending.ErrorName = UTF8_TO_TCHAR(Error->name);
            Pending.ErrorMessage = Error->message ? FString(UTF8_TO_TCHAR(Error->message)) : FString();
        }
        else
        {
            Pending.Reply = Pending.Api->message_ref(Message);
        }
        return 0;
    }

    /** Reads one complete value; unsupported types are skipped and return false. */
    bool ReadValue(const FSdBusApi& Api, sd_bus_message* Message, FValue& Out)
    {
        char Type = 0;
        const char* Contents = nullptr;
        if (Api.message_peek_type(Message, &Type, &Contents) <= 0)
        {
            return false;
        }
        auto ReadInt = [&](auto Value) -> bool
        {
            if (Api.message_read_basic(Message, Type, &Value) < 0) return false;
            Out.Set<int32>(static_cast<int32>(Value));
            return true;
        };
        switch (Type)
        {
            case 'b':
            {
                int Value = 0;
                if (Api.message_read_basic(Message, 'b', &Value) < 0) return false;
                Out.Set<bool>(Value != 0);
                return true;
            }
            case 'y': return ReadInt(uint8_t(0));
            case 'n': return ReadInt(int16_t(0));
            case 'q': return ReadInt(uint16_t(0));
            case 'i': return ReadInt(int32_t(0));
            case 'u': return ReadInt(uint32_t(0));
            case 's':
            case 'o':
            case 'g':
            {
                const char* Value = nullptr;
                if (Api.message_read_basic(Message, Type, &Value) < 0) return false;
                Out.Set<FString>(Value ? FString(UTF8_TO_TCHAR(Value)) : FString());
                return true;
            }
            case 'a':
                if (Contents && FCStringAnsi::Strcmp(Contents, "y") == 0)
                {
                    const void* Data = nullptr;
                    size_t Size = 0;
                    if (Api.message_read_array(Message, 'y', &Data, &Size) < 0) return false;
                    Out.Set<TArray<uint8>>(TArray<uint8>(static_cast<const uint8*>(Data), static_cast<int32>(Size)));
                    return true;
                }
                break;
            case 'v':
            {
                if (Api.message_enter_container(Message, 'v', Contents) < 0) return false;
                const bool bRead = ReadValue(Api, Message, Out);
                Api.message_exit_container(Message);
                return bRead;
            }
            default:
                break;
        }
        Api.message_skip(Message, nullptr);
        return false;
    }

    bool ReadProperties(const FSdBusApi& Api, sd_bus_message* Message, FProperties& Out)
    {
        if (Api.message_enter_container(Message, 'a', "{sv}") <= 0)
        {
            return false;
        }
        while (Api.message_enter_container(Message, 'e', "sv") > 0)
        {
            const char* Key = nullptr;
            FValue Value;
            if (Api.message_read_basic(Message, 's', &Key) >= 0 && ReadValue(Api, Message, Value) && Key)
            {
                Out.Add(UTF8_TO_TCHAR(Key), MoveTemp(Value));
            }
            Api.message_exit_container(Message);
        }
        Api.message_exit_container(Message);
        return true;
    }

    bool ReadInterfaces(const FSdBusApi& Api, sd_bus_message* Message, FInterfaces& Out)
    {
        if (Api.message_enter_container(Message, 'a', "{sa{sv}}") <= 0)
        {
            return false;
        }
        while (Api.message_enter_container(Message, 'e', "sa{sv}") > 0)
        {
            const char* Interface = nullptr;
            FProperties Properties;
            if (Api.message_read_basic(Message, 's', &Interface) >= 0 && Interface && ReadProperties(Api, Message, Properties))
            {
                Out.Add(UTF8_TO_TCHAR(Interface), MoveTemp(Properties));
            }
            Api.message_exit_container(Message);
        }
        Api.message_exit_container(Message);
        return true;
    }

    void ReadManagedObjects(const FSdBusApi& Api, sd_bus_message* Message, FManagedObjects& Out)
    {
        if (Api.message_enter_container(Message, 'a', "{oa{sa{sv}}}") <= 0)
        {
            return;
        }
        while (Api.message_enter_container(Message, 'e', "oa{sa{sv}}") > 0)
        {
            const char* Path = nullptr;
            FInterfaces Interfaces;
            if (Api.message_read_basic(Message, 'o', &Path) >= 0 && Path && ReadInterfaces(Api, Message, Interfaces))
            {
                Out.Add(UTF8_TO_TCHAR(Path), MoveTemp(Interfaces));
            }
            Api.message_exit_container(Message);
        }
        Api.message_exit_container(Message);
    }

    bool AppendVariant(const FSdBusApi& Api, sd_bus_message* Message, const FValue& Value)
    {
        if (const bool* Bool = Value.TryGet<bool>())
        {
            const int Int = *Bool ? 1 : 0;
            return Api.message_open_container(Message, 'v', "b") >= 0 && Api.message_append_basic(Message, 'b', &Int) >= 0 &&
                   Api.message_close_container(Message) >= 0;
        }
        if (const int32* Int = Value.TryGet<int32>())
        {
            return Api.message_open_container(Message, 'v', "i") >= 0 && Api.message_append_basic(Message, 'i', Int) >= 0 &&
                   Api.message_close_container(Message) >= 0;
        }
        if (const FString* String = Value.TryGet<FString>())
        {
            const FTCHARToUTF8 Utf8(**String);
            return Api.message_open_container(Message, 'v', "s") >= 0 && Api.message_append_basic(Message, 's', Utf8.Get()) >= 0 &&
                   Api.message_close_container(Message) >= 0;
        }
        const TArray<uint8>& Bytes = Value.Get<TArray<uint8>>();
        return Api.message_open_container(Message, 'v', "ay") >= 0 &&
               Api.message_append_array(Message, 'y', Bytes.GetData(), Bytes.Num()) >= 0 && Api.message_close_container(Message) >= 0;
    }

    bool AppendArgs(const FSdBusApi& Api, sd_bus_message* Message, const FBlueZArgs& Args)
    {
        for (const FString& String : Args.Strings)
        {
            const FTCHARToUTF8 Utf8(*String);
            if (Api.message_append_basic(Message, 's', Utf8.Get()) < 0) return false;
        }
        if (Args.Bytes.IsSet() && Api.message_append_array(Message, 'y', Args.Bytes->GetData(), Args.Bytes->Num()) < 0)
        {
            return false;
        }
        if (Args.Options.IsSet())
        {
            if (Api.message_open_container(Message, 'a', "{sv}") < 0) return false;
            for (const TPair<FString, FValue>& Option : Args.Options.GetValue())
            {
                const FTCHARToUTF8 Key(*Option.Key);
                if (Api.message_open_container(Message, 'e', "sv") < 0 || Api.message_append_basic(Message, 's', Key.Get()) < 0 ||
                    !AppendVariant(Api, Message, Option.Value) || Api.message_close_container(Message) < 0)
                {
                    return false;
                }
            }
            if (Api.message_close_container(Message) < 0) return false;
        }
        return true;
    }

    FBlueZReply BusFailure(const TCHAR* What, int Result)
    {
        FBlueZReply Reply;
        Reply.ErrorName = BusError;
        Reply.ErrorMessage = FString::Printf(TEXT("D-Bus %s failed (error %d)."), What, -Result);
        return Reply;
    }
} // namespace BlueZBusCodec

struct FBlueZBus::FMatch
{
    FBlueZBus* Owner = nullptr;
    sd_bus_slot* Slot = nullptr;
    FPropertiesHandler OnProperties;
    FInterfacesAddedHandler OnInterfacesAdded;
};

TUniquePtr<FBlueZBus> FBlueZBus::Open(FString& OutError)
{
    const TSharedPtr<const FSdBusApi> Api = FSdBusApi::Load(TEXT("libsystemd.so.0"), OutError);
    if (!Api)
    {
        return nullptr;
    }
    sd_bus* Bus = nullptr;
    const int Result = Api->open_system(&Bus);
    if (Result < 0)
    {
        OutError = FString::Printf(TEXT("Cannot connect to the system D-Bus for Bluetooth (error %d)."), -Result);
        return nullptr;
    }
    return TUniquePtr<FBlueZBus>(new FBlueZBus(Api.ToSharedRef(), Bus));
}

FBlueZBus::FBlueZBus(TSharedRef<const FSdBusApi> InApi, sd_bus* InBus)
    : Api(MoveTemp(InApi)), Bus(InBus)
{
}

FBlueZBus::~FBlueZBus()
{
    for (TPair<uint64, TUniquePtr<FMatch>>& Match : Matches)
    {
        Api->slot_unref(Match.Value->Slot);
    }
    Matches.Empty();
    Api->flush_close_unref(Bus);
}

FBlueZReply FBlueZBus::GetManagedObjects(const FBlueZWait& Wait)
{
    return Send(TEXT("/"), TEXT("org.freedesktop.DBus.ObjectManager"), TEXT("GetManagedObjects"), {}, Wait,
        [this](sd_bus_message* Message, FBlueZReply& Reply) { BlueZBusCodec::ReadManagedObjects(*Api, Message, Reply.Objects); });
}

FBlueZReply FBlueZBus::Call(const FString& Path, const FString& Interface, const FString& Method, const FBlueZArgs& Args, const FBlueZWait& Wait)
{
    return Send(Path, Interface, Method, Args, Wait,
        [this](sd_bus_message* Message, FBlueZReply& Reply) { BlueZBusCodec::ReadValue(*Api, Message, Reply.Value); });
}

FBlueZReply FBlueZBus::GetProperty(const FString& Path, const FString& Interface, const FString& Property, const FBlueZWait& Wait)
{
    FBlueZArgs Args;
    Args.Strings = {Interface, Property};
    return Call(Path, TEXT("org.freedesktop.DBus.Properties"), TEXT("Get"), Args, Wait);
}

sd_bus_message* FBlueZBus::NewCall(
    const FString& Path, const FString& Interface, const FString& Method, const FBlueZArgs& Args, FBlueZReply& OutFailure)
{
    sd_bus_message* Message = nullptr;
    const int Result = Api->message_new_method_call(
        Bus, &Message, BlueZBusCodec::BlueZService, TCHAR_TO_UTF8(*Path), TCHAR_TO_UTF8(*Interface), TCHAR_TO_UTF8(*Method));
    if (Result < 0)
    {
        OutFailure = BlueZBusCodec::BusFailure(TEXT("message creation"), Result);
        return nullptr;
    }
    if (!BlueZBusCodec::AppendArgs(*Api, Message, Args))
    {
        Api->message_unref(Message);
        OutFailure = BlueZBusCodec::BusFailure(TEXT("argument encoding"), 0);
        return nullptr;
    }
    return Message;
}

bool FBlueZBus::Post(const FString& Path, const FString& Interface, const FString& Method, const FBlueZArgs& Args)
{
    FBlueZReply Failure;
    sd_bus_message* Message = NewCall(Path, Interface, Method, Args, Failure);
    if (!Message)
    {
        UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Could not queue %s.%s: %s"), *Interface, *Method, *Failure.ErrorMessage);
        return false;
    }
    int Result = Api->message_set_expect_reply(Message, 0);
    if (Result >= 0)
    {
        Result = Api->send(Bus, Message, nullptr);
    }
    Api->message_unref(Message);
    if (Result < 0)
    {
        UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Could not queue %s.%s: %s"), *Interface, *Method,
            *BlueZBusCodec::BusFailure(TEXT("send"), Result).ErrorMessage);
        return false;
    }
    return true;
}

FBlueZReply FBlueZBus::Send(const FString& Path, const FString& Interface, const FString& Method, const FBlueZArgs& Args,
    const FBlueZWait& Wait, TFunctionRef<void(sd_bus_message*, FBlueZReply&)> Decode)
{
    FBlueZReply Failure;
    sd_bus_message* Message = NewCall(Path, Interface, Method, Args, Failure);
    if (!Message)
    {
        return Failure;
    }

    BlueZBusCodec::FPending Pending;
    Pending.Api = &Api.Get();
    sd_bus_slot* Slot = nullptr;
    int Result = Api->call_async(Bus, &Slot, Message, &BlueZBusCodec::OnReply, &Pending, 0);
    Api->message_unref(Message);
    if (Result < 0)
    {
        return BlueZBusCodec::BusFailure(TEXT("send"), Result);
    }

    const GattOperationWait::EResult Outcome = GattOperationWait::Wait(
        [&Pending] { return Pending.bDone; }, [&Wait] { return Wait.Cancelled && Wait.Cancelled(); },
        [] { return FPlatformTime::Seconds(); },
        [this, &Pending](double Seconds)
        {
            Pump();
            if (!Pending.bDone) WaitForActivity(Seconds);
        },
        FMath::Min(Wait.Deadline, FPlatformTime::Seconds() + BlueZBusCodec::OperationSeconds));
    // Releasing the slot cancels the callback, so Pending cannot be written after this.
    Api->slot_unref(Slot);

    FBlueZReply Reply;
    if (Outcome == GattOperationWait::EResult::Cancelled)
    {
        Reply.ErrorName = BlueZModel::CancelledError;
    }
    else if (Outcome == GattOperationWait::EResult::TimedOut)
    {
        Reply.ErrorName = BlueZModel::TimeoutError;
        Reply.ErrorMessage = FString::Printf(TEXT("%s.%s timed out."), *Interface, *Method);
    }
    else if (!Pending.ErrorName.IsEmpty())
    {
        Reply.ErrorName = Pending.ErrorName;
        Reply.ErrorMessage = Pending.ErrorMessage;
    }
    else if (Pending.Reply)
    {
        Decode(Pending.Reply, Reply);
    }
    if (Pending.Reply)
    {
        Api->message_unref(Pending.Reply);
    }
    return Reply;
}

uint64 FBlueZBus::MatchPropertiesChanged(const FString& Path, FPropertiesHandler Handler)
{
    TUniquePtr<FMatch> Match = MakeUnique<FMatch>();
    Match->OnProperties = MoveTemp(Handler);
    return AddMatch(TCHAR_TO_UTF8(*Path), "org.freedesktop.DBus.Properties", "PropertiesChanged", MoveTemp(Match));
}

uint64 FBlueZBus::MatchInterfacesAdded(FInterfacesAddedHandler Handler)
{
    TUniquePtr<FMatch> Match = MakeUnique<FMatch>();
    Match->OnInterfacesAdded = MoveTemp(Handler);
    return AddMatch("/", "org.freedesktop.DBus.ObjectManager", "InterfacesAdded", MoveTemp(Match));
}

uint64 FBlueZBus::AddMatch(const char* Path, const char* Interface, const char* Member, TUniquePtr<FMatch> Match)
{
    Match->Owner = this;
    if (Api->match_signal(Bus, &Match->Slot, BlueZBusCodec::BlueZService, Path, Interface, Member, &FBlueZBus::OnSignal, Match.Get()) < 0)
    {
        return 0;
    }
    const uint64 Id = NextMatchId++;
    Matches.Add(Id, MoveTemp(Match));
    return Id;
}

void FBlueZBus::Unmatch(uint64 MatchId)
{
    TUniquePtr<FMatch> Match;
    if (Matches.RemoveAndCopyValue(MatchId, Match) && Match)
    {
        Api->slot_unref(Match->Slot);
    }
}

int FBlueZBus::OnSignal(sd_bus_message* Message, void* UserData, sd_bus_error*)
{
    const FMatch& Match = *static_cast<FMatch*>(UserData);
    const FSdBusApi& Api = *Match.Owner->Api;
    const char* Name = nullptr;
    if (Match.OnProperties)
    {
        BlueZModel::FProperties Changed;
        if (Api.message_read_basic(Message, 's', &Name) >= 0 && Name && BlueZBusCodec::ReadProperties(Api, Message, Changed))
        {
            Match.OnProperties(UTF8_TO_TCHAR(Name), Changed);
        }
    }
    else if (Match.OnInterfacesAdded)
    {
        BlueZModel::FInterfaces Interfaces;
        if (Api.message_read_basic(Message, 'o', &Name) >= 0 && Name && BlueZBusCodec::ReadInterfaces(Api, Message, Interfaces))
        {
            Match.OnInterfacesAdded(UTF8_TO_TCHAR(Name), Interfaces);
        }
    }
    return 0;
}

void FBlueZBus::Pump()
{
    if (!ConnectionFailure.IsEmpty())
    {
        return;
    }
    int Result = 0;
    while ((Result = Api->process(Bus, nullptr)) > 0)
    {
    }
    if (Result < 0 && Result != -EINTR)
    {
        Fail(TEXT("process"), Result);
    }
}

void FBlueZBus::WaitForActivity(double Seconds)
{
    const double Clamped = FMath::Max(0.0, Seconds);
    const int Result = ConnectionFailure.IsEmpty() ? Api->wait(Bus, static_cast<uint64_t>(Clamped * 1000000.0)) : -ENOTCONN;
    if (Result >= 0 || Result == -EINTR)
    {
        return;
    }
    if (ConnectionFailure.IsEmpty())
    {
        Fail(TEXT("wait"), Result);
    }
    // A closed connection makes wait return at once; without this sleep the worker would spin.
    FPlatformProcess::Sleep(static_cast<float>(Clamped));
}

void FBlueZBus::Fail(const TCHAR* Operation, int Result)
{
    ConnectionFailure = FString::Printf(TEXT("the system bus connection failed (%s: %s)"), Operation, UTF8_TO_TCHAR(strerror(-Result)));
    UE_LOG(LogOpenLaunchMonitor, Error, TEXT("Bluetooth: %s. Restart the game after the system bus is back."), *ConnectionFailure);
}

#endif // PLATFORM_LINUX
