// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UI/RangeHud.h"
#include "OpenGolfWindIndicator.h"
#include "Tests/RenderedPlay.h"
#include "UI/WindIndicator.h"
#include "Slate/WidgetRenderer.h"
#include "Slate/SceneViewport.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfHUD.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "Types/SlateAttributeMetaData.h"
#include "Widgets/SViewport.h"

namespace
{
    class FCaptureWindIndicator : public IAutomationLatentCommand
    {
    public:
        explicit FCaptureWindIndicator(FAutomationTestBase* InTest) : Test(InTest) {}

        bool Update() override
        {
            UWorld* World = GEditor->PlayWorld;
            AUOpenGolfGameMode* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
            if (GetCurrentRunTime() > 90)
            {
                Test->AddError(TEXT("Timed out capturing wind indicator."));
                FScreenshotRequest::Reset();
                return true;
            }
            if (!Mode || !Mode->GetBall() || !Mode->GetRange())
                return false;
            auto* Controller = Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController());
            if (!Controller || !Controller->GetHUD())
                return false;
            if (Stage == 0)
            {
                const FIntPoint Size = RenderedPlay::CaptureSize();
                World->GetGameViewport()->GetGameViewport()->SetFixedViewportSize(Size.X, Size.Y);
                const TSharedRef<SRangeHud> Hud = SNew(SRangeHud).Controller(Controller);
                FSlateAttributeMetaData::UpdateAllAttributes(*Hud, FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);
                Test->TestEqual(TEXT("HUD allows clicks through"), Hud->GetVisibility(), EVisibility::HitTestInvisible);
                Controller->GetHUD()->bShowHUD = false;
                FSlateAttributeMetaData::UpdateAllAttributes(*Hud, FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);
                Test->TestEqual(TEXT("showhud hides indicator with HUD"), Hud->GetVisibility(), EVisibility::Collapsed);
                Controller->GetHUD()->bShowHUD = true;
                Controller->SetReplayPanelOpen(true);
                FSlateAttributeMetaData::UpdateAllAttributes(*Hud, FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidation);
                Test->TestEqual(TEXT("Test-shot panel hides indicator with HUD"), Hud->GetVisibility(), EVisibility::Collapsed);
                Controller->SetReplayPanelOpen(false);
                ++Stage;
            }
            if (Stage % 3 == 1)
            {
                const int32 View = (Stage - 1) / 3;
                const float Directions[] = {0, 0, 90, 180, 270};
                const TCHAR* Names[] = {TEXT("calm"), TEXT("headwind"), TEXT("from-right"), TEXT("tailwind"), TEXT("from-left")};
                Mode->GetRange()->WindSpeedMph = View == 0 ? 0 : 10;
                Mode->GetRange()->WindFromDeg = Directions[View];
                Filename = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(
                                                                                             TEXT("Screenshots/WindIndicator/game-%dx%d-%s-%s.png"), World->GetGameViewport()->Viewport->GetSizeXY().X,
                                                                                             World->GetGameViewport()->Viewport->GetSizeXY().Y, Names[View], *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))));
                IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
                ReadyAt = GetCurrentRunTime() + 2;
                ++Stage;
                return false;
            }
            if (Stage % 3 == 2)
            {
                if (GetCurrentRunTime() < ReadyAt)
                    return false;
                bool bFoundIndicator = false;
                for (TObjectIterator<UOpenGolfWindIndicator> It; It; ++It)
                {
                    if (It->GetWorld() == World && It->GetCachedWidget().IsValid())
                    {
                        const auto Indicator = StaticCastSharedPtr<SWindIndicator>(It->GetCachedWidget());
                        const auto Expected = WindDisplay::FromSettings(Mode->GetRange()->WindSpeedMph, Mode->GetRange()->WindFromDeg,
                            Controller->PlayerCameraManager->GetCameraRotation().Yaw);
                        Test->TestEqual(TEXT("Live HUD receives wind speed"), Indicator->GetWind().SpeedMph, Expected.SpeedMph, 1e-4f);
                        Test->TestTrue(TEXT("Live HUD receives camera-relative arrow"), Indicator->GetWind().Direction.Equals(Expected.Direction, 1e-5));
                        bFoundIndicator = true;
                    }
                }
                Test->TestTrue(TEXT("Gameplay created the Blueprint indicator"), bFoundIndicator);
                const FVector2D DrawSize = World->GetGameViewport()->GetGameViewportWidget()->GetCachedGeometry().GetDrawSize();
                Test->TestEqual(TEXT("Screenshot width matches viewport"), static_cast<int32>(DrawSize.X), World->GetGameViewport()->Viewport->GetSizeXY().X);
                Test->TestEqual(TEXT("Screenshot height matches viewport"), static_cast<int32>(DrawSize.Y), World->GetGameViewport()->Viewport->GetSizeXY().Y);
                FScreenshotRequest::RequestScreenshot(Filename, true, false, false, FIntRect(), true);
                ++Stage;
                return false;
            }
            if (!IFileManager::Get().FileExists(*Filename))
                return false;
            Test->AddInfo(FString::Printf(TEXT("Wind HUD capture: %s"), *Filename));
            ++Stage;
            return Stage == 16;
        }

    private:
        FAutomationTestBase* Test;
        int32 Stage = 0;
        double ReadyAt = 0;
        FString Filename;
    };
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWindIndicatorVisualTest, "UOpenGolf.Visual.WindIndicator",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FWindIndicatorVisualTest::RunTest(const FString& Parameters)
{
    UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UOpenGolf/UI/WBP_WindIndicator.WBP_WindIndicator_C"));
    if (!TestNotNull(TEXT("Wind Blueprint loads for Designer capture"), WidgetClass))
        return false;
    UUserWidget* Preview = NewObject<UUserWidget>(GetTransientPackage(), WidgetClass);
    Preview->SetDesignerFlags(EWidgetDesignFlags::Designing);
    Preview->Initialize();
    const int32 Width = RenderedPlay::CaptureSize().X, Height = RenderedPlay::CaptureSize().Y;
    FWidgetRenderer* Renderer = new FWidgetRenderer(true);
    UTextureRenderTarget2D* Target = Renderer->DrawWidget(Preview->TakeWidget(), FVector2D(Width, Height));
    TArray<FColor> Pixels;
    const bool bRead = Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
    if (TestTrue(TEXT("Designer preview renders without Play"), bRead))
    {
        TArray64<uint8> Png;
        FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
        const FString Filename = FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/WindIndicator/designer-%dx%d.png"), Width, Height);
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        TestTrue(TEXT("Designer screenshot saved"), FFileHelper::SaveArrayToFile(Png, *Filename));
        AddInfo(FString::Printf(TEXT("Designer capture: %s"), *Filename));
    }
    BeginCleanup(Renderer);
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<RenderedPlay::FStartInWindow>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCaptureWindIndicator>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
