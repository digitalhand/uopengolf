// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/DisplayOptions.h"
#include "OpenGolfGameUserSettings.h"

namespace DisplayOptionsTestDetail
{
    using namespace DisplayOptions;
    FFacts LinuxUltrawide()
    {
        FFacts F; F.bBorderlessIsFullscreen=true; F.Desktop={3440,1440}; F.WorkArea={3440,1394}; return F;
    }
    FFacts WindowsQhd()
    {
        FFacts F; F.bBorderlessIsFullscreen=false; F.RhiModes={{1280,720},{1920,1080},{2560,1440}}; F.Desktop={2560,1440}; F.WorkArea={2560,1400}; return F;
    }
    FString Describe(const TArray<FIntPoint>& Sizes)
    {
        FString S; for(const FIntPoint& P:Sizes) S+=FString::Printf(TEXT("%dx%d "),P.X,P.Y); return S;
    }
    bool Same(FAutomationTestBase* T,const TCHAR* What,const TArray<FIntPoint>& Actual,const TArray<FIntPoint>& Expected)
    {
        return T->TestEqual(FString(What)+TEXT(": ")+Describe(Actual),Actual,Expected);
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplayResolutionsTest,"UOpenGolf.Menu.DisplayResolutions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDisplayResolutionsTest::RunTest(const FString&)
{
    using namespace DisplayOptions;
    using namespace DisplayOptionsTestDetail;
    const FFacts Linux=LinuxUltrawide(), Windows=WindowsQhd();
    const FIntPoint SavedByV006(3440,1394);

    // Linux, 3440x1440 with a panel: Fullscreen renders at aspect-matched sizes up to the desktop.
    Same(this,TEXT("Linux fullscreen"),Resolutions(Fullscreen,Linux,SavedByV006),{{2560,1080},{3440,1440}});
    TestEqual(TEXT("Saved work-area size snaps to the desktop"),Choose(Resolutions(Fullscreen,Linux,SavedByV006),SavedByV006),FIntPoint(3440,1440));
    const TArray<FIntPoint> Windowed=Resolutions(DisplayOptions::Windowed,Linux,SavedByV006);
    Same(this,TEXT("Linux windowed"),Windowed,{{1280,720},{1280,800},{1366,768},{1440,900},{1600,900},{1680,1050},{1920,1080},{1920,1200},{2560,1080}});
    for(const FIntPoint& P:Windowed) TestTrue(TEXT("Window fits inside the work area"),P.X<3440 && P.Y<1394);

    // Windows, 2560x1440: exclusive fullscreen keeps the display's modes; Borderless renders at aspect-matched sizes.
    Same(this,TEXT("Windows fullscreen keeps a current size that fits"),Resolutions(Fullscreen,Windows,{1600,900}),{{1280,720},{1600,900},{1920,1080},{2560,1440}});
    Same(this,TEXT("Windows fullscreen drops a current size above the desktop"),Resolutions(Fullscreen,Windows,{3840,2160}),{{1280,720},{1920,1080},{2560,1440}});
    Same(this,TEXT("Windows borderless"),Resolutions(Borderless,Windows,{2560,1440}),{{1280,720},{1366,768},{1600,900},{1920,1080},{2560,1440}});
    Same(this,TEXT("Windows windowed"),Resolutions(DisplayOptions::Windowed,Windows,{2560,1440}),{{1280,720},{1280,800},{1366,768},{1440,900},{1600,900},{1680,1050},{1920,1080},{1920,1200}});
    FFacts Small=Windows; Small.WorkArea={1024,768};
    TestEqual(TEXT("No window fits a small work area"),Resolutions(DisplayOptions::Windowed,Small,{1024,768}).Num(),0);

    // Which modes are desktop-sized, and so rendered at the chosen size and scaled by the GPU.
    TestFalse(TEXT("Windows fullscreen is exclusive"),DesktopSized(Fullscreen,Windows));
    TestTrue(TEXT("Windows borderless is desktop-sized"),DesktopSized(Borderless,Windows));
    TestFalse(TEXT("Windowed is not desktop-sized"),DesktopSized(DisplayOptions::Windowed,Windows));
    TestTrue(TEXT("Linux fullscreen is desktop-sized"),DesktopSized(Fullscreen,Linux));
    FFacts NoModes=Windows; NoModes.RhiModes.Empty();
    TestTrue(TEXT("Fullscreen without display modes is desktop-sized"),DesktopSized(Fullscreen,NoModes));
    // Without somewhere to store a render resolution, a desktop-sized mode can only render at the desktop.
    FFacts NoStore=Linux; NoStore.bRenderResolution=false;
    Same(this,TEXT("Desktop only without render-resolution storage"),Resolutions(Fullscreen,NoStore,{1920,1080}),{{3440,1440}});
    Same(this,TEXT("Windowed is unaffected by render-resolution storage"),Resolutions(DisplayOptions::Windowed,NoStore,{1920,1080}),Windowed);

    // Mode lists and the mapping between shown and engine modes.
    TestEqual(TEXT("Linux offers Fullscreen and Windowed"),Modes(true).Values,TArray<int32>({0,2}));
    TestEqual(TEXT("Linux labels"),Modes(true).Labels,TArray<FString>({TEXT("Fullscreen"),TEXT("Windowed")}));
    TestEqual(TEXT("Windows offers three modes"),Modes(false).Values,TArray<int32>({0,1,2}));
    TestEqual(TEXT("Saved borderless shows as Fullscreen on Linux"),ShownMode(1,true),0);
    TestEqual(TEXT("Saved fullscreen shows as Fullscreen on Linux"),ShownMode(0,true),0);
    TestEqual(TEXT("Saved windowed shows as Windowed on Linux"),ShownMode(2,true),2);
    TestEqual(TEXT("Saved borderless shows as Borderless on Windows"),ShownMode(1,false),1);
    TestEqual(TEXT("Linux Fullscreen is sent as WindowedFullscreen"),EngineMode(0,true),1);
    TestEqual(TEXT("Linux Windowed is sent as Windowed"),EngineMode(2,true),2);
    TestEqual(TEXT("Windows Fullscreen is sent as Fullscreen"),EngineMode(0,false),0);
    TestEqual(TEXT("Windows Borderless is sent as WindowedFullscreen"),EngineMode(1,false),1);

    // Requests sent to the engine.
    FRequest R=Plan(Fullscreen,{2560,1080},Linux);
    TestEqual(TEXT("Linux fullscreen engine mode"),R.EngineMode,1);TestEqual(TEXT("Linux fullscreen window is the desktop"),R.ScreenResolution,FIntPoint(3440,1440));
    TestTrue(TEXT("Linux fullscreen renders at the choice"),R.RenderResolution.IsSet() && R.RenderResolution.GetValue()==FIntPoint(2560,1080));
    R=Plan(DisplayOptions::Windowed,{1920,1080},Linux);
    TestEqual(TEXT("Windowed engine mode"),R.EngineMode,2);TestEqual(TEXT("Windowed size is the choice"),R.ScreenResolution,FIntPoint(1920,1080));TestFalse(TEXT("Windowed has no render override"),R.RenderResolution.IsSet());
    R=Plan(Fullscreen,{1920,1080},Windows);
    TestEqual(TEXT("Exclusive engine mode"),R.EngineMode,0);TestEqual(TEXT("Exclusive mode size is the choice"),R.ScreenResolution,FIntPoint(1920,1080));TestFalse(TEXT("Exclusive has no render override"),R.RenderResolution.IsSet());
    R=Plan(Borderless,{1920,1080},Windows);
    TestEqual(TEXT("Borderless engine mode"),R.EngineMode,1);TestEqual(TEXT("Borderless window is the desktop"),R.ScreenResolution,FIntPoint(2560,1440));
    TestTrue(TEXT("Borderless renders at the choice"),R.RenderResolution.IsSet() && R.RenderResolution.GetValue()==FIntPoint(1920,1080));

    // Choosing within a list.
    TestEqual(TEXT("Offered size is kept"),Choose({{1280,720},{1920,1080}},{1280,720}),FIntPoint(1280,720));
    TestEqual(TEXT("Unoffered size becomes the largest"),Choose({{1920,1080},{1280,720}},{1600,900}),FIntPoint(1920,1080));
    TestEqual(TEXT("Empty list keeps the current size"),Choose({},{1600,900}),FIntPoint(1600,900));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRenderResolutionTest,"UOpenGolf.Menu.RenderResolution",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRenderResolutionTest::RunTest(const FString&)
{
    // Settings → Video needs the project class, which DefaultEngine.ini selects with GameUserSettingsClassName.
    TestNotNull(TEXT("The engine uses the project settings class"),GEngine?Cast<UOpenGolfGameUserSettings>(GEngine->GetGameUserSettings()):nullptr);
    auto* S=NewObject<UOpenGolfGameUserSettings>();
    S->SetFullscreenMode(EWindowMode::WindowedFullscreen);
    TestFalse(TEXT("No render resolution by default"),S->UsesRenderResolution());
    S->SetRenderResolution({1920,1080});
    TestTrue(TEXT("Render resolution in use"),S->UsesRenderResolution());
    TestEqual(TEXT("Render resolution read back"),S->GetRenderResolution(),FIntPoint(1920,1080));
    // Half the desktop, same aspect, is exactly half the render scale wherever the test runs.
    const FIntPoint Desktop=S->GetDesktopResolution();
    if(Desktop.X>=1280 && Desktop.Y>=720)
    {
        S->SetRenderResolution(Desktop/2);
        float Normalized,Current,Min,Max;S->GetResolutionScaleInformationEx(Normalized,Current,Min,Max);
        TestEqual(TEXT("Half the desktop renders at 50%"),Current,50.f,0.5f);
    }
    else AddInfo(TEXT("Desktop resolution unavailable; render-scale derivation not checked."));
    S->ClearRenderResolution();
    TestFalse(TEXT("Render resolution cleared"),S->UsesRenderResolution());
    TestEqual(TEXT("Falls back to the screen resolution"),S->GetRenderResolution(),S->GetScreenResolution());
    return true;
}
#endif
