// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Online/OnlineContract.h"
#include "OnlineTransport.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOnlineTransportBoundaryTest, "UOpenGolf.Online.TransportBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOnlineTransportBoundaryTest::RunTest(const FString&)
{
    using namespace OpenGolfOnline;
    const auto Config = FConfig::Load();
    TestTrue(TEXT("Host project online configuration is valid"), Config.IsValid());
    AddInfo(FString::Printf(TEXT("Config lengths: authority=%d client=%d audience=%d api=%d name=%d storage=%d ports=%d"),Config.Authority.Len(),Config.ClientId.Len(),Config.Audience.Len(),Config.ApiOrigin.Len(),Config.ApplicationName.Len(),Config.StorageName.Len(),Config.CallbackPorts.Num()));
    const FString Base = Config.ApiOrigin + Config.MePath;
    TestTrue(TEXT("Authenticated profile read allowed"), IsAllowedRequest(Config, Base, TEXT("GET"), TEXT("token")));
    TestTrue(TEXT("Device flow uses no bearer"), IsAllowedRequest(Config, Config.Authority + TEXT("oauth/device/code"), TEXT("POST"), TEXT("")));
    TestTrue(TEXT("Existing history query allowed"), IsAllowedRequest(Config, Base + TEXT("/clubs/7i/shots?days=90&limit=200"), TEXT("GET"), TEXT("token")));
    TestFalse(TEXT("Profile read needs bearer"), IsAllowedRequest(Config, Base, TEXT("GET"), TEXT("")));
    TestFalse(TEXT("No bearer sent to token endpoint"), IsAllowedRequest(Config, Config.Authority + TEXT("oauth/token"), TEXT("POST"), TEXT("token")));
    TestFalse(TEXT("Reject origin suffix attack"), IsAllowedRequest(Config, TEXT("https://server.digitalhand.net.evil.test/api/v1/ogolf/me"), TEXT("GET"), TEXT("token")));
    TestFalse(TEXT("Reject user-info authority attack"), IsAllowedRequest(Config, TEXT("https://server.digitalhand.net@evil.test/api/v1/ogolf/me"), TEXT("GET"), TEXT("token")));
    TestFalse(TEXT("Reject unencrypted requests"), IsAllowedRequest(Config, TEXT("http://server.digitalhand.net/api/v1/ogolf/me"), TEXT("GET"), TEXT("token")));
    TestFalse(TEXT("Reject header injection"), IsAllowedRequest(Config, Base, TEXT("GET"), TEXT("token\r\nX-Injected: yes")));
    TestFalse(TEXT("Reject encoded traversal"), IsAllowedRequest(Config, Base + TEXT("/%2e%2e/admin"), TEXT("GET"), TEXT("token")));
    TestFalse(TEXT("Reject unrecognized methods"), IsAllowedRequest(Config, Base, TEXT("DELETE"), TEXT("token")));
    TestFalse(TEXT("Reject additional query arguments"), IsAllowedRequest(Config, Base + TEXT("/clubs/carry?days=90&player_id=other"), TEXT("GET"), TEXT("token")));
    return true;
}

#endif
