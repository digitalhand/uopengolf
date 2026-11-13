// SPDX-License-Identifier: MIT
#include "GameUpdateVersion.h"

namespace GameUpdate
{
    namespace
    {
        bool ParseNumber(const FString& Text, int32& Out)
        {
            if (Text.IsEmpty() || Text.Len() > 9) return false;
            for (TCHAR C : Text) if (!FChar::IsDigit(C)) return false;
            Out = FCString::Atoi(*Text);
            return true;
        }

        /** Semantic Versioning rule 11: identifiers compare numerically, or as text; numbers sort first. */
        int32 CompareIdentifier(const FString& A, const FString& B)
        {
            int32 NumberA, NumberB;
            const bool bNumberA = ParseNumber(A, NumberA), bNumberB = ParseNumber(B, NumberB);
            if (bNumberA && bNumberB) return NumberA - NumberB;
            if (bNumberA != bNumberB) return bNumberA ? -1 : 1;
            return A.Compare(B, ESearchCase::CaseSensitive);
        }
    }

    bool FVersion::Parse(const FString& Text, FVersion& Out)
    {
        FString Rest = Text.TrimStartAndEnd();
        if (Rest.StartsWith(TEXT("v"))) Rest.RightChopInline(1);
        int32 Plus;
        if (Rest.FindChar('+', Plus)) Rest.LeftInline(Plus);
        FString Core = Rest, Prerelease;
        int32 Dash;
        if (Rest.FindChar('-', Dash))
        {
            Core = Rest.Left(Dash);
            Prerelease = Rest.Mid(Dash + 1);
            if (Prerelease.IsEmpty()) return false;
        }
        TArray<FString> Parts;
        Core.ParseIntoArray(Parts, TEXT("."), false);
        FVersion V;
        if (Parts.Num() != 3 || !ParseNumber(Parts[0], V.Major) || !ParseNumber(Parts[1], V.Minor) || !ParseNumber(Parts[2], V.Patch))
            return false;
        V.Prerelease = MoveTemp(Prerelease);
        Out = MoveTemp(V);
        return true;
    }

    int32 FVersion::Compare(const FVersion& Other) const
    {
        if (Major != Other.Major) return Major - Other.Major;
        if (Minor != Other.Minor) return Minor - Other.Minor;
        if (Patch != Other.Patch) return Patch - Other.Patch;
        // A release ranks above any of its prereleases.
        if (Prerelease.IsEmpty() != Other.Prerelease.IsEmpty()) return Prerelease.IsEmpty() ? 1 : -1;
        if (Prerelease.IsEmpty()) return 0;
        TArray<FString> A, B;
        Prerelease.ParseIntoArray(A, TEXT("."), false);
        Other.Prerelease.ParseIntoArray(B, TEXT("."), false);
        for (int32 I = 0; I < FMath::Min(A.Num(), B.Num()); ++I)
            if (const int32 Order = CompareIdentifier(A[I], B[I])) return Order;
        return A.Num() - B.Num();
    }

    bool IsNewer(const FString& Latest, const FString& Current)
    {
        FVersion L, C;
        return FVersion::Parse(Latest, L) && FVersion::Parse(Current, C) && L.Compare(C) > 0;
    }
}
