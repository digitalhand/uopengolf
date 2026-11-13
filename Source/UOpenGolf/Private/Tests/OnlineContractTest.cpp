// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Online/OnlineContract.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOnlineContractTest, "UOpenGolf.Online.Contract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOnlineContractTest::RunTest(const FString&)
{
    using namespace OpenGolfOnline;
    FRangeShot Shot;
    Shot.Id = FGuid::NewGuid();
    Shot.SessionId = FGuid::NewGuid();
    Shot.ShotAt = FDateTime::UtcNow();
    Shot.CarryYards = 150.25;
    Shot.RollYards = -2.25;
    auto Json = EncodeShot(Shot);
    TestTrue(TEXT("Valid range shot"), Json.IsValid());
    if (!Json) return false;
    TestEqual(TEXT("Carry rounds away from zero"), Json->GetNumberField(TEXT("carry_yd")), 150.3);
    TestEqual(TEXT("Signed roll rounds away from zero"), Json->GetNumberField(TEXT("roll_yd")), -2.3);
    FString Wire;
    FJsonSerializer::Serialize(Json.ToSharedRef(),TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Wire));
    TestTrue(TEXT("Wire carry has exactly one decimal"), Wire.Contains(TEXT("\"carry_yd\":150.3")) && !Wire.Contains(TEXT("150.300")));
    TestTrue(TEXT("Wire roll has exactly one decimal"), Wire.Contains(TEXT("\"roll_yd\":-2.3")) && !Wire.Contains(TEXT("-2.299")));
    TestEqual(TEXT("Driver uses server key"), Json->GetStringField(TEXT("club")), FString(TEXT("DR")));
    TestTrue(TEXT("Completely missing measurements remain null"), Json->Values[TEXT("details")]->IsNull());
    Shot.Club = ELaunchMonitorClub::Putter;
    TestFalse(TEXT("Putter excluded"), EncodeShot(Shot).IsValid());
    Shot.Club = ELaunchMonitorClub::LobWedge;
    TestFalse(TEXT("Lob wedge excluded"), EncodeShot(Shot).IsValid());
    Shot.Club = ELaunchMonitorClub::Driver;
    Shot.CarryYards = 451;
    TestFalse(TEXT("Excess carry excluded"), EncodeShot(Shot).IsValid());
    TestTrue(TEXT("Rate limit retains shots"), ClassifyStatus(429) == EFailure::RetryLater);
    TestTrue(TEXT("Proxy refusal retains shots"), ClassifyStatus(403) == EFailure::RetryLater);
    TestTrue(TEXT("Invalid data identified"), ClassifyStatus(422) == EFailure::Rejected);
    return true;
}

#endif
