// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "LaunchMonitorTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OpenLaunchMonitorSubsystem.generated.h"

UENUM(BlueprintType)
enum class ESquareSpinMode : uint8 { Measured, Calculated };

class FOpenConnectServer;
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
 * Game boundary for launch-monitor input. Owns the active provider (the Square Home
 * connection or the Open Connect listener) and delivers measured and replayed shots
 * through the same events. All events fire on the game thread.
 */
UCLASS()
class OPENLAUNCHMONITOR_API UOpenLaunchMonitorSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;
    UFUNCTION(BlueprintCallable, Category = "Launch Monitor")
    void SetSpinMode(ESquareSpinMode Mode);
    UFUNCTION(BlueprintPure, Category = "Launch Monitor")
    ESquareSpinMode GetSpinMode() const { return SpinMode; }


    /**
     * Square Home: scans and connects to the first device found. Open Connect: starts listening
     * for a connector (state Connecting while waiting, Connected once one is attached).
     */
    UFUNCTION(BlueprintCallable, Category = "Launch Monitor")
    void Connect();

    UFUNCTION(BlueprintCallable, Category = "Launch Monitor")
    void Disconnect();

    /** Switches the active provider; the previous one is disconnected and the state returns to Disconnected. */
    UFUNCTION(BlueprintCallable, Category = "Launch Monitor")
    void SetProvider(ELaunchMonitorProvider NewProvider);

    UFUNCTION(BlueprintPure, Category = "Launch Monitor")
    ELaunchMonitorProvider GetProvider() const { return Provider; }

    /** Open Connect port and whether connectors on other computers may connect; a running listener restarts. */
    UFUNCTION(BlueprintCallable, Category = "Launch Monitor|Open Connect")
    void SetOpenConnectEndpoint(int32 Port, bool bAllowLan);

    UFUNCTION(BlueprintPure, Category = "Launch Monitor|Open Connect")
    int32 GetOpenConnectPort() const { return OpenConnectPort; }

    UFUNCTION(BlueprintPure, Category = "Launch Monitor|Open Connect")
    bool IsOpenConnectLanAllowed() const { return bOpenConnectAllowLan; }

    /** Where a connector should send shots: "127.0.0.1:921", or this computer's network address when LAN is allowed. */
    UFUNCTION(BlueprintPure, Category = "Launch Monitor|Open Connect")
    FString GetOpenConnectAddress() const;

    UFUNCTION(BlueprintPure, Category = "Launch Monitor|Open Connect")
    bool IsOpenConnectListening() const;

    /** When the connector last sent anything; false before the first message. */
    bool GetOpenConnectLastMessageUtc(FDateTime& OutUtc) const;

    /**
     * Called by a shot handler that refuses a shot (menu open, unusable data) while the shot event is
     * being broadcast. An Open Connect connector then receives 501 with Reason instead of 200.
     */
    void RejectShot(int32 ShotNumber, const FString& Reason);

    UFUNCTION(BlueprintCallable, Category = "Launch Monitor")
    void SetClub(ELaunchMonitorClub NewClub);

    UFUNCTION(BlueprintCallable, Category = "Launch Monitor")
    void SetLeftHanded(bool bNewLeftHanded);

    /** Delivers the next built-in replay shot (recorded or synthesized Square frames) without hardware. */
    UFUNCTION(BlueprintCallable, Category = "Launch Monitor|Replay")
    bool InjectReplayShot();

    /** Stable catalog order; selecting a preset does not deliver a shot. */
    UFUNCTION(BlueprintPure, Category = "Launch Monitor|Replay")
    static TArray<FString> GetReplayShotNames();

    UFUNCTION(BlueprintPure, Category = "Launch Monitor|Replay")
    static FString GetReplayShotDescription(int32 Index);

    /** Delivers the chosen preset without advancing the cycling replay cursor. */
    UFUNCTION(BlueprintCallable, Category = "Launch Monitor|Replay")
    bool InjectReplayShotAt(int32 Index);

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
    UFUNCTION(BlueprintSetter, Category = "Launch Monitor")
    void SetAutoReconnect(bool bEnabled);

    UPROPERTY(BlueprintReadWrite, BlueprintSetter = SetAutoReconnect, Category = "Launch Monitor")
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
    friend class FLaunchMonitorReconnectSettingTest;
    bool EnsureSession();
    bool IsSquareActive() const { return Provider == ELaunchMonitorProvider::SquareHome; }
    void DeliverShot(const FSquareShotMetrics& Metrics, const FString& Source, int32 SessionShotId);
    /** Numbers, timestamps and broadcasts a shot whose values and Source are filled in. */
    void DeliverShot(FLaunchMonitorShot Shot, int32 SessionShotId);
    void DeliverClubData(const FSquareClubMetrics& Metrics, int32 ShotNumber);
    void DeliverClubData(const FLaunchMonitorClubData& ClubData);
    void StartOpenConnect();
    void StopOpenConnect();
    void HandleOpenConnectMessage(const FString& Json);
    void SendOpenConnectPlayer();
    void SetConnectionState(ELaunchMonitorConnectionState NewState);
    void SetBallReady(bool bNewBallReady);
    void ReportError(const FString& Message);
    /** Passes the olm.Square.* timing console values to the session before a connect. */
    void ApplyTiming();
    void ScheduleReconnect();
    void CancelReconnect();

    TSharedPtr<FSquareSession> Session;
    TSharedPtr<FOpenConnectServer> OpenConnect;
    ELaunchMonitorProvider Provider = ELaunchMonitorProvider::SquareHome;
    int32 OpenConnectPort = LaunchMonitorDefaultOpenConnectPort;
    bool bOpenConnectAllowLan = false;
    bool bOpenConnectClient = false;
    /** Messages rejected since the connector attached; limits the log. */
    int32 OpenConnectRejections = 0;
    /** This computer's network address, looked up when LAN connections are allowed. */
    FString OpenConnectLanHost;
    TOptional<FDateTime> OpenConnectLastMessageUtc;
    /** The shot being broadcast, and a rejection a handler reported for it. */
    int32 BroadcastingShotNumber = 0;
    TOptional<FString> BroadcastRejection;
    ELaunchMonitorConnectionState ConnectionState = ELaunchMonitorConnectionState::Disconnected;
    bool bBallReady = false;
    int32 BatteryPercent = -1;
    FString Firmware;
    FString DeviceName;
    FString LastError;
    ELaunchMonitorClub Club = ELaunchMonitorClub::Driver;
    bool bLeftHanded = false;
    ESquareSpinMode SpinMode = ESquareSpinMode::Measured;
    int32 ShotCounter = 0;
    TMap<int32, int32> SessionShotNumbers;
    TOptional<FLaunchMonitorShot> LastShot;
    TOptional<FLaunchMonitorClubData> LastClubData;
    int32 ReplayIndex = 0;
    int32 ReconnectAttempts = 0;
    FTSTicker::FDelegateHandle ReconnectHandle;
};
