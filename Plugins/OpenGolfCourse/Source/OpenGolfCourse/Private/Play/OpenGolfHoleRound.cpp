// SPDX-License-Identifier: MIT
#include "Play/OpenGolfHoleRound.h"

void FOpenGolfHoleRound::Start(int32 InPar)
{
    *this = FOpenGolfHoleRound();
    Par = FMath::Max(InPar, 1);
}

void FOpenGolfHoleRound::RecordStroke(bool bFromGreen, bool bInCup)
{
    if (bHoled || bPractice)
        return;
    ++Strokes;
    if (bFromGreen)
        ++Putts;
    bHoled = bInCup;
}

FString FOpenGolfHoleRound::ResultName() const
{
    if (!bHoled)
        return FString();
    return bPractice ? FString::Printf(TEXT("Holed in %d"), Strokes) : ScoreName(Strokes, Par);
}

FString FOpenGolfHoleRound::ScoreName(int32 Strokes, int32 Par)
{
    if (Strokes == 1)
        return TEXT("Hole in one");
    switch (Strokes - Par)
    {
    case -3:
        return TEXT("Albatross");
    case -2:
        return TEXT("Eagle");
    case -1:
        return TEXT("Birdie");
    case 0:
        return TEXT("Par");
    case 1:
        return TEXT("Bogey");
    case 2:
        return TEXT("Double bogey");
    case 3:
        return TEXT("Triple bogey");
    default:
        return FString::Printf(TEXT("%+d"), Strokes - Par);
    }
}
