// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "OpenGolfOnlineConfig.h"
class FSocket;

class FOpenGolfBrowserLogin
{
public:
    ~FOpenGolfBrowserLogin();
    bool Start(const OpenGolfOnline::FConfig& Config, bool bCreateAccount);
    bool Poll(FString& Code, FString& Error);
    static bool ParseCallback(const FString& RequestLine, const FString& ExpectedState, FString& Code, FString& Error);
    FString Verifier, State, Redirect;
private:
    FSocket* Listener = nullptr;
    FSocket* Client = nullptr;
    FString Input;
    double ConnectedAt = 0;
    void CloseClient();
};
