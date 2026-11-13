// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "TokenStore.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Async/Async.h"
#include <atomic>

namespace
{
    struct FKeyringProbe { std::atomic<bool> Done{false}; bool Ok=false; };
    class FWaitForKeyring : public IAutomationLatentCommand
    {
    public:
        FWaitForKeyring(FAutomationTestBase* InTest,TSharedRef<FKeyringProbe,ESPMode::ThreadSafe> InState):Test(InTest),State(InState){}
        bool Update() override
        {
            if(!State->Done.load()&&GetCurrentRunTime()<30)return false;
            Test->TestTrue(TEXT("Secure store can save, read and remove an isolated test credential"),State->Done.load()&&State->Ok);
            return true;
        }
    private:FAutomationTestBase* Test;TSharedRef<FKeyringProbe,ESPMode::ThreadSafe> State;
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfOnlineKeyringTest,"OpenGolf.Online.Keyring",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOpenGolfOnlineKeyringTest::RunTest(const FString&)
{
    using namespace OpenGolfOnline;
    if(!FParse::Param(FCommandLine::Get(),TEXT("OpenGolfOnlineKeyring")))
    {AddInfo(TEXT("OS credential-store probe requires -OpenGolfOnlineKeyring."));return true;}
    auto State=MakeShared<FKeyringProbe,ESPMode::ThreadSafe>();
    Async(EAsyncExecution::ThreadPool,[State]
    {
        const FString Name=TEXT("OpenGolfOnlineTest-")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
        const FString Value=FGuid::NewGuid().ToString();FString Read;
        const bool Stored=WriteRefreshToken(Name,Value)==EStoreResult::Ok;
        const bool Loaded=Stored&&ReadRefreshToken(Name,Read)==EStoreResult::Ok&&Read==Value;
        const bool Removed=ClearRefreshToken(Name)==EStoreResult::Ok;
        State->Ok=Loaded&&Removed&&ReadRefreshToken(Name,Read)==EStoreResult::Empty;
        State->Done.store(true);
    });
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FWaitForKeyring>(this,State));return true;
}

#endif
