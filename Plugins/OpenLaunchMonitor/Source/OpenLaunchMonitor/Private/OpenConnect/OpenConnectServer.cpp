// SPDX-License-Identifier: MIT
#include "OpenConnect/OpenConnectServer.h"

#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/RunnableThread.h"
#include "OpenConnect/OpenConnectProtocol.h"
#include "OpenConnect/OpenConnectSocket.h"
#include "OpenLaunchMonitorLog.h"

namespace
{
    const FTimespan PollInterval = FTimespan::FromMilliseconds(25);
    /** Refused addresses logged per listener; later refusals from new addresses stay silent. */
    constexpr int32 MaxLoggedRefusals = 64;
    /** Tries to send one reply, PollInterval apart: about a second for a client that stopped reading. */
    constexpr int32 SendAttempts = 40;

    /** Sends every byte, waiting briefly while a non-blocking socket's buffer is full. */
    bool SendAll(FOpenConnectSocket& Socket, const TArray<uint8>& Bytes)
    {
        int32 Offset = 0;
        for (int32 Attempt = 0; Offset < Bytes.Num() && Attempt < SendAttempts; ++Attempt)
        {
            int32 Sent = 0;
            if (!Socket.Send(Bytes.GetData() + Offset, Bytes.Num() - Offset, Sent))
                return false;
            Offset += Sent;
            if (Offset < Bytes.Num())
                Socket.WaitWritable(PollInterval);
        }
        return Offset == Bytes.Num();
    }

    FString FormatIPv4(uint32 Ip)
    {
        return FString::Printf(TEXT("%u.%u.%u.%u"), (Ip >> 24) & 0xFF, (Ip >> 16) & 0xFF, (Ip >> 8) & 0xFF, Ip & 0xFF);
    }
} // namespace

bool FOpenConnectServer::IsLocalNetworkAddress(uint32 Ip)
{
    const uint32 A = Ip >> 24;
    const uint32 B = (Ip >> 16) & 0xFF;
    return A == 127 || A == 10 || (A == 172 && B >= 16 && B <= 31) || (A == 192 && B == 168) || (A == 169 && B == 254);
}

class FOpenConnectServer::FWorker : public FRunnable
{
public:
    FWorker(TSharedRef<FChannel> InChannel, int32 InPort, bool bInAllowLan) : Channel(InChannel), Port(InPort), bAllowLan(bInAllowLan) {}

    virtual void Stop() override { Channel->bStop = true; }

    virtual uint32 Run() override
    {
        FString Error;
        TUniquePtr<FOpenConnectSocket> Listener = FOpenConnectSocket::Listen(Port, bAllowLan, Error);
        if (!Listener)
        {
            Push(EEvent::ListenFailed, FString::Printf(TEXT("Could not listen on %s:%d: %s. Another program may be using port %d%s."),
                bAllowLan ? TEXT("0.0.0.0") : TEXT("127.0.0.1"), Port, *Error, Port,
                PLATFORM_LINUX && Port < 1024 ? TEXT("; on Linux, ports below 1024 need root") : TEXT("")));
            return 0;
        }
        Push(EEvent::Listening);

        while (!Channel->bStop)
        {
            if (Listener->WaitReadable(Client ? FTimespan::Zero() : PollInterval))
            {
                for (int32 AcceptedCount = 0; AcceptedCount < 8 && !Channel->bStop; ++AcceptedCount)
                {
                    TUniquePtr<FOpenConnectSocket> Accepted = Listener->Accept();
                    if (!Accepted) break;
                    Admit(MoveTemp(Accepted));
                }
            }
            if (!Client)
            {
                FReply Dropped;
                while (!Channel->bStop && Channel->Outgoing.Dequeue(Dropped)) {}
                continue;
            }
            if (!Serve())
                DropClient();
        }
        Client.Reset();
        return 0;
    }

private:
    /** Accepts a connection, refusing strangers in LAN mode and anyone trying to displace an active connector. */
    void Admit(TUniquePtr<FOpenConnectSocket> Accepted)
    {
        const uint32 Ip = Accepted->PeerIPv4();
        const double Now = FPlatformTime::Seconds();
        if (bAllowLan && !IsLocalNetworkAddress(Ip))
        {
            LogRefusal(Ip, TEXT("it is not on the local network"));
            return;
        }
        // A restarted connector reconnects from the same address and replaces its old connection.
        if (Client && Ip != ClientIp && Now - LastClientActivity < ActiveClientSeconds)
        {
            LogRefusal(Ip, TEXT("another connector is active"));
            return;
        }
        // Reserve a slot for the eventual disconnect, plus replacement lifecycle events.
        if (Channel->PendingEvents.load() > MaxPendingEvents - (Client ? 3 : 2))
        {
            UE_LOG(LogOpenLaunchMonitor, Verbose, TEXT("Open Connect refused a connection from %s: the game has not caught up with earlier events."),
                *FormatIPv4(Ip));
            return;
        }
        if (Client)
            DropClient();
        Generation = ++NextGeneration;
        Channel->ClientGeneration = Generation;
        Client = MoveTemp(Accepted);
        ClientIp = Ip;
        LastClientActivity = Now;
        BytesSinceMessage = 0;
        // The listener owns the allowance: replacements cannot replenish it.
        Held.Reset();
        Framer.Reset();
        Push(EEvent::ClientConnected, Client->PeerString());
    }

    void LogRefusal(uint32 Ip, const TCHAR* Reason)
    {
        if (!LoggedRefusals.Contains(Ip) && LoggedRefusals.Num() < MaxLoggedRefusals)
        {
            LoggedRefusals.Add(Ip);
            UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("Open Connect refused a connection from %s: %s."), *FormatIPv4(Ip), Reason);
        }
    }

    void DropClient()
    {
        Channel->ClientGeneration = 0;
        Client.Reset();
        Held.Reset();
        Framer.Reset();
        Push(EEvent::ClientDisconnected);
    }

    /** One pass for the attached client. False when the connection was lost or the client is dropped. */
    bool Serve()
    {
        FReply Reply;
        while (!Channel->bStop && Channel->Outgoing.Dequeue(Reply))
        {
            if (Reply.Generation != Generation) continue;
            if (!SendAll(*Client, Reply.Bytes))
                return false;
        }

        const double Now = FPlatformTime::Seconds();
        Tokens = FMath::Min(MessageBurst, Tokens + (Now - LastRefill) * MessagesPerSecond);
        LastRefill = Now;
        Deliver();

        // Read only while there is allowance and room: otherwise the bytes stay in the kernel and
        // TCP slows the sender down, instead of anything growing here or on the game thread.
        if (!Held.IsSet() && Tokens >= 1.0 && Channel->PendingEvents.load() < MaxPendingEvents - 1)
        {
            if (Client->WaitReadable(PollInterval))
            {
                int32 Read = 0;
                // False means the connector closed the connection or it failed; true with 0 bytes is "nothing yet".
                if (!Client->Recv(Buffer, sizeof(Buffer), Read))
                    return false;
                if (Read > 0)
                {
                    BytesSinceMessage += Read;
                    if (!Framer.Append(Buffer, Read))
                        Push(EEvent::Overflow);
                    Deliver();
                    // Oversized objects cost no rate allowance, and junk outside any object is discarded
                    // unseen: a client that sends this much without one complete message is dropped.
                    if (BytesSinceMessage > MaxBytesWithoutMessage)
                    {
                        UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("Open Connect connector %s sent %d KB without a complete message; disconnected."),
                            *Client->PeerString(), BytesSinceMessage / 1024);
                        return false;
                    }
                }
            }
        }
        else
        {
            FPlatformProcess::SleepNoStats(static_cast<float>(PollInterval.GetTotalSeconds()));
        }

        // Over the limit: the allowance is spent and the client still has more waiting. A game thread
        // that is merely busy (events not yet taken) never counts against the client.
        const bool bOverLimit = Tokens < 1.0 && (Held.IsSet() || Client->WaitReadable(FTimespan::Zero()));
        if (!bOverLimit)
        {
            ThrottledSince = 0.0;
        }
        else if (ThrottledSince == 0.0)
        {
            ThrottledSince = Now;
        }
        else if (Now - ThrottledSince >= FloodDisconnectSeconds)
        {
            UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("Open Connect connector %s sent more than %.0f messages a second for %.0f s; disconnected."),
                *Client->PeerString(), MessagesPerSecond, FloodDisconnectSeconds);
            return false;
        }
        return true;
    }

    /** Hands complete messages to the game thread while the rate allowance and the queue allow. */
    void Deliver()
    {
        while (Tokens >= 1.0 && Channel->PendingEvents.load() < MaxPendingEvents - 1)
        {
            FString Json;
            if (Held.IsSet())
            {
                Json = MoveTemp(Held.GetValue());
                Held.Reset();
            }
            else if (!Framer.Next(Json))
            {
                return;
            }
            Tokens -= 1.0;
            // Only complete messages count as activity, so a client sending junk can't hold the slot.
            LastClientActivity = FPlatformTime::Seconds();
            BytesSinceMessage = 0;
            Push(EEvent::Message, MoveTemp(Json));
        }
        // Keep the next complete message ready, so a waiting backlog is visible to the rate check.
        FString Next;
        if (!Held.IsSet() && Framer.Next(Next))
            Held = MoveTemp(Next);
    }

    void Push(EEvent Type, FString Text = FString())
    {
        ++Channel->PendingEvents;
        Channel->Incoming.Enqueue({Type, MoveTemp(Text), Generation});
    }

    TSharedRef<FChannel> Channel;
    int32 Port;
    bool bAllowLan;

    TUniquePtr<FOpenConnectSocket> Client;
    uint32 ClientIp = 0;
    uint64 Generation = 0;
    uint64 NextGeneration = 0;
    double LastClientActivity = 0.0;
    /** Bytes received since the last complete message (or the connection). */
    int32 BytesSinceMessage = 0;
    /** Token bucket: messages the client may still send now. */
    double Tokens = MessageBurst;
    double LastRefill = FPlatformTime::Seconds();
    /** When the client went over the rate limit; 0 while it is within it. */
    double ThrottledSince = 0.0;
    /** A complete message taken from the framer but not yet delivered. */
    TOptional<FString> Held;
    FOpenConnectFramer Framer;
    TSet<uint32> LoggedRefusals;
    uint8 Buffer[4096];
};

FOpenConnectServer::FOpenConnectServer() = default;

FOpenConnectServer::~FOpenConnectServer()
{
    Stop();
}

bool FOpenConnectServer::Start(int32 Port, bool bAllowLan)
{
    Stop();
    Channel = MakeShared<FChannel>();
    Worker = MakeUnique<FWorker>(Channel.ToSharedRef(), Port, bAllowLan);
    Thread.Reset(FRunnableThread::Create(Worker.Get(), TEXT("OpenConnectServer")));
    if (!Thread)
    {
        Worker.Reset();
        Channel.Reset();
        return false;
    }
    TWeakPtr<FOpenConnectServer> WeakThis = AsShared();
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
    {
        if (const TSharedPtr<FOpenConnectServer> This = WeakThis.Pin())
        {
            This->ProcessEvents();
            return true;
        }
        return false;
    }));
    return true;
}

void FOpenConnectServer::Stop()
{
    if (TickerHandle.IsValid())
    {
        FTSTicker::RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }
    if (Thread)
    {
        // Kill calls the worker's Stop and waits; the worker closes its own sockets.
        Thread->Kill(true);
        Thread.Reset();
    }
    Worker.Reset();
    Channel.Reset();
}

void FOpenConnectServer::Send(const FString& Json)
{
    if (Channel && (!DispatchChannel || DispatchChannel == Channel))
    {
        const uint64 Generation = DispatchChannel ? DispatchGeneration : Channel->ClientGeneration.load();
        if (Generation == 0) return;
        const FTCHARToUTF8 Utf8(*Json);
        Channel->Outgoing.Enqueue({TArray<uint8>(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length()), Generation});
    }
}

void FOpenConnectServer::ProcessEvents()
{
    // A callback may stop or restart the server; events from a replaced channel are then dropped.
    // A bounded number per call keeps a burst from stalling one frame; the rest wait for the next.
    const TSharedPtr<FChannel> Current = Channel;
    FEvent Event;
    for (int32 Delivered = 0; Delivered < MaxEventsPerTick && Current && Channel == Current && Current->Incoming.Dequeue(Event); ++Delivered)
    {
        --Current->PendingEvents;
        TGuardValue<TSharedPtr<FChannel>> ChannelGuard(DispatchChannel, Current);
        TGuardValue<uint64> GenerationGuard(DispatchGeneration, Event.Generation);
        switch (Event.Type)
        {
        case EEvent::Listening:
            if (OnListening) OnListening();
            break;
        case EEvent::ListenFailed:
            if (OnListenFailed) OnListenFailed(Event.Text);
            break;
        case EEvent::ClientConnected:
            if (OnClientConnected) OnClientConnected(Event.Text);
            break;
        case EEvent::ClientDisconnected:
            if (OnClientDisconnected) OnClientDisconnected();
            break;
        case EEvent::Message:
            if (OnMessage) OnMessage(Event.Text);
            break;
        case EEvent::Overflow:
            if (OnOverflow) OnOverflow();
            break;
        }
    }
}
