// SPDX-License-Identifier: MIT
#include "UOpenGolfPlayerController.h"

#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "OpenLaunchMonitorSubsystem.h"

namespace
{
    UOpenLaunchMonitorSubsystem* GetLaunchMonitor(const APlayerController& Controller)
    {
        const UGameInstance* GameInstance = Controller.GetGameInstance();
        return GameInstance ? GameInstance->GetSubsystem<UOpenLaunchMonitorSubsystem>() : nullptr;
    }
} // namespace

void AUOpenGolfPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::C, IE_Pressed, this, &ThisClass::ConnectLaunchMonitor);
    InputComponent->BindKey(EKeys::X, IE_Pressed, this, &ThisClass::DisconnectLaunchMonitor);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::InjectReplayShot);
    InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ThisClass::SelectNextClub);
}

void AUOpenGolfPlayerController::ConnectLaunchMonitor()
{
    if (UOpenLaunchMonitorSubsystem* LaunchMonitor = GetLaunchMonitor(*this))
    {
        LaunchMonitor->Connect();
    }
}

void AUOpenGolfPlayerController::DisconnectLaunchMonitor()
{
    if (UOpenLaunchMonitorSubsystem* LaunchMonitor = GetLaunchMonitor(*this))
    {
        LaunchMonitor->Disconnect();
    }
}

void AUOpenGolfPlayerController::InjectReplayShot()
{
    if (UOpenLaunchMonitorSubsystem* LaunchMonitor = GetLaunchMonitor(*this))
    {
        LaunchMonitor->InjectReplayShot();
    }
}

void AUOpenGolfPlayerController::SelectNextClub()
{
    if (UOpenLaunchMonitorSubsystem* LaunchMonitor = GetLaunchMonitor(*this))
    {
        const int32 ClubCount = static_cast<int32>(ELaunchMonitorClub::Putter) + 1;
        LaunchMonitor->SetClub(static_cast<ELaunchMonitorClub>((static_cast<int32>(LaunchMonitor->GetClub()) + 1) % ClubCount));
    }
}
