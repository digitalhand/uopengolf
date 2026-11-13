// SPDX-License-Identifier: MIT
#include "UI/Common/FolderPicker.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <shobjidl.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace FolderPicker
{
    bool Supported() { return PLATFORM_WINDOWS != 0; }

    bool Pick(const FString& Title, const FString& Start, FString& OutFolder)
    {
#if PLATFORM_WINDOWS
        // The shell's IFileOpenDialog, as the editor's DesktopPlatform uses, without that developer-only module.
        const bool bComStarted = FWindowsPlatformMisc::CoInitialize();
        bool bPicked = false;
        IFileOpenDialog* Dialog = nullptr;
        if (SUCCEEDED(CoCreateInstance(__uuidof(FileOpenDialog), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Dialog))))
        {
            DWORD Options = 0;
            Dialog->GetOptions(&Options);
            Dialog->SetOptions(Options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
            Dialog->SetTitle(*Title);
            FString StartFolder = FPaths::ConvertRelativePathToFull(Start);
            FPaths::MakePlatformFilename(StartFolder);
            IShellItem* StartItem = nullptr;
            if (!StartFolder.IsEmpty() && SUCCEEDED(SHCreateItemFromParsingName(*StartFolder, nullptr, IID_PPV_ARGS(&StartItem))))
            {
                Dialog->SetFolder(StartItem);
                StartItem->Release();
            }
            HWND Owner = nullptr;
            if (FSlateApplication::IsInitialized())
                Owner = static_cast<HWND>(const_cast<void*>(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)));
            IShellItem* Result = nullptr;
            if (SUCCEEDED(Dialog->Show(Owner)) && SUCCEEDED(Dialog->GetResult(&Result)))
            {
                PWSTR Path = nullptr;
                if (SUCCEEDED(Result->GetDisplayName(SIGDN_FILESYSPATH, &Path)))
                {
                    OutFolder = Path;
                    CoTaskMemFree(Path);
                    bPicked = true;
                }
                Result->Release();
            }
            Dialog->Release();
        }
        if (bComStarted) FWindowsPlatformMisc::CoUninitialize();
        return bPicked;
#else
        return false;
#endif
    }
}
