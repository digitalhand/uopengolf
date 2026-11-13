// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Misc/FileHelper.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#else
#include <cstdio>
#include <sys/stat.h>
#endif

namespace OpenGolfOnline
{
    inline bool WriteAtomically(const FString& Path, const FString& Contents)
    {
        const FString Temp = Path + TEXT(".tmp");
        if (!FFileHelper::SaveStringToFile(Contents,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return false;
#if PLATFORM_WINDOWS
        return MoveFileExW(*Temp,*Path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH) != 0;
#else
        const FTCHARToUTF8 From(*Temp), To(*Path);
        if (chmod(From.Get(),S_IRUSR|S_IWUSR) != 0) return false;
        return std::rename(From.Get(),To.Get()) == 0;
#endif
    }
}
