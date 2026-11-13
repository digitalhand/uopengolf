// SPDX-License-Identifier: MIT
// Display text for the account panel: club names and shot times. Shared with the plugin's tests.
#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "Misc/DateTime.h"

namespace OpenGolfOnline
{
    /** The standard short name for an API club code: "7I" becomes "7i"; DR, 3W, PW, GW and SW stay as sent. */
    inline FString ClubAbbreviation(const FString& Club)
    {
        return Club.Len() == 2 && FChar::IsDigit(Club[0]) && Club[1] == TEXT('I') ? Club.Left(1) + TEXT("i") : Club;
    }

    /** The club's spoken name ("7 iron", "Pitching wedge"), for screen readers. */
    inline FString ClubName(const FString& Club)
    {
        if (Club == TEXT("DR")) return TEXT("Driver");
        if (Club == TEXT("PW")) return TEXT("Pitching wedge");
        if (Club == TEXT("GW")) return TEXT("Gap wedge");
        if (Club == TEXT("SW")) return TEXT("Sand wedge");
        return Club.Left(1) + (Club.EndsWith(TEXT("I")) ? TEXT(" iron") : TEXT(" wood"));
    }

    /**
     * A UTC ISO-8601 time from the API as a medium date and short time in TimeZone, an IANA ID such as
     * "America/Los_Angeles" (empty is this computer's zone). ICU applies daylight saving. Text that is not
     * ISO-8601 is returned as sent.
     */
    inline FString ShotTime(const FString& Iso, const FString& TimeZone)
    {
        FDateTime Utc;
        return FDateTime::ParseIso8601(*Iso, Utc) ? FText::AsDateTime(Utc, EDateTimeStyle::Medium, EDateTimeStyle::Short, TimeZone).ToString() : Iso;
    }

    /** As ShotTime, the date only. */
    inline FString ShotDate(const FString& Iso, const FString& TimeZone)
    {
        FDateTime Utc;
        return FDateTime::ParseIso8601(*Iso, Utc) ? FText::AsDate(Utc, EDateTimeStyle::Medium, TimeZone).ToString() : Iso.Left(10);
    }
} // namespace OpenGolfOnline
