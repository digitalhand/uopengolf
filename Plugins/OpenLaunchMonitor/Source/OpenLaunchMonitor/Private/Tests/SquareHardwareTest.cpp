// SPDX-License-Identifier: MIT
// Times a real Square Home: connect to Ready with the shipped and the upstream handshake delays,
// holds Ready for 20 s so the log shows the device's notification rhythm, and optionally a power
// cycle (link-loss detection and reconnect). Opt-in, because it needs the device switched on and
// nearby: set OLM_SQUARE_HARDWARE=1 (OLM_SQUARE_POWER_CYCLE=1 for the power cycle,
// OLM_SQUARE_SKIP_TIMING=1 to skip the connect timings) before running UOpenGolf.LaunchMonitor.Hardware. Results go to
// Saved/Automation/SquareHardware/results.txt; during a power cycle, status.txt says when to
// switch the device off and on.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Bluetooth/GattTransport.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "OpenLaunchMonitorLog.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Square/SquareScheduler.h"
#include "Square/SquareSession.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    constexpr double ConnectTimeoutSeconds = 60.0;
    constexpr double SettleSeconds = 3.0;
    constexpr double PowerStepTimeoutSeconds = 240.0;
    constexpr double CadenceHoldSeconds = 20.0;

    /** Keeps every LogOpenLaunchMonitor line with its time, including those from the Bluetooth worker. */
    class FLaunchMonitorLogCapture final : public FOutputDevice
    {
    public:
        explicit FLaunchMonitorLogCapture(double InOrigin) : Origin(InOrigin) {}

        virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type, const FName& Category) override
        {
            if (Category == LogOpenLaunchMonitor.GetCategoryName())
            {
                FScopeLock Lock(&Guard);
                Lines.Add(FString::Printf(TEXT("%8.3f  %s"), FPlatformTime::Seconds() - Origin, Text));
                if (FCString::Strstr(Text, TEXT("Could not arm")))
                    ++ArmErrors;
            }
        }
        virtual bool CanBeUsedOnAnyThread() const override { return true; }
        virtual bool CanBeUsedOnMultipleThreads() const override { return true; }

        int32 TakeArmErrors()
        {
            FScopeLock Lock(&Guard);
            const int32 Count = ArmErrors;
            ArmErrors = 0;
            return Count;
        }
        TArray<FString> GetLines()
        {
            FScopeLock Lock(&Guard);
            return Lines;
        }

    private:
        FCriticalSection Guard;
        double Origin;
        TArray<FString> Lines;
        int32 ArmErrors = 0;
    };

    struct FPlannedRun
    {
        const TCHAR* Label;
        float ClubDelay;
        float ArmDelay;
    };

    const FPlannedRun Plan[] = {
        {TEXT("shipped 0 s / 0.5 s"), 0.0f, 0.5f},
        {TEXT("shipped 0 s / 0.5 s"), 0.0f, 0.5f},
        {TEXT("shipped 0 s / 0.5 s"), 0.0f, 0.5f},
        {TEXT("upstream 2 s / 3 s"), 2.0f, 3.0f},
        {TEXT("upstream 2 s / 3 s"), 2.0f, 3.0f},
    };

    void SetDelays(float Club, float Arm)
    {
        IConsoleManager::Get().FindConsoleVariable(TEXT("olm.Square.ClubDelay"))->Set(Club, ECVF_SetByCode);
        IConsoleManager::Get().FindConsoleVariable(TEXT("olm.Square.ArmDelay"))->Set(Arm, ECVF_SetByCode);
    }

    class FSquareHardwareProbe final : public IAutomationLatentCommand
    {
    public:
        FSquareHardwareProbe(FAutomationTestBase& InTest, bool bInPowerCycle, bool bSkipTiming)
            : Test(InTest), bPowerCycle(bInPowerCycle), Origin(FPlatformTime::Seconds()), Capture(MakeUnique<FLaunchMonitorLogCapture>(Origin))
        {
            RunIndex = bSkipTiming ? UE_ARRAY_COUNT(Plan) : 0;
            Dir = FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("SquareHardware");
            GameInstance.Reset(NewObject<UGameInstance>(GetTransientPackage()));
            Monitor.Reset(NewObject<UOpenLaunchMonitorSubsystem>(GameInstance.Get()));
            Monitor->SetAutoReconnect(true);
            LogOpenLaunchMonitor.SetVerbosity(ELogVerbosity::Verbose);
            GLog->AddOutputDevice(Capture.Get());
            Status(TEXT("RUNNING: timing connects; leave the Square on."));
        }

        virtual ~FSquareHardwareProbe() override
        {
            GLog->RemoveOutputDevice(Capture.Get());
            LogOpenLaunchMonitor.SetVerbosity(ELogVerbosity::Log);
        }

        virtual bool Update() override
        {
            const double Now = FPlatformTime::Seconds();
            const ELaunchMonitorConnectionState State = Monitor->GetConnectionState();
            switch (Step)
            {
            case EStep::StartRun:
                if (RunIndex >= UE_ARRAY_COUNT(Plan))
                {
                    SetDelays(Plan[0].ClubDelay, Plan[0].ArmDelay);
                    Note(TEXT("Cadence: connecting with the shipped delays."));
                    StepStart = Now;
                    Monitor->Connect();
                    Step = EStep::CadenceReady;
                    return false;
                }
                SetDelays(Plan[RunIndex].ClubDelay, Plan[RunIndex].ArmDelay);
                Capture->TakeArmErrors();
                StepStart = Now;
                SeenConnecting = SeenConnected = -1.0;
                Note(FString::Printf(TEXT("Run %d (%s): Connect."), RunIndex + 1, Plan[RunIndex].Label));
                Monitor->Connect();
                Step = EStep::WaitReady;
                return false;

            case EStep::WaitReady:
                if (SeenConnecting < 0 && State == ELaunchMonitorConnectionState::Connecting)
                    SeenConnecting = Now - StepStart;
                if (SeenConnected < 0 && State == ELaunchMonitorConnectionState::Connected)
                    SeenConnected = Now - StepStart;
                if (State == ELaunchMonitorConnectionState::Ready || Now - StepStart > ConnectTimeoutSeconds)
                {
                    const bool bReady = State == ELaunchMonitorConnectionState::Ready;
                    const FString Line = FString::Printf(TEXT("Run %d (%s): %s in %.2f s (device found %.2f s, connected %.2f s); arm retries %d."),
                        RunIndex + 1, Plan[RunIndex].Label, bReady ? TEXT("Ready") : TEXT("NOT READY"), Now - StepStart, SeenConnecting,
                        SeenConnected, Capture->TakeArmErrors());
                    Note(Line);
                    Summary.Add(Line);
                    if (!bReady)
                        Test.AddError(Line);
                    Monitor->Disconnect();
                    ++RunIndex;
                    StepStart = Now;
                    Step = EStep::Settle;
                }
                return false;

            case EStep::Settle:
                // Windows keeps the radio link briefly after Close; let it drop before the next cold connect.
                if (Now - StepStart >= SettleSeconds)
                    Step = EStep::StartRun;
                return false;

            case EStep::CadenceReady:
                if (State == ELaunchMonitorConnectionState::Ready)
                {
                    Note(FString::Printf(TEXT("Cadence: holding Ready for %.0f s."), CadenceHoldSeconds));
                    StepStart = Now;
                    Step = EStep::CadenceHold;
                }
                else if (Now - StepStart > ConnectTimeoutSeconds)
                {
                    Test.AddError(TEXT("Cadence: the Square never became ready."));
                    return Finish();
                }
                return false;

            case EStep::CadenceHold:
                if (State != ELaunchMonitorConnectionState::Ready)
                {
                    const FString Line = FString::Printf(TEXT("Cadence: left Ready after %.2f s of the hold."), Now - StepStart);
                    Note(Line);
                    Summary.Add(Line);
                    Test.AddError(Line);
                    return Finish();
                }
                if (Now - StepStart >= CadenceHoldSeconds)
                {
                    Note(TEXT("Cadence: hold done."));
                    if (!bPowerCycle)
                        return Finish();
                    StepStart = Now;
                    Step = EStep::PowerReady;
                }
                return false;

            case EStep::PowerReady:
                if (State == ELaunchMonitorConnectionState::Ready)
                {
                    StepStart = Now;
                    Note(TEXT("Ready. Waiting for the Square to be switched off."));
                    Status(TEXT("SWITCH_OFF: switch the Square off now."));
                    Step = EStep::PowerOff;
                }
                else if (Now - StepStart > ConnectTimeoutSeconds)
                {
                    Test.AddError(TEXT("Power cycle: the Square never became ready."));
                    return Finish();
                }
                return false;

            case EStep::PowerOff:
                if (State != ELaunchMonitorConnectionState::Ready)
                {
                    LossAt = Now;
                    Note(FString::Printf(TEXT("Link loss detected %.2f s after the switch-off prompt."), Now - StepStart));
                    Status(TEXT("SWITCH_ON: switch the Square back on now."));
                    Step = EStep::PowerOn;
                }
                else if (Now - StepStart > PowerStepTimeoutSeconds)
                {
                    Test.AddError(TEXT("Power cycle: no link loss was detected."));
                    return Finish();
                }
                return false;

            case EStep::PowerOn:
                if (State == ELaunchMonitorConnectionState::Ready)
                {
                    const FString Line = FString::Printf(TEXT("Power cycle: Ready again %.2f s after the link loss (see the log for the attempt that succeeded)."), Now - LossAt);
                    Note(Line);
                    Summary.Add(Line);
                    return Finish();
                }
                if (Now - LossAt > PowerStepTimeoutSeconds)
                {
                    Test.AddError(TEXT("Power cycle: the Square did not reconnect."));
                    return Finish();
                }
                return false;
            }
            return true;
        }

    private:
        enum class EStep { StartRun, WaitReady, Settle, CadenceReady, CadenceHold, PowerReady, PowerOff, PowerOn };

        void Note(const FString& Line)
        {
            UE_LOG(LogOpenLaunchMonitor, Display, TEXT("[hardware] %s"), *Line);
        }

        void Status(const TCHAR* Text)
        {
            FFileHelper::SaveStringToFile(FString(Text), *(Dir / TEXT("status.txt")));
        }

        bool Finish()
        {
            Monitor->Disconnect();
            SetDelays(Plan[0].ClubDelay, Plan[0].ArmDelay);
            Status(TEXT("DONE"));
            FString Report = TEXT("Summary\n") + FString::Join(Summary, TEXT("\n")) + TEXT("\n\nLog (seconds since start)\n") +
                             FString::Join(Capture->GetLines(), TEXT("\n")) + TEXT("\n");
            FFileHelper::SaveStringToFile(Report, *(Dir / TEXT("results.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
            return true;
        }

        FAutomationTestBase& Test;
        bool bPowerCycle;
        double Origin;
        TUniquePtr<FLaunchMonitorLogCapture> Capture;
        FString Dir;
        TStrongObjectPtr<UGameInstance> GameInstance;
        TStrongObjectPtr<UOpenLaunchMonitorSubsystem> Monitor;
        EStep Step = EStep::StartRun;
        int32 RunIndex = 0;
        double StepStart = 0.0;
        double SeenConnecting = -1.0;
        double SeenConnected = -1.0;
        double LossAt = 0.0;
        TArray<FString> Summary;
    };
    /**
     * What a Disconnect does to the device (OLM_SQUARE_DISCONNECT=1). Connects its own session, checks
     * whether the Square advertises while connected, disarms it (status DISARMED) and, 8 s later,
     * disconnects (status DISCONNECTED), timing when it advertises again with a second transport.
     * OLM_SQUARE_GRACEFUL=0 turns the Windows link release off for an A/B comparison.
     */
    class FSquareDisconnectProbe final : public IAutomationLatentCommand
    {
    public:
        explicit FSquareDisconnectProbe(FAutomationTestBase& InTest)
            : Test(InTest), Origin(FPlatformTime::Seconds()), Capture(MakeUnique<FLaunchMonitorLogCapture>(Origin))
        {
            Dir = FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("SquareHardware");
            const bool bGraceful = FPlatformMisc::GetEnvironmentVariable(TEXT("OLM_SQUARE_GRACEFUL")) != TEXT("0");
            IConsoleManager::Get().FindConsoleVariable(TEXT("olm.Bluetooth.GracefulRelease"))->Set(bGraceful ? 1 : 0, ECVF_SetByCode);
            LogOpenLaunchMonitor.SetVerbosity(ELogVerbosity::Verbose);
            GLog->AddOutputDevice(Capture.Get());
            Summary.Add(FString::Printf(TEXT("Graceful release: %s"), bGraceful ? TEXT("on") : TEXT("off")));
            Status(TEXT("RUNNING: connecting; the red lights should come on."));
            const TSharedPtr<IGattTransport> Transport = CreatePlatformGattTransport();
            if (Transport)
            {
                Session = MakeShared<FSquareSession>(Transport.ToSharedRef(), MakeShared<FTickerSquareScheduler>());
                Session->Initialize();
                Session->Connect();
            }
            StepStart = Origin;
        }

        virtual ~FSquareDisconnectProbe() override
        {
            GLog->RemoveOutputDevice(Capture.Get());
            LogOpenLaunchMonitor.SetVerbosity(ELogVerbosity::Log);
            IConsoleManager::Get().FindConsoleVariable(TEXT("olm.Bluetooth.GracefulRelease"))->Set(1, ECVF_SetByCode);
        }

        virtual bool Update() override
        {
            const double Now = FPlatformTime::Seconds();
            if (!Session)
            {
                Test.AddError(TEXT("No Bluetooth transport on this platform."));
                return true;
            }
            switch (Step)
            {
            case EStep::WaitReady:
                if (Session->GetStatus() == ESquareSessionStatus::Ready)
                {
                    Add(FString::Printf(TEXT("Ready in %.2f s."), Now - StepStart));
                    StartScanner();
                    StepStart = Now;
                    Step = EStep::AdvertiseCheck;
                }
                else if (Now - StepStart > ConnectTimeoutSeconds)
                {
                    Test.AddError(TEXT("The Square never became ready."));
                    return Finish();
                }
                return false;

            case EStep::AdvertiseCheck:
                if (Now - StepStart >= 3.0)
                {
                    Add(SeenAt >= 0.0 ? FString::Printf(TEXT("Advertises while connected: yes (seen %.2f s into the check)."), SeenAt - StepStart)
                                      : FString(TEXT("Advertises while connected: no (3 s scan).")));
                    StopScanner();
                    Status(TEXT("DISARMED: ball detection stopped; are the red lights off?"));
                    Add(TEXT("Disarm sent."));
                    Session->Disarm();
                    StepStart = Now;
                    Step = EStep::Disarmed;
                }
                return false;

            case EStep::Disarmed:
                if (Now - StepStart >= 8.0)
                {
                    Status(TEXT("DISCONNECTED: link released; watch the red lights."));
                    Add(TEXT("Disconnect called."));
                    Session->Disconnect();
                    StartScanner();
                    StepStart = Now;
                    Step = EStep::Disconnected;
                }
                return false;

            case EStep::Disconnected:
                if (SeenAt >= 0.0)
                {
                    Add(FString::Printf(TEXT("Advertising again %.2f s after Disconnect."), SeenAt - StepStart));
                    return Finish();
                }
                if (Now - StepStart > 30.0)
                {
                    Add(TEXT("Not advertising within 30 s of Disconnect."));
                    return Finish();
                }
                return false;
            }
            return true;
        }

    private:
        enum class EStep { WaitReady, AdvertiseCheck, Disarmed, Disconnected };

        void StartScanner()
        {
            SeenAt = -1.0;
            Scanner = CreatePlatformGattTransport();
            if (Scanner)
            {
                Scanner->OnDeviceDiscovered = [this](const FGattDeviceInfo&)
                {
                    if (SeenAt < 0.0)
                        SeenAt = FPlatformTime::Seconds();
                };
                Scanner->StartScan(TEXT("SquareGolf"));
            }
        }

        void StopScanner()
        {
            if (Scanner)
            {
                Scanner->OnDeviceDiscovered = nullptr;
                Scanner->StopScan();
                Scanner.Reset();
            }
        }

        void Add(const FString& Line)
        {
            UE_LOG(LogOpenLaunchMonitor, Display, TEXT("[hardware] %s"), *Line);
            Summary.Add(FString::Printf(TEXT("%8.3f  %s"), FPlatformTime::Seconds() - Origin, *Line));
        }

        void Status(const TCHAR* Text)
        {
            FFileHelper::SaveStringToFile(FString(Text), *(Dir / TEXT("status.txt")));
        }

        bool Finish()
        {
            StopScanner();
            if (Session)
                Session->Disconnect();
            Status(TEXT("DONE"));
            const FString Report = TEXT("Summary\n") + FString::Join(Summary, TEXT("\n")) + TEXT("\n\nLog (seconds since start)\n") +
                                   FString::Join(Capture->GetLines(), TEXT("\n")) + TEXT("\n");
            FFileHelper::SaveStringToFile(Report, *(Dir / TEXT("results.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
            return true;
        }

        FAutomationTestBase& Test;
        double Origin;
        TUniquePtr<FLaunchMonitorLogCapture> Capture;
        FString Dir;
        TSharedPtr<FSquareSession> Session;
        TSharedPtr<IGattTransport> Scanner;
        EStep Step = EStep::WaitReady;
        double StepStart = 0.0;
        double SeenAt = -1.0;
        TArray<FString> Summary;
    };
} // namespace

/** Bluetooth warnings (link lost, heartbeat failures) are expected during a power cycle, not failures. */
class FSquareHardwareTestBase : public FAutomationTestBase
{
public:
    FSquareHardwareTestBase(const FString& InName, bool bInComplexTask) : FAutomationTestBase(InName, bInComplexTask) {}
    virtual bool SuppressLogWarnings() override { return true; }
};

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FSquareHardwareTest, FSquareHardwareTestBase, "UOpenGolf.LaunchMonitor.Hardware.Square",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSquareHardwareTest::RunTest(const FString&)
{
    if (FPlatformMisc::GetEnvironmentVariable(TEXT("OLM_SQUARE_HARDWARE")) != TEXT("1"))
    {
        AddInfo(TEXT("Skipped: set OLM_SQUARE_HARDWARE=1 with a Square Home switched on to run it."));
        return true;
    }
    if (FPlatformMisc::GetEnvironmentVariable(TEXT("OLM_SQUARE_DISCONNECT")) == TEXT("1"))
    {
        ADD_LATENT_AUTOMATION_COMMAND(FSquareDisconnectProbe(*this));
        return true;
    }
    const bool bPowerCycle = FPlatformMisc::GetEnvironmentVariable(TEXT("OLM_SQUARE_POWER_CYCLE")) == TEXT("1");
    const bool bSkipTiming = FPlatformMisc::GetEnvironmentVariable(TEXT("OLM_SQUARE_SKIP_TIMING")) == TEXT("1");
    ADD_LATENT_AUTOMATION_COMMAND(FSquareHardwareProbe(*this, bPowerCycle, bSkipTiming));
    return true;
}
#endif
