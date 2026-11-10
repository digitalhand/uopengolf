// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

/**
 * Non-blocking IPv4 TCP socket for the Open Connect worker, used from that one thread only.
 * On Windows it is a Winsock socket so the listener can bind with SO_EXCLUSIVEADDRUSE: no other
 * process can share or take over the port while the game listens. Elsewhere it wraps FSocket;
 * Linux already refuses another user's process the same port.
 */
class FOpenConnectSocket
{
public:
    /** Listens on 127.0.0.1, or every interface when bAllowLan. Null with OutError if the port cannot be bound. */
    static TUniquePtr<FOpenConnectSocket> Listen(int32 Port, bool bAllowLan, FString& OutError);

    ~FOpenConnectSocket();
    FOpenConnectSocket(const FOpenConnectSocket&) = delete;
    FOpenConnectSocket& operator=(const FOpenConnectSocket&) = delete;

    /** Listener only: the next pending connection, already non-blocking, or null. */
    TUniquePtr<FOpenConnectSocket> Accept();

    /** Waits for a pending connection (listener) or for data or a close (client). */
    bool WaitReadable(FTimespan Timeout);
    bool WaitWritable(FTimespan Timeout);

    /** False when the peer closed the connection or it failed; true with 0 bytes is "nothing yet". */
    bool Recv(uint8* Data, int32 Size, int32& OutRead);
    /** False when the connection failed; true with OutSent 0 when the send buffer is full. */
    bool Send(const uint8* Data, int32 Size, int32& OutSent);

    /** The peer's IPv4 address in host order (127.0.0.1 is 0x7F000001); 0 for a listener. */
    uint32 PeerIPv4() const { return PeerIp; }
    /** "a.b.c.d:port" of the peer. */
    FString PeerString() const;

private:
    struct FImpl;
    explicit FOpenConnectSocket(TUniquePtr<FImpl> InImpl, uint32 InPeerIp, uint16 InPeerPort);

    TUniquePtr<FImpl> Impl;
    uint32 PeerIp = 0;
    uint16 PeerPort = 0;
};
