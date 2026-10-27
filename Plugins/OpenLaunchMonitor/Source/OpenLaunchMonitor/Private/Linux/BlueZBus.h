// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

#if PLATFORM_LINUX

#include "Bluetooth/BlueZModel.h"
#include "Linux/SdBusApi.h"

/** Bounds one blocking call: Cancelled is polled at most every 25 ms; Deadline is FPlatformTime seconds. */
struct FBlueZWait
{
    TFunction<bool()> Cancelled;
    double Deadline = 0;
};

/** Method arguments, appended in this order: strings (s), bytes (ay), options (a{sv}). */
struct FBlueZArgs
{
    TArray<FString> Strings;
    TOptional<TArray<uint8>> Bytes;
    TOptional<BlueZModel::FProperties> Options;
};

struct FBlueZReply
{
    /** Empty on success; BlueZModel::TimeoutError or CancelledError for local outcomes. */
    FString ErrorName;
    FString ErrorMessage;
    /** The first reply argument: an ay result, or the contents of a v result. */
    BlueZModel::FValue Value;
    /** Filled by GetManagedObjects. */
    BlueZModel::FManagedObjects Objects;

    bool Ok() const { return ErrorName.IsEmpty(); }
};

/**
 * One system-bus connection to org.bluez through runtime-loaded sd-bus. Not thread-safe:
 * create, call, pump and destroy it on one thread. Signal handlers run inside Pump or a call's wait.
 */
class FBlueZBus
{
public:
    using FPropertiesHandler = TFunction<void(const FString& Interface, const BlueZModel::FProperties& Changed)>;
    using FInterfacesAddedHandler = TFunction<void(const FString& Path, const BlueZModel::FInterfaces& Interfaces)>;

    /** Loads libsystemd.so.0 and opens the system bus; null with OutError on failure. */
    static TUniquePtr<FBlueZBus> Open(FString& OutError);
    ~FBlueZBus();

    FBlueZReply GetManagedObjects(const FBlueZWait& Wait);
    FBlueZReply Call(const FString& Path, const FString& Interface, const FString& Method, const FBlueZArgs& Args, const FBlueZWait& Wait);
    FBlueZReply GetProperty(const FString& Path, const FString& Interface, const FString& Property, const FBlueZWait& Wait);
    /** Queues a call with no reply expected and returns at once; false if it could not be queued. Closing the bus flushes it. */
    bool Post(const FString& Path, const FString& Interface, const FString& Method, const FBlueZArgs& Args = {});

    /** Returns 0 when the match could not be added. */
    uint64 MatchPropertiesChanged(const FString& Path, FPropertiesHandler Handler);
    uint64 MatchInterfacesAdded(FInterfacesAddedHandler Handler);
    void Unmatch(uint64 MatchId);

    /** Processes queued messages until idle, dispatching matched signals on this thread. */
    void Pump();
    /** Blocks for bus activity for at most Seconds; sleeps instead once the connection has failed. */
    void WaitForActivity(double Seconds);

    /** Why the connection stopped working (for example a restarted system bus); empty while it works. */
    const FString& GetFailure() const { return ConnectionFailure; }

    FBlueZBus(const FBlueZBus&) = delete;
    FBlueZBus& operator=(const FBlueZBus&) = delete;

private:
    struct FMatch;

    FBlueZBus(TSharedRef<const FSdBusApi> InApi, sd_bus* InBus);
    /** A method call to org.bluez with Args appended; null with OutFailure set on failure. */
    sd_bus_message* NewCall(const FString& Path, const FString& Interface, const FString& Method, const FBlueZArgs& Args, FBlueZReply& OutFailure);
    FBlueZReply Send(const FString& Path, const FString& Interface, const FString& Method, const FBlueZArgs& Args, const FBlueZWait& Wait,
        TFunctionRef<void(sd_bus_message*, FBlueZReply&)> Decode);
    uint64 AddMatch(const char* Path, const char* Interface, const char* Member, TUniquePtr<FMatch> Match);
    static int OnSignal(sd_bus_message* Message, void* UserData, sd_bus_error* RetError);
    void Fail(const TCHAR* Operation, int Result);

    TSharedRef<const FSdBusApi> Api;
    sd_bus* Bus = nullptr;
    TMap<uint64, TUniquePtr<FMatch>> Matches;
    uint64 NextMatchId = 1;
    FString ConnectionFailure;
};

#endif // PLATFORM_LINUX
