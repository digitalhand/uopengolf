// SPDX-License-Identifier: MIT
#include "TokenStore.h"

#if PLATFORM_LINUX
#include <dlfcn.h>
#elif PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <wincred.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace OpenGolfOnline
{
#if PLATFORM_LINUX
    namespace
    {
        class FSecretService
        {
        public:
            void* Library = nullptr;
            void* Schema = nullptr;
            void* (*NewSchema)(const char*, int, ...) = nullptr;
            void (*FreeSchema)(void*) = nullptr;
            char* (*Lookup)(const void*, void*, void**, ...) = nullptr;
            int (*Store)(const void*, const char*, const char*, const char*, void*, void**, ...) = nullptr;
            int (*Clear)(const void*, void*, void**, ...) = nullptr;
            void (*FreePassword)(char*) = nullptr;
            void (*FreeError)(void*) = nullptr;

            FSecretService()
            {
                // GLib retains type names from this library, so keep it loaded for the process.
                static void* const ResidentLibrary = dlopen("libsecret-1.so.0", RTLD_NOW | RTLD_LOCAL);
                Library = ResidentLibrary;
                if (!Library) return;
                NewSchema = reinterpret_cast<decltype(NewSchema)>(dlsym(Library, "secret_schema_new"));
                FreeSchema = reinterpret_cast<decltype(FreeSchema)>(dlsym(Library, "secret_schema_unref"));
                Lookup = reinterpret_cast<decltype(Lookup)>(dlsym(Library, "secret_password_lookup_sync"));
                Store = reinterpret_cast<decltype(Store)>(dlsym(Library, "secret_password_store_sync"));
                Clear = reinterpret_cast<decltype(Clear)>(dlsym(Library, "secret_password_clear_sync"));
                FreePassword = reinterpret_cast<decltype(FreePassword)>(dlsym(Library, "secret_password_free"));
                FreeError = reinterpret_cast<decltype(FreeError)>(dlsym(Library, "g_error_free"));
                if (NewSchema && FreeSchema && Lookup && Store && Clear && FreePassword && FreeError)
                    Schema = NewSchema("org.opengolf.native.session", 0, "application", 0, nullptr);
            }
            ~FSecretService()
            {
                if (Schema) FreeSchema(Schema);
            }
        };
    }
#endif

    EStoreResult ReadRefreshToken(const FString& StorageName, FString& Token)
    {
        Token.Empty();
#if PLATFORM_LINUX
        FSecretService Service;
        if (!Service.Schema) return EStoreResult::Unavailable;
        void* Error = nullptr;
        char* Password = Service.Lookup(Service.Schema, nullptr, &Error, "application", TCHAR_TO_UTF8(*StorageName), nullptr);
        if (Password) { Token = UTF8_TO_TCHAR(Password); Service.FreePassword(Password); }
        if (Error) { Service.FreeError(Error); Token.Empty(); return EStoreResult::Unavailable; }
        return Token.IsEmpty() ? EStoreResult::Empty : EStoreResult::Ok;
#elif PLATFORM_WINDOWS
        PCREDENTIALW Credential = nullptr;
        if (!CredReadW(*(StorageName + TEXT("/Auth0/refresh")), CRED_TYPE_GENERIC, 0, &Credential))
            return GetLastError() == ERROR_NOT_FOUND ? EStoreResult::Empty : EStoreResult::Unavailable;
        const int32 Count = Credential->CredentialBlobSize;
        if (Count > 0 && Count <= CRED_MAX_CREDENTIAL_BLOB_SIZE)
        {
            const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Credential->CredentialBlob), Count);
            Token = FString(Text.Length(), Text.Get());
        }
        CredFree(Credential);
        return Token.IsEmpty() ? EStoreResult::Empty : EStoreResult::Ok;
#else
        return EStoreResult::Unavailable;
#endif
    }

    EStoreResult WriteRefreshToken(const FString& StorageName, const FString& Token)
    {
        if (Token.IsEmpty()) return ClearRefreshToken(StorageName);
#if PLATFORM_LINUX
        FSecretService Service;
        if (!Service.Schema) return EStoreResult::Unavailable;
        void* Error = nullptr;
        const FTCHARToUTF8 Password(*Token);
        const int Saved = Service.Store(Service.Schema, "default", TCHAR_TO_UTF8(*(StorageName + TEXT(" sign-in"))), Password.Get(),
            nullptr, &Error, "application", TCHAR_TO_UTF8(*StorageName), nullptr);
        if (Error) Service.FreeError(Error);
        return Saved && !Error ? EStoreResult::Ok : EStoreResult::Unavailable;
#elif PLATFORM_WINDOWS
        const FTCHARToUTF8 Password(*Token);
        if (Password.Length() > CRED_MAX_CREDENTIAL_BLOB_SIZE) return EStoreResult::Unavailable;
        const FString TargetName = StorageName + TEXT("/Auth0/refresh");
        CREDENTIALW Credential{};
        Credential.Type = CRED_TYPE_GENERIC;
        Credential.TargetName = const_cast<LPWSTR>(*TargetName);
        Credential.UserName = const_cast<LPWSTR>(*StorageName);
        Credential.CredentialBlobSize = Password.Length();
        Credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<ANSICHAR*>(Password.Get()));
        Credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
        return CredWriteW(&Credential, 0) ? EStoreResult::Ok : EStoreResult::Unavailable;
#else
        return EStoreResult::Unavailable;
#endif
    }

    EStoreResult ClearRefreshToken(const FString& StorageName)
    {
#if PLATFORM_LINUX
        FSecretService Service;
        if (!Service.Schema) return EStoreResult::Unavailable;
        void* Error = nullptr;
        Service.Clear(Service.Schema, nullptr, &Error, "application", TCHAR_TO_UTF8(*StorageName), nullptr);
        if (Error) { Service.FreeError(Error); return EStoreResult::Unavailable; }
        return EStoreResult::Ok;
#elif PLATFORM_WINDOWS
        if (CredDeleteW(*(StorageName + TEXT("/Auth0/refresh")), CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND)
            return EStoreResult::Ok;
        return EStoreResult::Unavailable;
#else
        return EStoreResult::Unavailable;
#endif
    }
}
