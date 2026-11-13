// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "BrowserLogin.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfCallbackTest,"OpenGolf.Online.BrowserCallback",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOpenGolfCallbackTest::RunTest(const FString&)
{
    FString Code,Error;
    auto Parse=[&](const FString& Line){return FOpenGolfBrowserLogin::ParseCallback(Line,TEXT("expected"),Code,Error);};
    TestFalse(TEXT("Forged state ignored"),Parse(TEXT("GET /callback?state=forged&code=attack HTTP/1.1")));
    TestFalse(TEXT("Duplicate state ignored"),Parse(TEXT("GET /callback?state=expected&state=forged&code=attack HTTP/1.1")));
    TestFalse(TEXT("Other callback paths ignored"),Parse(TEXT("GET /other?state=expected&code=attack HTTP/1.1")));
    TestFalse(TEXT("Non-GET callback ignored"),Parse(TEXT("POST /callback?state=expected&code=attack HTTP/1.1")));
    TestTrue(TEXT("Valid callback accepted"),Parse(TEXT("GET /callback?code=valid%2Bcode&state=expected HTTP/1.1")));
    TestEqual(TEXT("Code decoded once"),Code,FString(TEXT("valid+code")));
    TestTrue(TEXT("Valid callback has no error"),Error.IsEmpty());
    TestTrue(TEXT("Authenticated denial ends attempt"),Parse(TEXT("GET /callback?state=expected&error=access_denied HTTP/1.1")));
    TestTrue(TEXT("Denial reports a safe error"),!Error.IsEmpty());
    return true;
}

#endif
