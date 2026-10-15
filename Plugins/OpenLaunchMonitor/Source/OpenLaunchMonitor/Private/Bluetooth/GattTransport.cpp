// SPDX-License-Identifier: MIT
#include "Bluetooth/GattTransport.h"

#if PLATFORM_WINDOWS
TSharedPtr<IGattTransport> CreateWindowsGattTransport();
#endif

TSharedPtr<IGattTransport> CreatePlatformGattTransport()
{
#if PLATFORM_WINDOWS
    return CreateWindowsGattTransport();
#else
    // The Linux BlueZ transport is planned; see Docs/LaunchMonitorIntegration.md.
    return nullptr;
#endif
}
