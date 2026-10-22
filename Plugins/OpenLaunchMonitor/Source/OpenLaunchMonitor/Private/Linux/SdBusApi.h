// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

#if PLATFORM_LINUX

#include <cstddef>
#include <cstdint>

// sd-bus types and prototypes declared locally, so the build needs no systemd headers.
// Layouts and signatures follow systemd's public sd-bus.h (stable ABI of libsystemd.so.0).
struct sd_bus;
struct sd_bus_message;
struct sd_bus_slot;
struct sd_bus_error
{
    const char* name;
    const char* message;
    int _need_free;
};
using sd_bus_message_handler_t = int (*)(sd_bus_message* Message, void* UserData, sd_bus_error* RetError);

/** The sd-bus entry points this plugin uses, resolved from libsystemd at runtime. */
struct FSdBusApi
{
    /** Loads LibraryName ("libsystemd.so.0" in production); null with OutError when it or a symbol is missing. */
    static TSharedPtr<const FSdBusApi> Load(const TCHAR* LibraryName, FString& OutError);
    ~FSdBusApi();

    int (*open_system)(sd_bus** Bus) = nullptr;
    sd_bus* (*flush_close_unref)(sd_bus* Bus) = nullptr;
    int (*process)(sd_bus* Bus, sd_bus_message** Message) = nullptr;
    int (*wait)(sd_bus* Bus, uint64_t TimeoutUsec) = nullptr;
    int (*call_async)(sd_bus* Bus, sd_bus_slot** Slot, sd_bus_message* Message, sd_bus_message_handler_t Callback, void* UserData,
        uint64_t TimeoutUsec) = nullptr;
    int (*match_signal)(sd_bus* Bus, sd_bus_slot** Slot, const char* Sender, const char* Path, const char* Interface, const char* Member,
        sd_bus_message_handler_t Callback, void* UserData) = nullptr;
    sd_bus_slot* (*slot_unref)(sd_bus_slot* Slot) = nullptr;
    int (*message_new_method_call)(sd_bus* Bus, sd_bus_message** Message, const char* Destination, const char* Path, const char* Interface,
        const char* Member) = nullptr;
    sd_bus_message* (*message_ref)(sd_bus_message* Message) = nullptr;
    sd_bus_message* (*message_unref)(sd_bus_message* Message) = nullptr;
    const sd_bus_error* (*message_get_error)(sd_bus_message* Message) = nullptr;
    int (*message_append_basic)(sd_bus_message* Message, char Type, const void* Value) = nullptr;
    int (*message_append_array)(sd_bus_message* Message, char Type, const void* Data, size_t Size) = nullptr;
    int (*message_open_container)(sd_bus_message* Message, char Type, const char* Contents) = nullptr;
    int (*message_close_container)(sd_bus_message* Message) = nullptr;
    int (*message_read_basic)(sd_bus_message* Message, char Type, void* Value) = nullptr;
    int (*message_read_array)(sd_bus_message* Message, char Type, const void** Data, size_t* Size) = nullptr;
    int (*message_enter_container)(sd_bus_message* Message, char Type, const char* Contents) = nullptr;
    int (*message_exit_container)(sd_bus_message* Message) = nullptr;
    int (*message_peek_type)(sd_bus_message* Message, char* Type, const char** Contents) = nullptr;
    int (*message_skip)(sd_bus_message* Message, const char* Types) = nullptr;
    int (*message_set_expect_reply)(sd_bus_message* Message, int ExpectReply) = nullptr;
    int (*send)(sd_bus* Bus, sd_bus_message* Message, uint64_t* Cookie) = nullptr;

private:
    void* Library = nullptr;
};

#endif // PLATFORM_LINUX
