// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "OnlineTransport.h"
#include "OnlineJson.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
    struct FLiveProbe { bool Done=false; bool Ok=false; int32 Status=0; };
    class FWaitForLiveProbe : public IAutomationLatentCommand
    {
    public:
        FWaitForLiveProbe(FAutomationTestBase* InTest,TSharedRef<FLiveProbe> InState,OpenGolfOnline::FCancelHandle InCancel)
            : Test(InTest),State(InState),Cancel(InCancel) {}
        bool Update() override
        {
            if(!State->Done && GetCurrentRunTime()<30)return false;
            if(!State->Done)Cancel->Cancel();
            Test->TestTrue(TEXT("Auth0 accepts this native app's device authorization request over verified TLS"),State->Done&&State->Ok);
            Test->AddInfo(FString::Printf(TEXT("Device authorization HTTP status: %d. Codes were kept out of the report."),State->Status));
            return true;
        }
    private:
        FAutomationTestBase* Test;TSharedRef<FLiveProbe> State;OpenGolfOnline::FCancelHandle Cancel;
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfOnlineLiveTest,"OpenGolf.Online.LiveConfiguration",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOpenGolfOnlineLiveTest::RunTest(const FString&)
{
    using namespace OpenGolfOnline;
    if(!FParse::Param(FCommandLine::Get(),TEXT("OpenGolfOnlineLive")))
    { AddInfo(TEXT("Live Auth0 probe requires -OpenGolfOnlineLive; no network request made.")); return true; }
    auto Config=FConfig::Load();
    if(!TestTrue(TEXT("Host has valid public settings"),Config.IsValid()))return false;
    auto State=MakeShared<FLiveProbe>();
    auto Cancel=Request(Config,Config.Authority+TEXT("oauth/device/code"),TEXT("POST"),
        Form({{TEXT("client_id"),Config.ClientId},{TEXT("audience"),Config.Audience},{TEXT("scope"),TEXT("openid profile offline_access")}}),{},
        [State,Config](FHttpResult Reply)
        {
            State->Done=true;State->Status=Reply.Status;
            const auto Data=Object(Reply.Body);
            State->Ok=SuccessReply(Reply)&&!Text(Data,TEXT("device_code")).IsEmpty()
                &&Text(Data,TEXT("verification_uri_complete")).StartsWith(Config.Authority,ESearchCase::CaseSensitive);
        },true);
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FWaitForLiveProbe>(this,State,Cancel));
    return true;
}

#endif
