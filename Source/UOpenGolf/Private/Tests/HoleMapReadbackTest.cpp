// SPDX-License-Identifier: MIT
// The hole map's read-back hands over the same pixels as ReadPixels, a frame or two later, without making the game
// thread wait for the GPU.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HoleMap/HoleMapReadback.h"
#include "Misc/AutomationTest.h"
#include "TextureResource.h"

namespace HoleMapReadbackTestDetail
{
    class FCompareReadback : public IAutomationLatentCommand
    {
    public:
        FCompareReadback(FAutomationTestBase& InTest, UTextureRenderTarget2D& InTarget)
            : Test(InTest), Target(&InTarget)
        {
            Readback.Request(InTarget);
        }

        virtual bool Update() override
        {
            if (GetCurrentRunTime() > 10)
            {
                Test.AddError(TEXT("The read-back never landed"));
                return Finish();
            }
            TArray<FColor> Pixels;
            const FHoleMapReadback::EStatus Status = Readback.Poll(Pixels);
            if (Status == FHoleMapReadback::EStatus::Pending)
                return false;
            Test.TestTrue(TEXT("The read-back lands"), Status == FHoleMapReadback::EStatus::Ready);
            TArray<FColor> Expected;
            Test.TestTrue(TEXT("ReadPixels reads the same target"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Expected));
            Test.TestEqual(TEXT("As many pixels as ReadPixels"), Pixels.Num(), Expected.Num());
            Test.TestTrue(TEXT("Every pixel matches ReadPixels, in FColor order"), Pixels == Expected);
            Test.TestTrue(TEXT("The pattern is there to compare"), Expected.Num() > 0 && Expected[0] != Expected[30 * 301 + 40]);
            return Finish();
        }

    private:
        bool Finish()
        {
            Target->RemoveFromRoot();
            return true;
        }

        FAutomationTestBase& Test;
        UTextureRenderTarget2D* Target;
        FHoleMapReadback Readback;
    };
} // namespace HoleMapReadbackTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoleMapReadbackTest, "UOpenGolf.Minimap.Readback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FHoleMapReadbackTest::RunTest(const FString&)
{
    // The hole map's format, an odd width so the GPU pads each row, and a patch so the rows differ.
    auto* Target = NewObject<UTextureRenderTarget2D>();
    Target->AddToRoot();
    Target->RenderTargetFormat = RTF_RGBA8_SRGB;
    Target->ClearColor = FLinearColor(0.8f, 0.3f, 0.1f);
    Target->InitAutoFormat(301, 203);
    Target->UpdateResourceImmediate(true);
    FCanvas Canvas(Target->GameThread_GetRenderTargetResource(), nullptr, FGameTime(), GMaxRHIFeatureLevel);
    FCanvasTileItem Patch(FVector2D(17, 29), FVector2D(64, 40), FLinearColor(0.1f, 0.7f, 0.9f));
    Canvas.DrawItem(Patch);
    Canvas.Flush_GameThread();
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HoleMapReadbackTestDetail::FCompareReadback>(*this, *Target));
    return true;
}
#endif
