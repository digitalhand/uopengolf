// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "LaunchMonitorTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OpenLaunchMonitorSubsystem.generated.h"

class FSquareSession;
struct FSquareClubMetrics;
struct FSquareShotMetrics;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLaunchMonitorConnectionStateChanged, ELaunchMonitorConnectionState, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLaunchMonitorBallReadyChanged, bool, bBallReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLaunchMonitorBatteryChanged, int32, BatteryPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLaunchMonitorShotReceived, const FLaunchMonitorShot&, Shot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLaunchMonitorClubDataReceived, const FLaunchMonitorClubData&, ClubData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLaunchMonitorError, const FString&, Message);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLaunchMonitorShotReceivedNative, const FLaunchMonitorShot&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLaunchMonitorClubDataReceivedNative, const FLaunchMonitorClubData&);

/**
 * Game boundary for launch-monitor input. Owns the Square Home connection and
 * delivers measured and replayed shots through the same events. All events fire
 * on the game thread.
 */
UCLASS()
class OPENLAUNCHMONITOR_API UOpenLaunchMonitorSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;

    /** Scans for a Square launch monitor and connects to the first one found. */
    UFUNCTION(BlueprintCallable, Category = "Launch Monitor")
    void Connect();

    UFUNCTION(BlueprintCallable, Category = "Launch Monitor")
    void Disconnect();

    UFUNCTION(BlueprintCallable, Category = "Launch Monitor")
    void SetClub(ELaunchMonitorClub NewClub);

    UFUNCTION(BlueprintCallable, Category = "Launch Monitor")
    void SetLeftHanded(bool bNewLeftHanded);

    /** Delivers the next built-in replay shot (recorded or synthesized Square frames) without hardware. */
    UFUNCTION(BlueprintCallable, Category = "Launch Monitor|Replay")
    bool InjectReplayShot();

    /** Decodes Square ball and optional club frames given as hex and delivers them as a replay shot. */
    UFUNCTION(BlueprintCallable, Category = "Launch Monitor|Replay")
    bool InjectReplayFrames(const FString& BallFrameHex, const FString& ClubFrameHex);

    UFUNCTION(BlueprintPure, Category = "Launch Monitor")
    ELaunchMonitorConnectionState GetConnectionState() const { return ConnectionState; }

    UFUNCTION(BlueprintPure, Category = "Launch Monitor")
    bool IsBallReady() const { return bBallReady; }

    /** -1 until the device reports it. */
    UFUNCTION(BlueprintPure, Category = "Launch Monitor")
    int32 GetBatteryPercent() const { return BatteryPercent; }

    UFUNCTION(BlueprintPure, Category = "Launch Monitor")
    FString GetFirmware() const { return Firmware; }

    UFUNCTION(BlueprintPure, Category = "Launch Monitor")
    FString GetDeviceName() const { return DeviceName; }

    UFUNCTION(BlueprintPure, Category = "Launch Monitor")
    ELaunchMonitorClub GetClub() const { return Club; }

    UFUNCTION(BlueprintPure, Category = "Launch Monitor")
    bool IsLeftHanded() const { return bLeftHanded; }

    UFUNCTION(BlueprintPure, Category = "Launch Monitor")
    bool GetLastShot(FLaunchMonitorShot& OutShot) const;

    /** Club data for the last shot, if the device has reported it. */
    UFUNCTION(BlueprintPure, Category = "Launch Monitor")
    bool GetLastClubData(FLaunchMonitorClubData& OutClubData) const;

    UFUNCTION(BlueprintPure, Category = "Launch Monitor")
    FString GetLastError() const { return LastError; }

    UFUNCTION(BlueprintPure, Category = "Launch Monitor")
    static FText GetClubDisplayName(ELaunchMonitorClub InClub);

    /** Reconnect with backoff when an established connection drops. */
    UPROPERTY(BlueprintReadWrite, Category = "Launch Monitor")
    bool bAutoReconnect = true;

    UPROPERTY(BlueprintAssignable, Category = "Launch Monitor")
    FOnLaunchMonitorConnectionStateChanged OnConnectionStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Launch Monitor")
    FOnLaunchMonitorBallReadyChanged OnBallReadyChanged;

    UPROPERTY(BlueprintAssignable, Category = "Launch Monitor")
    FOnLaunchMonitorBatteryChanged OnBatteryChanged;

    UPROPERTY(BlueprintAssignable, Category = "Launch Monitor")
    FOnLaunchMonitorShotReceived OnShotReceived;

    UPROPERTY(BlueprintAssignable, Category = "Launch Monitor")
    FOnLaunchMonitorClubDataReceived OnClubDataReceived;

    UPROPERTY(BlueprintAssignable, Category = "Launch Monitor")
    FOnLaunchMonitorError OnError;

    /** C++ equivalents of the shot and club events. */
    FOnLaunchMonitorShotReceivedNative OnShotReceivedNative;
    FOnLaunchMonitorClubDataReceivedNative OnClubDataReceivedNative;

private:
    bool EnsureSession();
    void DeliverShot(const FSquareShotMetrics& Metrics, const FString& Source, int32 SessionShotId);
    void DeliverClubData(const FSquareClubMetrics& Metrics, int32 ShotNumber);
    void SetConnectionState(ELaunchMonitorConnectionState NewState);
    void SetBallReady(bool bNewBallReady);
    void ReportError(const FString& Message);
    void ScheduleReconnect();
    void CancelReconnect();

    TSharedPtr<FSquareSession> Session;
    ELaunchMonitorConnectionState ConnectionState = ELaunchMonitorConnectionState::Disconnected;
    bool bBallReady = false;
    int32 BatteryPercent = -1;
    FString Firmware;
    FString DeviceName;
    FString LastError;
    ELaunchMonitorClub Club = ELaunchMonitorClub::Driver;
    bool bLeftHanded = false;
    int32 ShotCounter = 0;
    TMap<int32, int32> SessionShotNumbers;
    TOptional<FLaunchMonitorShot> LastShot;
    TOptional<FLaunchMonitorClubData> LastClubData;
    int32 ReplayIndex = 0;
    int32 ReconnectAttempts = 0;
    FTSTicker::FDelegateHandle ReconnectHandle;
};
