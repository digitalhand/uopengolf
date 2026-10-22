// SPDX-License-Identifier: MIT
#include "Bluetooth/GattTransport.h"

#if PLATFORM_WINDOWS
TSharedPtr<IGattTransport> CreateWindowsGattTransport();
#elif PLATFORM_LINUX
TSharedPtr<IGattTransport> CreateLinuxGattTransport();
#endif

TSharedPtr<IGattTransport> CreatePlatformGattTransport()
{
#if PLATFORM_WINDOWS
    return CreateWindowsGattTransport();
#elif PLATFORM_LINUX
    return CreateLinuxGattTransport();
#else
    return nullptr;
#endif
}
