// SPDX-License-Identifier: MIT
#include "HoleMapField.h"
#include "Async/ParallelFor.h"

namespace HoleMapFieldDetail
{
    /** Fills every grid point not Known (0) from its known neighbours, spreading outward; false when none is known. */
    bool FillGaps(TArray<float>& Values, TArray<uint8>& Known, int32 Columns, int32 Rows)
    {
        if (!Known.Contains(1))
            return false;
        while (Known.Contains(0))
        {
            // Each pass reads the previous one's values, so the fill spreads evenly instead of along the scan order.
            const TArray<float> Before = Values;
            const TArray<uint8> KnownBefore = Known;
            for (int32 Row = 0; Row < Rows; ++Row)
            {
                for (int32 Column = 0; Column < Columns; ++Column)
                {
                    const int32 Index = Row * Columns + Column;
                    if (KnownBefore[Index])
                        continue;
                    double Sum = 0;
                    int32 Count = 0;
                    const auto Take = [&](int32 C, int32 R)
                    {
                        if (C < 0 || R < 0 || C >= Columns || R >= Rows || !KnownBefore[R * Columns + C])
                            return;
                        Sum += Before[R * Columns + C];
                        ++Count;
                    };
                    Take(Column - 1, Row);
                    Take(Column + 1, Row);
                    Take(Column, Row - 1);
                    Take(Column, Row + 1);
                    if (Count > 0)
                    {
                        Values[Index] = float(Sum / Count);
                        Known[Index] = 1;
                    }
                }
            }
        }
        return true;
    }
} // namespace HoleMapFieldDetail

FHoleMapField FHoleMapField::Build(const FHoleMapFrame& Frame, double Spacing, FSampler Sampler, EHoleMapSampling Sampling)
{
    FHoleMapField Field;
    Field.Frame = Frame;
    Field.Spacing = FMath::Max(Spacing, 1.0);
    // One spacing beyond each edge, and a point on each end.
    Field.Columns = FMath::CeilToInt32(Frame.Length * FHoleMapFrame::Aspect / Field.Spacing) + 3;
    Field.Rows = FMath::CeilToInt32(Frame.Length / Field.Spacing) + 3;

    TArray<float> Values;
    Values.SetNumZeroed(Field.Columns * Field.Rows);
    // One flag per point rather than a bit array, so parallel rows never write to the same word.
    TArray<uint8> Sampled;
    Sampled.SetNumZeroed(Values.Num());
    ParallelFor(Field.Rows, [&](int32 Row)
    {
        for (int32 Column = 0; Column < Field.Columns; ++Column)
        {
            double Value = 0;
            if (Sampler(Field.PointAt(Column, Row), Value) && FMath::IsFinite(Value))
            {
                Values[Row * Field.Columns + Column] = float(Value);
                Sampled[Row * Field.Columns + Column] = 1;
            }
        }
    }, Sampling == EHoleMapSampling::Parallel ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread);
    if (!HoleMapFieldDetail::FillGaps(Values, Sampled, Field.Columns, Field.Rows))
        return FHoleMapField();

    Field.Values = MoveTemp(Values);
    Field.Min = Field.Max = Field.Values[0];
    for (const float Value : Field.Values)
    {
        Field.Min = FMath::Min<double>(Field.Min, Value);
        Field.Max = FMath::Max<double>(Field.Max, Value);
    }
    return Field;
}

FVector2D FHoleMapField::PointAt(int32 Column, int32 Row) const
{
    const double Across = (Column - 1) * Spacing - Frame.Length * FHoleMapFrame::Aspect * 0.5;
    const double Along = (Row - 1) * Spacing - Frame.Length * 0.5;
    return FVector2D(Frame.Center) + Frame.Right() * Across + Frame.Up * Along;
}

double FHoleMapField::At(FVector2D World) const
{
    if (!IsValid())
        return 0;
    const FVector2D Delta = World - FVector2D(Frame.Center);
    const double X = FMath::Clamp(((Delta | Frame.Right()) + Frame.Length * FHoleMapFrame::Aspect * 0.5) / Spacing + 1, 0.0, Columns - 1.0);
    const double Y = FMath::Clamp(((Delta | Frame.Up) + Frame.Length * 0.5) / Spacing + 1, 0.0, Rows - 1.0);
    const int32 Column = FMath::Min(FMath::FloorToInt32(X), Columns - 2);
    const int32 Row = FMath::Min(FMath::FloorToInt32(Y), Rows - 2);
    const double U = X - Column, V = Y - Row;
    const double Bottom = FMath::Lerp(Value(Column, Row), Value(Column + 1, Row), U);
    const double Top = FMath::Lerp(Value(Column, Row + 1), Value(Column + 1, Row + 1), U);
    return FMath::Lerp(Bottom, Top, V);
}

FVector2D FHoleMapField::Gradient(FVector2D World) const
{
    if (!IsValid())
        return FVector2D::ZeroVector;
    const FVector2D Right = Frame.Right() * Spacing, Up = Frame.Up * Spacing;
    const double AcrossRate = (At(World + Right) - At(World - Right)) / (2 * Spacing);
    const double AlongRate = (At(World + Up) - At(World - Up)) / (2 * Spacing);
    return Frame.Right() * AcrossRate + Frame.Up * AlongRate;
}
