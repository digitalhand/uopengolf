// SPDX-License-Identifier: MIT
// GSPro Open Connect v1 compatible messages (https://gsprogolf.com/GSProConnectV1.html):
// a connector sends JSON shots and status over TCP; the simulator answers with JSON codes.
#pragma once

#include "CoreMinimal.h"
#include "LaunchMonitorTypes.h"

/** One message from a connector. Shot and club values use the plugin's units and signs. */
struct FOpenConnectMessage
{
    /** "DeviceID": the connector or launch monitor name; may be empty. */
    FString DeviceId;
    /** "ShotNumber" as the connector counts; 0 when absent. */
    int32 DeviceShotNumber = 0;
    bool bHeartbeat = false;
    /** "LaunchMonitorIsReady", when sent. */
    TOptional<bool> bReady;
    /** A shot to deliver. ShotNumber, ReceivedUtc and Source are left for the subsystem. */
    TOptional<FLaunchMonitorShot> Shot;
    /** Club delivery for Shot, when the connector marks it present. */
    TOptional<FLaunchMonitorClubData> Club;
};

/**
 * Splits a TCP byte stream into top-level JSON objects. A message may arrive in several reads
 * and several messages in one read. Ported from open-lm-connector (MIT) addons/common/tcp_server/TcpServer.cs.
 */
class FOpenConnectFramer
{
public:
    /** Buffered bytes allowed without a complete object; beyond this the buffer is dropped. */
    static constexpr int32 MaxBufferBytes = 64 * 1024;

    /** Appends received bytes. False when the buffer overflowed and was cleared. */
    bool Append(const uint8* Data, int32 Count);

    /** The next complete object, decoded from UTF-8. Leading bytes before an object are discarded. */
    bool Next(FString& OutJson);

    void Reset() { Buffer.Reset(); }
    int32 BufferedBytes() const { return Buffer.Num(); }

private:
    TArray<uint8> Buffer;
};

namespace OpenConnectProtocol
{
    /**
     * Connector text made safe to show and log: control characters and bidirectional overrides
     * removed, then cut to MaxChars without splitting a surrogate pair.
     */
    FString SanitizeText(const FString& Text, int32 MaxChars);

    /**
     * Decodes one JSON object. False with OutError for malformed or implausible messages: every ball
     * number must be finite and in range. Out-of-range club fields are dropped; DeviceID is sanitized.
     */
    bool Decode(const FString& Json, FOpenConnectMessage& OutMessage, FString& OutError);

    /** {"Code":200,"Message":"Shot received successfully"} and the like; no delimiter, as GSPro sends. */
    FString Response(int32 Code, const FString& Message);

    /** The 201 player-information message: handedness and the selected club as a GSPro club code. */
    FString PlayerInformation(bool bLeftHanded, ELaunchMonitorClub Club);

    /** GSPro club code: DR, W3, I7, PW, PT and so on. */
    const TCHAR* ClubCode(ELaunchMonitorClub Club);
} // namespace OpenConnectProtocol
