// SPDX-License-Identifier: MIT
#include "Play/OpenGolfRoundSetup.h"
#include "Math/RandomStream.h"

namespace OpenGolfPlay
{
    namespace RoundSetupDetail
    {
        /** A direction as a whole degree from 0 to 359. */
        float WholeDirection(float Deg)
        {
            return FMath::IsFinite(Deg) ? static_cast<float>(FMath::RoundToInt(FRotator::ClampAxis(Deg)) % 360) : 0.0f;
        }

        /** The value of ?Key=Value in a travel URL; the first one when it appears more than once. */
        bool FindOption(const FString& Options, const TCHAR* Key, FString& OutValue)
        {
            TArray<FString> Parts;
            Options.ParseIntoArray(Parts, TEXT("?"), true);
            for (const FString& Part : Parts)
            {
                FString Name;
                FString Value;
                if (!Part.Split(TEXT("="), &Name, &Value))
                    Name = Part;
                if (Name.TrimStartAndEnd().Equals(Key, ESearchCase::IgnoreCase))
                {
                    OutValue = Value.TrimStartAndEnd();
                    return true;
                }
            }
            return false;
        }

        bool FindInt(const FString& Options, const TCHAR* Key, int32& OutValue)
        {
            FString Value;
            if (!FindOption(Options, Key, Value) || !Value.IsNumeric())
                return false;
            OutValue = FCString::Atoi(*Value);
            return true;
        }

        TArray<int32> Sequence(int32 First, int32 Last)
        {
            TArray<int32> Holes;
            for (int32 Hole = First; Hole <= Last; ++Hole)
                Holes.Add(Hole);
            return Holes;
        }

        /** Sorted, unique, and on a course of HoleCount holes. */
        TArray<int32> CleanHoles(const TArray<int32>& Holes, int32 HoleCount)
        {
            TArray<int32> Clean;
            for (const int32 Hole : Holes)
                if (Hole >= 1 && Hole <= HoleCount)
                    Clean.AddUnique(Hole);
            Clean.Sort();
            return Clean;
        }
    } // namespace RoundSetupDetail

    void FOpenGolfRoundSetup::RollWind(FRandomStream& Stream)
    {
        WindSpeedMph = Stream.RandRange(0, MaxRoundWindMph);
        WindFromDeg = static_cast<float>(Stream.RandRange(0, 359));
    }

    void FOpenGolfRoundSetup::Validate(int32 HoleCount, int32 TeeCount)
    {
        using namespace RoundSetupDetail;
        TeeIndex = FMath::Clamp(TeeIndex, 0, FMath::Max(TeeCount - 1, 0));
        GreenStimpFeet = FMath::Clamp(GreenStimpFeet, MinRoundStimpFeet, MaxRoundStimpFeet);
        WindFromDeg = WholeDirection(WindFromDeg);
        WindSpeedMph = FMath::Clamp(WindSpeedMph, 0, MaxRoundWindMph);
        // The nearer of the two radii the setup offers.
        GimmieFeet = GimmieFeet <= (ShortGimmieFeet + LongGimmieFeet) / 2 ? ShortGimmieFeet : LongGimmieFeet;
        Holes = CleanHoles(Holes, HoleCount);
        if (Holes.IsEmpty())
            Holes = HolesFor(EOpenGolfHoleSet::All, HoleCount);
    }

    FString FOpenGolfRoundSetup::ToOptions() const
    {
        using namespace RoundSetupDetail;
        FString Options = FString::Printf(TEXT("?TeeIndex=%d?Stimp=%d?WindMph=%d?WindFrom=%d?Gimmie=%d"), TeeIndex, GreenStimpFeet, WindSpeedMph,
            FMath::RoundToInt(WindFromDeg), AppliedGimmieFeet());
        if (!Holes.IsEmpty())
            Options += TEXT("?Holes=") + FormatHoles(Holes);
        return Options;
    }

    FOpenGolfRoundSetup FOpenGolfRoundSetup::FromOptions(const FString& Options, const FOpenGolfRoundSetup& Defaults, int32 HoleCount, int32 TeeCount)
    {
        using namespace RoundSetupDetail;
        FOpenGolfRoundSetup Setup = Defaults;
        FindInt(Options, TEXT("TeeIndex"), Setup.TeeIndex);
        FindInt(Options, TEXT("Stimp"), Setup.GreenStimpFeet);
        FindInt(Options, TEXT("WindMph"), Setup.WindSpeedMph);
        int32 WholeNumber = 0;
        if (FindInt(Options, TEXT("WindFrom"), WholeNumber))
            Setup.WindFromDeg = static_cast<float>(WholeNumber);
        if (FindInt(Options, TEXT("Gimmie"), WholeNumber))
        {
            Setup.bGimmie = WholeNumber > 0;
            if (Setup.bGimmie)
                Setup.GimmieFeet = WholeNumber;
        }
        FString HoleList;
        if (FindOption(Options, TEXT("Holes"), HoleList))
            Setup.Holes = ParseHoles(HoleList, HoleCount);
        Setup.Validate(HoleCount, TeeCount);
        return Setup;
    }

    bool FOpenGolfRoundSetup::operator==(const FOpenGolfRoundSetup& Other) const
    {
        return TeeIndex == Other.TeeIndex && GreenStimpFeet == Other.GreenStimpFeet && WindSpeedMph == Other.WindSpeedMph &&
               WindFromDeg == Other.WindFromDeg && bGimmie == Other.bGimmie && GimmieFeet == Other.GimmieFeet && Holes == Other.Holes;
    }

    bool HasNines(int32 HoleCount)
    {
        return HoleCount > HolesPerNine;
    }

    TArray<int32> HolesFor(EOpenGolfHoleSet Set, int32 HoleCount)
    {
        using RoundSetupDetail::Sequence;
        if (HoleCount <= 0)
            return {};
        if (HasNines(HoleCount) && Set == EOpenGolfHoleSet::Front)
            return Sequence(1, HolesPerNine);
        if (HasNines(HoleCount) && Set == EOpenGolfHoleSet::Back)
            return Sequence(HolesPerNine + 1, HoleCount);
        return Sequence(1, HoleCount);
    }

    EOpenGolfHoleSet DetectHoleSet(const TArray<int32>& Holes, int32 HoleCount)
    {
        if (Holes == HolesFor(EOpenGolfHoleSet::All, HoleCount))
            return EOpenGolfHoleSet::All;
        if (HasNines(HoleCount) && Holes == HolesFor(EOpenGolfHoleSet::Front, HoleCount))
            return EOpenGolfHoleSet::Front;
        if (HasNines(HoleCount) && Holes == HolesFor(EOpenGolfHoleSet::Back, HoleCount))
            return EOpenGolfHoleSet::Back;
        return EOpenGolfHoleSet::Custom;
    }

    FString FormatHoles(const TArray<int32>& Holes)
    {
        TArray<FString> Runs;
        for (int32 Index = 0; Index < Holes.Num();)
        {
            int32 End = Index;
            while (End + 1 < Holes.Num() && Holes[End + 1] == Holes[End] + 1)
                ++End;
            Runs.Add(End > Index ? FString::Printf(TEXT("%d-%d"), Holes[Index], Holes[End]) : FString::FromInt(Holes[Index]));
            Index = End + 1;
        }
        return FString::Join(Runs, TEXT(","));
    }

    TArray<int32> ParseHoles(const FString& Text, int32 HoleCount)
    {
        TArray<FString> Tokens;
        Text.ParseIntoArray(Tokens, TEXT(","), true);
        TArray<int32> Holes;
        for (FString Token : Tokens)
        {
            Token.TrimStartAndEndInline();
            FString First;
            FString Last;
            if (!Token.Split(TEXT("-"), &First, &Last))
            {
                if (Token.IsNumeric())
                    Holes.Add(FCString::Atoi(*Token));
                continue;
            }
            if (!First.IsNumeric() || !Last.IsNumeric())
                continue;
            // Inside the course before counting, so a huge range cannot run long.
            const int32 From = FMath::Clamp(FMath::Min(FCString::Atoi(*First), FCString::Atoi(*Last)), 1, FMath::Max(HoleCount, 1));
            const int32 To = FMath::Clamp(FMath::Max(FCString::Atoi(*First), FCString::Atoi(*Last)), 0, HoleCount);
            for (int32 Hole = From; Hole <= To; ++Hole)
                Holes.Add(Hole);
        }
        return RoundSetupDetail::CleanHoles(Holes, HoleCount);
    }

    FString GreenSpeedLabel(int32 StimpFeet)
    {
        const int32 Feet = FMath::Clamp(StimpFeet, MinRoundStimpFeet, MaxRoundStimpFeet);
        const TCHAR* Name = Feet <= 9 ? TEXT("Slow") : Feet <= 11 ? TEXT("Medium") : Feet <= 13 ? TEXT("Fast") : TEXT("Tour");
        return FString::Printf(TEXT("%d ft · %s"), Feet, Name);
    }

    float RoundWindRelativeFromDeg(float WorldFromDeg, float TeeShotBearingDeg)
    {
        const float Difference = WorldFromDeg - TeeShotBearingDeg;
        return FMath::IsFinite(Difference) ? FRotator::ClampAxis(Difference) : 0.0f;
    }
} // namespace OpenGolfPlay
