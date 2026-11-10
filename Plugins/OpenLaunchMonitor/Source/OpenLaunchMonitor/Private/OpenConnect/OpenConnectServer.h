// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Containers/Ticker.h"
#include "HAL/Runnable.h"
#include <atomic>

class FRunnableThread;

/**
 * TCP listener for Open Connect connectors. One worker thread owns the sockets: it accepts one
 * client at a time, splits the stream into JSON objects and sends queued replies. Events reach the
 * callbacks on the game thread, from the core ticker or an explicit ProcessEvents call. Create with MakeShared.
 *
 * The protocol has no authentication, so the listener limits who and how much:
 * - In LAN mode only loopback, private (10/8, 172.16/12, 192.168/16) and link-local peers are accepted.
 * - A new connection from the current client's address replaces it, as a restarted connector would.
 *   One from another address is refused until the current client has been silent for ActiveClientSeconds.
 * - Messages are rate limited (MessagesPerSecond, bursts of MessageBurst). Over the limit the worker
 *   stops reading, so TCP slows the sender; one that stays over it for FloodDisconnectSeconds is dropped.
 * - A client that sends MaxBytesWithoutMessage without completing a message is dropped, and only
 *   complete messages count as activity, so junk can't hold the connection slot.
 */
class FOpenConnectServer : public TSharedFromThis<FOpenConnectServer>
{
public:
    // Out of line: FWorker is incomplete here, and this module builds with C++ exceptions.
    FOpenConnectServer();
    ~FOpenConnectServer();

    /** Starts listening on 127.0.0.1, or on every interface when bAllowLan. False if the worker could not start. */
    bool Start(int32 Port, bool bAllowLan);

    /** Stops the worker and closes the sockets; waits for the worker. Undelivered events are dropped. */
    void Stop();

    bool IsRunning() const { return Thread.IsValid(); }

    /** Loopback, private or link-local IPv4 (host order): the peers LAN mode accepts. */
    static bool IsLocalNetworkAddress(uint32 Ip);

    static constexpr double MessagesPerSecond = 20.0;
    static constexpr double MessageBurst = 40.0;
    static constexpr double FloodDisconnectSeconds = 5.0;
    static constexpr double ActiveClientSeconds = 10.0;
    /** Bytes a client may send without completing a message: four oversized (413) objects. */
    static constexpr int32 MaxBytesWithoutMessage = 256 * 1024;
    /** Worker events waiting for the game thread before the worker stops reading. */
    static constexpr int32 MaxPendingEvents = 64;
    /** Events one ProcessEvents call delivers; the rest wait for the next tick. */
    static constexpr int32 MaxEventsPerTick = 32;

    /** Queues a UTF-8 reply for the current client; dropped when no client is connected. */
    void Send(const FString& Json);

    /** Delivers worker events to the callbacks. Runs every frame from the core ticker while running. */
    void ProcessEvents();

    TFunction<void()> OnListening;
    TFunction<void(const FString& Error)> OnListenFailed;
    TFunction<void(const FString& Peer)> OnClientConnected;
    TFunction<void()> OnClientDisconnected;
    TFunction<void(const FString& Json)> OnMessage;
    /** The client sent more than FOpenConnectFramer::MaxBufferBytes without a complete object. */
    TFunction<void()> OnOverflow;

private:
    friend class FOpenConnectReconnectTest;
    enum class EEvent : uint8 { Listening, ListenFailed, ClientConnected, ClientDisconnected, Message, Overflow };
    struct FEvent
    {
        EEvent Type;
        FString Text;
        uint64 Generation = 0;
    };
    struct FReply
    {
        TArray<uint8> Bytes;
        uint64 Generation = 0;
    };
    /** Shared by the game thread and the worker; a new one per Start, so a stopped worker's events can't leak. */
    struct FChannel
    {
        TQueue<FEvent, EQueueMode::Mpsc> Incoming;
        TQueue<FReply, EQueueMode::Mpsc> Outgoing;
        std::atomic<uint64> ClientGeneration{0};
        std::atomic<bool> bStop{false};
        /** Events in Incoming, so the worker can apply backpressure when the game thread falls behind. */
        std::atomic<int32> PendingEvents{0};
    };
    class FWorker;

    TSharedPtr<FChannel> Channel;
    TSharedPtr<FChannel> DispatchChannel;
    uint64 DispatchGeneration = 0;
    TUniquePtr<FWorker> Worker;
    TUniquePtr<FRunnableThread> Thread;
    FTSTicker::FDelegateHandle TickerHandle;
};
