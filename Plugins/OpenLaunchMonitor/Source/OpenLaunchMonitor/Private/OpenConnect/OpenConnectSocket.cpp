// SPDX-License-Identifier: MIT
#include "OpenConnect/OpenConnectSocket.h"

FOpenConnectSocket::FOpenConnectSocket(TUniquePtr<FImpl> InImpl, uint32 InPeerIp, uint16 InPeerPort)
    : Impl(MoveTemp(InImpl)), PeerIp(InPeerIp), PeerPort(InPeerPort)
{
}

FString FOpenConnectSocket::PeerString() const
{
    return FString::Printf(TEXT("%u.%u.%u.%u:%u"), (PeerIp >> 24) & 0xFF, (PeerIp >> 16) & 0xFF, (PeerIp >> 8) & 0xFF, PeerIp & 0xFF, PeerPort);
}

#if PLATFORM_WINDOWS

#include "Windows/AllowWindowsPlatformTypes.h"
#include <winsock2.h>
#include <ws2tcpip.h>

struct FOpenConnectSocket::FImpl
{
    SOCKET Handle = INVALID_SOCKET;
    bool bListener = false;
    /** Each socket holds its own Winsock reference, so a client may outlive its listener. */
    bool bStarted = false;

    FImpl()
    {
        WSADATA Data;
        bStarted = WSAStartup(MAKEWORD(2, 2), &Data) == 0;
    }

    ~FImpl()
    {
        if (Handle != INVALID_SOCKET)
            closesocket(Handle);
        if (bStarted)
            WSACleanup();
    }
};

namespace
{
    FString SocketError(const TCHAR* Step)
    {
        const int Code = WSAGetLastError();
        if (Code == WSAEADDRINUSE || Code == WSAEACCES)
            return FString::Printf(TEXT("the port is in use (Winsock error %d)"), Code);
        return FString::Printf(TEXT("%s failed (Winsock error %d)"), Step, Code);
    }

    bool SetNonBlocking(SOCKET Handle)
    {
        u_long On = 1;
        return ioctlsocket(Handle, FIONBIO, &On) == 0;
    }

    bool WaitFor(SOCKET Handle, bool bRead, FTimespan Timeout)
    {
        fd_set Set;
        FD_ZERO(&Set);
        FD_SET(Handle, &Set);
        const int64 Microseconds = FMath::Max<int64>(0, Timeout.GetTicks() / ETimespan::TicksPerMicrosecond);
        timeval Wait;
        Wait.tv_sec = static_cast<long>(Microseconds / 1000000);
        Wait.tv_usec = static_cast<long>(Microseconds % 1000000);
        return select(0, bRead ? &Set : nullptr, bRead ? nullptr : &Set, nullptr, &Wait) > 0;
    }
} // namespace

TUniquePtr<FOpenConnectSocket> FOpenConnectSocket::Listen(int32 Port, bool bAllowLan, FString& OutError)
{
    TUniquePtr<FImpl> Listener = MakeUnique<FImpl>();
    if (!Listener->bStarted)
    {
        OutError = TEXT("Winsock could not start");
        return nullptr;
    }
    Listener->Handle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_NO_HANDLE_INHERIT);
    if (Listener->Handle == INVALID_SOCKET)
    {
        OutError = SocketError(TEXT("socket"));
        return nullptr;
    }
    // Set before bind: no other process can bind this port, even with SO_REUSEADDR, while it is open.
    BOOL bExclusive = TRUE;
    if (setsockopt(Listener->Handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&bExclusive), sizeof(bExclusive)) != 0)
    {
        OutError = SocketError(TEXT("SO_EXCLUSIVEADDRUSE"));
        return nullptr;
    }
    sockaddr_in Address = {};
    Address.sin_family = AF_INET;
    Address.sin_port = htons(static_cast<u_short>(Port));
    Address.sin_addr.s_addr = htonl(bAllowLan ? INADDR_ANY : INADDR_LOOPBACK);
    if (bind(Listener->Handle, reinterpret_cast<const sockaddr*>(&Address), sizeof(Address)) != 0)
    {
        OutError = SocketError(TEXT("bind"));
        return nullptr;
    }
    if (listen(Listener->Handle, 4) != 0 || !SetNonBlocking(Listener->Handle))
    {
        OutError = SocketError(TEXT("listen"));
        return nullptr;
    }
    Listener->bListener = true;
    return TUniquePtr<FOpenConnectSocket>(new FOpenConnectSocket(MoveTemp(Listener), 0, 0));
}

FOpenConnectSocket::~FOpenConnectSocket() = default;

TUniquePtr<FOpenConnectSocket> FOpenConnectSocket::Accept()
{
    if (!Impl->bListener)
        return nullptr;
    sockaddr_in Peer = {};
    int Length = sizeof(Peer);
    const SOCKET Handle = accept(Impl->Handle, reinterpret_cast<sockaddr*>(&Peer), &Length);
    if (Handle == INVALID_SOCKET)
        return nullptr;
    TUniquePtr<FImpl> Client = MakeUnique<FImpl>();
    Client->Handle = Handle;
    SetHandleInformation(reinterpret_cast<HANDLE>(Handle), HANDLE_FLAG_INHERIT, 0);
    if (!SetNonBlocking(Handle))
        return nullptr;
    return TUniquePtr<FOpenConnectSocket>(new FOpenConnectSocket(MoveTemp(Client), ntohl(Peer.sin_addr.s_addr), ntohs(Peer.sin_port)));
}

bool FOpenConnectSocket::WaitReadable(FTimespan Timeout)
{
    return WaitFor(Impl->Handle, true, Timeout);
}

bool FOpenConnectSocket::WaitWritable(FTimespan Timeout)
{
    return WaitFor(Impl->Handle, false, Timeout);
}

bool FOpenConnectSocket::Recv(uint8* Data, int32 Size, int32& OutRead)
{
    const int Result = recv(Impl->Handle, reinterpret_cast<char*>(Data), Size, 0);
    OutRead = FMath::Max(Result, 0);
    if (Result > 0)
        return true;
    // 0 is an orderly close by the peer.
    return Result < 0 && WSAGetLastError() == WSAEWOULDBLOCK;
}

bool FOpenConnectSocket::Send(const uint8* Data, int32 Size, int32& OutSent)
{
    const int Result = send(Impl->Handle, reinterpret_cast<const char*>(Data), Size, 0);
    OutSent = FMath::Max(Result, 0);
    return Result >= 0 || WSAGetLastError() == WSAEWOULDBLOCK;
}

#include "Windows/HideWindowsPlatformTypes.h"

#else

#include "Common/TcpSocketBuilder.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

struct FOpenConnectSocket::FImpl
{
    FSocket* Socket = nullptr;
    bool bListener = false;

    ~FImpl()
    {
        if (Socket)
        {
            Socket->Close();
            ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
        }
    }
};

TUniquePtr<FOpenConnectSocket> FOpenConnectSocket::Listen(int32 Port, bool bAllowLan, FString& OutError)
{
    const FIPv4Endpoint Endpoint(bAllowLan ? FIPv4Address::Any : FIPv4Address::InternalLoopback, static_cast<uint16>(Port));
    TUniquePtr<FImpl> Listener = MakeUnique<FImpl>();
    Listener->Socket = FTcpSocketBuilder(TEXT("OpenConnectListener")).AsNonBlocking().BoundToEndpoint(Endpoint).Listening(4).Build();
    if (!Listener->Socket)
    {
        OutError = TEXT("the port could not be bound");
        return nullptr;
    }
    Listener->bListener = true;
    return TUniquePtr<FOpenConnectSocket>(new FOpenConnectSocket(MoveTemp(Listener), 0, 0));
}

FOpenConnectSocket::~FOpenConnectSocket() = default;

TUniquePtr<FOpenConnectSocket> FOpenConnectSocket::Accept()
{
    bool bPending = false;
    if (!Impl->bListener || !Impl->Socket->HasPendingConnection(bPending) || !bPending)
        return nullptr;
    FSocket* Accepted = Impl->Socket->Accept(TEXT("OpenConnectClient"));
    if (!Accepted)
        return nullptr;
    TUniquePtr<FImpl> Client = MakeUnique<FImpl>();
    Client->Socket = Accepted;
    Accepted->SetNonBlocking(true);
    const TSharedRef<FInternetAddr> Peer = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    uint32 Ip = 0;
    if (Accepted->GetPeerAddress(*Peer))
        Peer->GetIp(Ip);
    return TUniquePtr<FOpenConnectSocket>(new FOpenConnectSocket(MoveTemp(Client), Ip, static_cast<uint16>(Peer->GetPort())));
}

bool FOpenConnectSocket::WaitReadable(FTimespan Timeout)
{
    if (Impl->bListener)
    {
        bool bPending = false;
        return Impl->Socket->WaitForPendingConnection(bPending, Timeout) && bPending;
    }
    return Impl->Socket->Wait(ESocketWaitConditions::WaitForRead, Timeout);
}

bool FOpenConnectSocket::WaitWritable(FTimespan Timeout)
{
    return Impl->Socket->Wait(ESocketWaitConditions::WaitForWrite, Timeout);
}

bool FOpenConnectSocket::Recv(uint8* Data, int32 Size, int32& OutRead)
{
    // FSocket reports an orderly close of a stream as failure and "nothing yet" as success with 0 bytes.
    return Impl->Socket->Recv(Data, Size, OutRead);
}

bool FOpenConnectSocket::Send(const uint8* Data, int32 Size, int32& OutSent)
{
    if (Impl->Socket->Send(Data, Size, OutSent))
        return true;
    OutSent = 0;
    return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode() == SE_EWOULDBLOCK;
}

#endif
