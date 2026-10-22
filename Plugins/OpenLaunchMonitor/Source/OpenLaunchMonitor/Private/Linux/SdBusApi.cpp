// SPDX-License-Identifier: MIT
#include "Linux/SdBusApi.h"

#if PLATFORM_LINUX

#include <dlfcn.h>

namespace
{
    template <typename FunctionType>
    bool Resolve(void* Library, const char* Symbol, FunctionType& OutFunction, FString& OutError)
    {
        OutFunction = reinterpret_cast<FunctionType>(dlsym(Library, Symbol));
        if (!OutFunction)
        {
            OutError = FString::Printf(TEXT("Bluetooth on Linux needs systemd's libsystemd.so.0 (missing %s)."), UTF8_TO_TCHAR(Symbol));
        }
        return OutFunction != nullptr;
    }
} // namespace

TSharedPtr<const FSdBusApi> FSdBusApi::Load(const TCHAR* LibraryName, FString& OutError)
{
    void* Library = dlopen(TCHAR_TO_UTF8(LibraryName), RTLD_NOW | RTLD_LOCAL);
    if (!Library)
    {
        const char* Reason = dlerror();
        OutError = FString::Printf(TEXT("Bluetooth on Linux needs systemd's libsystemd.so.0 (%s)."),
            Reason ? UTF8_TO_TCHAR(Reason) : LibraryName);
        return nullptr;
    }

    const TSharedRef<FSdBusApi> Api = MakeShared<FSdBusApi>();
    Api->Library = Library;
#define OLM_RESOLVE(Field) Resolve(Library, "sd_bus_" #Field, Api->Field, OutError)
    const bool bResolved = OLM_RESOLVE(open_system) && OLM_RESOLVE(flush_close_unref) && OLM_RESOLVE(process) && OLM_RESOLVE(wait) &&
                           OLM_RESOLVE(call_async) && OLM_RESOLVE(match_signal) && OLM_RESOLVE(slot_unref) &&
                           OLM_RESOLVE(message_new_method_call) && OLM_RESOLVE(message_ref) && OLM_RESOLVE(message_unref) &&
                           OLM_RESOLVE(message_get_error) && OLM_RESOLVE(message_append_basic) && OLM_RESOLVE(message_append_array) &&
                           OLM_RESOLVE(message_open_container) && OLM_RESOLVE(message_close_container) && OLM_RESOLVE(message_read_basic) &&
                           OLM_RESOLVE(message_read_array) && OLM_RESOLVE(message_enter_container) && OLM_RESOLVE(message_exit_container) &&
                           OLM_RESOLVE(message_peek_type) && OLM_RESOLVE(message_skip) &&
                           OLM_RESOLVE(message_set_expect_reply) && OLM_RESOLVE(send);
#undef OLM_RESOLVE
    return bResolved ? TSharedPtr<const FSdBusApi>(Api) : nullptr;
}

FSdBusApi::~FSdBusApi()
{
    if (Library)
    {
        dlclose(Library);
    }
}

#endif // PLATFORM_LINUX
