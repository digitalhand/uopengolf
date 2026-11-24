// SPDX-License-Identifier: MIT
// The hole map's elevation field and picture compositing: synthetic ground and pictures, no world.
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Texture2D.h"
#include "HoleMap/HoleMapCompositor.h"
#include "HoleMapField.h"
#include "HoleMapFrame.h"
#include "Misc/AutomationTest.h"

namespace HoleMapCompositorTestDetail
{
    using HoleMapCompositor::FPicture;

    /** A 100 m hole along +X. */
    FHoleMapFrame Frame() { return FHoleMapFrame::FromHole(FVector::ZeroVector, FVector(10000, 0, 0), 1000, 4000, 0); }

    FPicture Solid(int32 Width, int32 Height, FColor Color)
    {
        FPicture Image;
        Image.Width = Width;
        Image.Height = Height;
        Image.Pixels.Init(Color, Width * Height);
        return Image;
    }

    FHoleMapField Field(const FHoleMapFrame& InFrame, TFunction<double(FVector2D)> Value)
    {
        return FHoleMapField::Build(InFrame, 300, [&Value](FVector2D World, double& Out) { Out = Value(World); return true; });
    }

    int32 Luma(FColor Color) { return Color.R + Color.G + Color.B; }

    int32 CountChanged(const FPicture& Before, const FPicture& After)
    {
        int32 Count = 0;
        for (int32 Index = 0; Index < Before.Pixels.Num(); ++Index)
            Count += Before.Pixels[Index] != After.Pixels[Index];
        return Count;
    }
} // namespace HoleMapCompositorTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoleMapFieldTest, "UOpenGolf.Minimap.Field",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHoleMapFieldTest::RunTest(const FString&)
{
    using namespace HoleMapCompositorTestDetail;
    // A plane is reproduced exactly by bilinear sampling, between and beyond the grid points.
    const auto Plane = [](FVector2D P) { return 0.02 * P.X - 0.03 * P.Y + 500; };
    const FHoleMapField Heights = Field(Frame(), Plane);
    TestTrue(TEXT("A sampled field is valid"), Heights.IsValid());
    for (const FVector2D Point : {FVector2D(0, 0), FVector2D(4321, 987), FVector2D(10000, -1500), FVector2D(-900, 3000)})
    {
        TestEqual(TEXT("Bilinear sampling reproduces a plane"), Heights.At(Point), Plane(Point), 1e-3);
        TestTrue(TEXT("The gradient of a plane is its slope"), Heights.Gradient(Point).Equals(FVector2D(0.02, -0.03), 1e-6));
    }
    TestTrue(TEXT("The field spans the plane's heights over the frame"), Heights.GetMin() < 500 && Heights.GetMax() > 500);

    // Points the sampler misses take their neighbours' values rather than leaving holes.
    const FHoleMapField Partial = FHoleMapField::Build(Frame(), 300, [](FVector2D World, double& Out) { Out = 200; return World.X < 5000; });
    TestEqual(TEXT("Missed points are filled from sampled ones"), Partial.At(FVector2D(9000, 0)), 200.0, 1e-6);

    // A pure sampler may run in parallel; the field comes out the same.
    const FHoleMapField Parallel = FHoleMapField::Build(Frame(), 300, [&Plane](FVector2D World, double& Out) { Out = Plane(World); return true; }, EHoleMapSampling::Parallel);
    const FHoleMapField ParallelPartial = FHoleMapField::Build(Frame(), 300, [](FVector2D World, double& Out) { Out = 200; return World.X < 5000; }, EHoleMapSampling::Parallel);
    for (const FVector2D Point : {FVector2D(0, 0), FVector2D(4321, 987), FVector2D(9000, 0)})
    {
        TestEqual(TEXT("Parallel sampling matches serial"), Parallel.At(Point), Heights.At(Point), 1e-9);
        TestEqual(TEXT("Parallel sampling fills gaps the same way"), ParallelPartial.At(Point), Partial.At(Point), 1e-9);
    }

    const FHoleMapField None = FHoleMapField::Build(Frame(), 300, [](FVector2D, double&) { return false; });
    TestFalse(TEXT("A field with no samples is invalid"), None.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoleMapReliefTest, "UOpenGolf.Minimap.Relief",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHoleMapReliefTest::RunTest(const FString&)
{
    using namespace HoleMapCompositorTestDetail;
    const FHoleMapFrame Map = Frame();
    const FPicture Grass = Solid(40, 64, FColor(90, 140, 60));
    const FPicture Flat = HoleMapCompositor::WithRelief(Grass, Map, Field(Map, [](FVector2D) { return 0.0; }));
    TestEqual(TEXT("Flat ground leaves the picture unchanged"), CountChanged(Grass, Flat), 0);

    // The light comes from the map's top left: ground falling toward the top left faces it; rising toward it faces away.
    const FVector2D TopLeft = (Map.Up - Map.Right()).GetSafeNormal();
    const FPicture Toward = HoleMapCompositor::WithRelief(Grass, Map, Field(Map, [&](FVector2D P) { return -0.2 * (P | TopLeft); }));
    const FPicture Away = HoleMapCompositor::WithRelief(Grass, Map, Field(Map, [&](FVector2D P) { return 0.2 * (P | TopLeft); }));
    const int32 Middle = 32 * 40 + 20;
    TestTrue(TEXT("A slope facing the light is lit"), Luma(Toward.Pixels[Middle]) > Luma(Grass.Pixels[Middle]));
    TestTrue(TEXT("A slope facing away is shaded"), Luma(Away.Pixels[Middle]) < Luma(Grass.Pixels[Middle]));
    TestEqual(TEXT("Relief keeps the picture opaque"), Away.Pixels[Middle].A, uint8(255));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoleMapContoursTest, "UOpenGolf.Minimap.Contours",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHoleMapContoursTest::RunTest(const FString&)
{
    using namespace HoleMapCompositorTestDetail;
    TestEqual(TEXT("A 10 m span takes 1 m contours"), HoleMapCompositor::ContourInterval(1000), 100.0);
    TestEqual(TEXT("A 40 m span takes 2 m contours"), HoleMapCompositor::ContourInterval(4000), 200.0);
    TestEqual(TEXT("A 100 m span takes 5 m contours"), HoleMapCompositor::ContourInterval(10000), 500.0);
    TestEqual(TEXT("Flat ground takes the finest interval"), HoleMapCompositor::ContourInterval(0), 100.0);

    const FHoleMapFrame Map = Frame();
    const FPicture Sand = Solid(80, 128, FColor(200, 150, 100));
    TestEqual(TEXT("Flat ground draws no contours"),
        CountChanged(Sand, HoleMapCompositor::WithContours(Sand, Map, Field(Map, [](FVector2D) { return 37.0; }))), 0);

    // Rising 1 m every 25 m up the map: the 120 m frame crosses about five 1 m contours, each a band across the picture.
    const FPicture Ramp = HoleMapCompositor::WithContours(Sand, Map, Field(Map, [&Map](FVector2D P) { return (P | Map.Up) * 0.04; }));
    int32 LineRows = 0;
    for (int32 Y = 0; Y < Ramp.Height; ++Y)
    {
        const bool bLine = Ramp.Pixels[Y * Ramp.Width + Ramp.Width / 2] != Sand.Pixels[0];
        LineRows += bLine;
        if (bLine)
            TestTrue(TEXT("A contour runs across the whole picture"), Ramp.Pixels[Y * Ramp.Width] != Sand.Pixels[0]);
    }
    TestTrue(FString::Printf(TEXT("A ramp draws its contours (%d rows)"), LineRows), LineRows > 0 && LineRows < Ramp.Height / 4);
    TestTrue(TEXT("Contours darken"), Luma(Ramp.Pixels[0]) <= Luma(Sand.Pixels[0]));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoleMapSurroundTest, "UOpenGolf.Minimap.SurroundDimming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHoleMapSurroundTest::RunTest(const FString&)
{
    using namespace HoleMapCompositorTestDetail;
    const FHoleMapFrame Map = Frame();
    const FPicture Picture = Solid(40, 64, FColor(210, 120, 60));
    // Play covers the right half of the map; the left edge is about 35 m from it.
    const FHoleMapField FromPlay = Field(Map, [&Map](FVector2D P) { return FMath::Max(0.0, -((P - FVector2D(Map.Center)) | Map.Right())); });
    const FPicture Dimmed = HoleMapCompositor::WithSurroundDimmed(Picture, Map, FromPlay);
    const FColor Near = Dimmed.Pixels[32 * 40 + 35], Far = Dimmed.Pixels[32 * 40 + 1];
    TestEqual(TEXT("Play keeps its colour"), Near, Picture.Pixels[0]);
    TestTrue(TEXT("The surround is darker"), Luma(Far) < Luma(Picture.Pixels[0]));
    TestTrue(TEXT("The surround is less saturated"), Far.R - Far.B < Picture.Pixels[0].R - Picture.Pixels[0].B);
    TestEqual(TEXT("Without play data nothing is dimmed"),
        CountChanged(Picture, HoleMapCompositor::WithSurroundDimmed(Picture, Map, FHoleMapField())), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoleMapMipsTest, "UOpenGolf.Minimap.Mips",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHoleMapMipsTest::RunTest(const FString&)
{
    using namespace HoleMapCompositorTestDetail;
    const TArray<FPicture> Chain = HoleMapCompositor::BuildMips(Solid(1280, 2048, FColor(10, 20, 30)));
    TestEqual(TEXT("The chain runs down to one pixel"), Chain.Num(), 12);
    TestTrue(TEXT("Each mip halves the last"), Chain[1].Width == 640 && Chain[1].Height == 1024 && Chain.Last().Width == 1 && Chain.Last().Height == 1);
    TestEqual(TEXT("A solid picture stays solid"), Chain.Last().Pixels[0], FColor(10, 20, 30));

    // Black and white average to middle grey in light, which is 188 in sRGB, not 128.
    FPicture Checker = Solid(2, 2, FColor::Black);
    Checker.Pixels[1] = Checker.Pixels[2] = FColor::White;
    const TArray<FPicture> Grey = HoleMapCompositor::BuildMips(Checker);
    TestTrue(FString::Printf(TEXT("Mips average in linear light (%d)"), Grey[1].Pixels[0].G), FMath::Abs(int32(Grey[1].Pixels[0].G) - 188) <= 1);

    const TArray<FPicture> Odd = HoleMapCompositor::BuildMips(Solid(5, 3, FColor::White));
    TestTrue(TEXT("Odd sizes round down"), Odd.Num() == 3 && Odd[1].Width == 2 && Odd[1].Height == 1);

    UTexture2D* Texture = HoleMapCompositor::MakeTexture(Chain, TEXT("HoleMapCompositorTest"));
    TestTrue(TEXT("The texture keeps every mip"), Texture && Texture->GetNumMips() == Chain.Num() && Texture->GetSizeX() == 1280);
    TestTrue(TEXT("The texture is sRGB and filtered between mips"), Texture && Texture->SRGB && Texture->Filter == TF_Trilinear);
    return true;
}
#endif
