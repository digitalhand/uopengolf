// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "OnlineFormat.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfFormatTest,"OpenGolf.Online.Format",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOpenGolfFormatTest::RunTest(const FString&)
{
    using namespace OpenGolfOnline;
    TestEqual(TEXT("Irons are abbreviated"),ClubAbbreviation(TEXT("7I")),FString(TEXT("7i")));
    TestEqual(TEXT("Every iron"),ClubAbbreviation(TEXT("4I"))+ClubAbbreviation(TEXT("9I")),FString(TEXT("4i9i")));
    for(const TCHAR* Club:{TEXT("DR"),TEXT("3W"),TEXT("5W"),TEXT("7W"),TEXT("PW"),TEXT("GW"),TEXT("SW")})
        TestEqual(FString(TEXT("Kept as sent: "))+Club,ClubAbbreviation(Club),FString(Club));
    TestEqual(TEXT("Spoken iron name"),ClubName(TEXT("7I")),FString(TEXT("7 iron")));
    TestEqual(TEXT("Spoken wedge name"),ClubName(TEXT("PW")),FString(TEXT("Pitching wedge")));

    const FDateTime Utc(2026, 11, 11);
    const FString LocalTime = FText::AsDateTime(Utc, TEXT("%b%e")).ToString() + TEXT(" ") + FText::AsDateTime(Utc, TEXT("%H%M")).ToString();
    TestEqual(TEXT("UTC timestamp displays locally"), ShotTime(TEXT("2026-11-11T00:00:00Z")), LocalTime);
    TestEqual(TEXT("ISO offset represents the same instant"), ShotTime(TEXT("2026-11-10T16:00:00-08:00")), LocalTime);
    TestEqual(TEXT("Unparseable time is shown as sent"), ShotTime(TEXT("not a time")), FString(TEXT("not a time")));
    TestEqual(TEXT("Member-since date displays locally"), ShotDate(TEXT("2026-11-11T00:00:00Z")),
        FText::AsDate(Utc, EDateTimeStyle::Medium).ToString());
    TestEqual(TEXT("Unparseable date keeps its date prefix"), ShotDate(TEXT("2026-99-99 invalid")), FString(TEXT("2026-99-99")));
    return true;
}

#endif
