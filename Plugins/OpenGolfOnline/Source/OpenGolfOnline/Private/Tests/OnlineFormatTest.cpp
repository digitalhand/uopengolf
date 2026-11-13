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

    // A UTC instant in a zone reads as that zone's wall time, which formats the same as the wall time in the
    // invariant (unconverted) zone, whatever the culture.
    auto Wall=[](int32 Year,int32 Month,int32 Day,int32 Hour)
    {return FText::AsDateTime(FDateTime(Year,Month,Day,Hour,0,0),EDateTimeStyle::Medium,EDateTimeStyle::Short,FText::GetInvariantTimeZone()).ToString();};
    TestEqual(TEXT("Winter Pacific time is UTC-8 (PST)"),ShotTime(TEXT("2026-11-11T00:00:00Z"),TEXT("America/Los_Angeles")),Wall(2026,11,10,16));
    TestEqual(TEXT("Summer Pacific time is UTC-7 (PDT)"),ShotTime(TEXT("2026-07-01T00:00:00Z"),TEXT("America/Los_Angeles")),Wall(2026,6,30,17));
    TestEqual(TEXT("Fractional seconds parse"),ShotTime(TEXT("2026-11-11T00:00:00.250Z"),TEXT("America/Los_Angeles")),Wall(2026,11,10,16));
    TestEqual(TEXT("UTC is unchanged"),ShotTime(TEXT("2026-11-11T00:00:00Z"),TEXT("Etc/UTC")),Wall(2026,11,11,0));
    TestEqual(TEXT("Unparseable time is shown as sent"),ShotTime(TEXT("not a time"),TEXT("Etc/UTC")),FString(TEXT("not a time")));
    TestEqual(TEXT("Member-since date in the zone"),ShotDate(TEXT("2026-11-11T03:00:00Z"),TEXT("America/Los_Angeles")),
        FText::AsDate(FDateTime(2026,11,10),EDateTimeStyle::Medium,FText::GetInvariantTimeZone()).ToString());
    return true;
}

#endif
