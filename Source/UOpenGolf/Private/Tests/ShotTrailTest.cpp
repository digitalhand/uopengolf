// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "FileHelpers.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "ShaderCompiler.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfBall.h"
#include "UOpenGolfGameMode.h"
#include "RangePerformance.h"
#include "Misc/App.h"
#include "Engine/Engine.h"
#include "ProfilingDebugging/CsvProfiler.h"

class FCheckShotTrail : public IAutomationLatentCommand
{
public:
    FCheckShotTrail(FAutomationTestBase* InTest, bool bInCapture)
        : Test(InTest), bCapture(bInCapture) {}
    ~FCheckShotTrail() { RangePerformance::bEnabled = false; }
    bool Update() override
    {
        if (GetCurrentRunTime() > 540)
        {
            Test->AddError(TEXT("Shot trail test timed out"));
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        auto* Ball = Mode ? Cast<AUOpenGolfBall>(Mode->GetBall()) : nullptr;
        if (!Ball || !World->HasBegunPlay())
            return false;
        if (bCapture && !bSizedViewport)
        {
            FSceneViewport* Viewport = World->GetGameViewport() ? World->GetGameViewport()->GetGameViewport() : nullptr;
            if (!Viewport)
                return false;
            FParse::Value(FCommandLine::Get(), TEXT("RangeRenderWidth="), RenderWidth);
            FParse::Value(FCommandLine::Get(), TEXT("RangeRenderHeight="), RenderHeight);
            RenderWidth = FMath::Clamp(RenderWidth, 320, 7680);
            RenderHeight = FMath::Clamp(RenderHeight, 240, 4320);
            Viewport->SetFixedViewportSize(RenderWidth, RenderHeight);
            bSizedViewport = true;
            WarmupStarted = GetCurrentRunTime();
            return false;
        }
        if (bCapture && GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
        {
            WarmupStarted = GetCurrentRunTime();
            return false;
        }
        if (Stage == 0)
        {
            if (bCapture && GetCurrentRunTime() - WarmupStarted < 10)
                return false;
            if (bCapture)
            {
                const FIntPoint Size = World->GetGameViewport()->Viewport->GetSizeXY();
                Test->TestEqual(TEXT("Controlled render resolution"), Size, FIntPoint(RenderWidth, RenderHeight));
            }
            if (bCapture && FParse::Param(FCommandLine::Get(), TEXT("RangeCsvTiming")))
            {
                if (Shot == 0)
                    GEngine->Exec(World, TEXT("csvprofile start"));
                CSV_EVENT_GLOBAL(TEXT("RangeShotStart%d"), Shot);
            }
            RangePerformance::Reset();
            RangePerformance::bEnabled = bCapture;
            FrameSeconds = 0;
            FrameCount = 0;
            FrameTimes.Reset();
            const int32 HistoryBefore = Ball->GetHistoryTrailCount();
            Mode->ResetBallToTee();
            Test->TestEqual(TEXT("Reset clears trail"), Ball->GetTrailPointCount(), 0);
            Test->TestEqual(TEXT("Automatic re-tee keeps history tracers"), Ball->GetHistoryTrailCount(), HistoryBefore);
            FOpenFairwayLaunch Launch;
            Launch.BallSpeedMph = Shot == 0 ? 150 : Shot == 1 ? 100
                                                              : 8;
            Launch.VerticalLaunchDeg = Shot == 0 ? 12.5f : Shot == 1 ? 25
                                                                     : 0;
            Launch.bHasBackSpin = true;
            Launch.BackSpinRpm = Shot == 0 ? 2300 : Shot == 1 ? 6500
                                                              : 0;
            Test->TestTrue(TEXT("Trail test launch accepted"), Ball->Launch(Launch));
            FOpenFairwayShotResult Result;
            ShotNumbers[Shot] = Ball->GetShotResult(Result) ? Result.ShotNumber : INDEX_NONE;
            Ball->StartShotTrail();
            PreviousOpacity = Ball->TrailOpacity;
            StartTime = World->GetTimeSeconds();
            NextCapture = StartTime + 0.25;
            Stage = 1;
            return false;
        }
        FrameSeconds += FApp::GetDeltaTime();
        ++FrameCount;
        if (bCapture)
            FrameTimes.Add(FApp::GetDeltaTime() * 1000.0);
        const float Opacity = Ball->GetTrailOpacity();
        Test->TestTrue(TEXT("Opacity stays bounded"), Opacity >= 0 && Opacity <= Ball->TrailOpacity);
        Test->TestTrue(TEXT("Trail never brightens during a shot"), Opacity <= PreviousOpacity + 0.0001f);
        PreviousOpacity = Opacity;
        if (Opacity > 0 && Opacity < Ball->TrailOpacity - 0.001f)
            bSawFade = true;
        if (Ball->GetTrailPointCount() > 2)
            bSawPath = true;
        if (bCapture && !FParse::Param(FCommandLine::Get(), TEXT("RangeTrailTimingOnly")) && World->GetTimeSeconds() >= NextCapture)
        {
            const FString Filename = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/ShotTrail") /
                                                                       FString::Printf(TEXT("%s-Shot%d-%03d.png"), *RunId, Shot, Frame++));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
            FScreenshotRequest::RequestScreenshot(Filename, true, false, false, FIntRect(), true);
            Test->AddInfo(FString::Printf(TEXT("Shot %d t=%.2f speed=%.2f opacity=%.3f: %s"), Shot,
                World->GetTimeSeconds() - StartTime, Ball->GetSpeed(), Opacity, *Filename));
            NextCapture = World->GetTimeSeconds() + 0.5;
        }
        if (Ball->IsMoving())
            return false;
        if (bCapture)
        {
            if (FParse::Param(FCommandLine::Get(), TEXT("RangeCsvTiming")))
                CSV_EVENT_GLOBAL(TEXT("RangeShotEnd%d"), Shot);
            FrameTimes.Sort();
            const double P95 = FrameTimes.IsEmpty() ? 0 : FrameTimes[FMath::Clamp(FMath::CeilToInt(FrameTimes.Num() * 0.95) - 1, 0, FrameTimes.Num() - 1)];
            Test->AddInfo(FString::Printf(TEXT("Frame budget shot=%d resolution=%dx%d p95_ms=%.3f target_ms=16.667"), Shot, RenderWidth, RenderHeight, P95));
            const auto MeanUs = [](const RangePerformance::FSample& S)
            { return S.Count ? S.Seconds * 1e6 / S.Count : 0; };
            Test->AddInfo(FString::Printf(TEXT("Performance shot=%d legacy=%d frames=%d frame_ms=%.3f physics_us=%.3f trail_us=%.3f hud_us=%.3f physics_max_us=%.3f trail_max_us=%.3f"),
                Shot, FParse::Param(FCommandLine::Get(), TEXT("RangeLegacyTrail")), FrameCount,
                FrameSeconds * 1000 / FMath::Max(FrameCount, 1), MeanUs(RangePerformance::Physics), MeanUs(RangePerformance::Trail), MeanUs(RangePerformance::Hud),
                RangePerformance::Physics.MaxSeconds * 1e6, RangePerformance::Trail.MaxSeconds * 1e6));
        }
        Test->TestTrue(TEXT("Trail records path"), bSawPath);
        Test->TestTrue(TEXT("Trail fades before rest"), bSawFade);
        Test->TestEqual(TEXT("Trail invisible at rest"), Opacity, 0.0f);
        // The finished shot becomes the newest history tracer; at most two are kept.
        Test->TestEqual(TEXT("History tracer count"), Ball->GetHistoryTrailCount(), FMath::Min(Shot + 1, AUOpenGolfBall::MaxHistoryTrails));
        Test->TestEqual(TEXT("Newest history tracer is the finished shot"), Ball->GetHistoryTrailShotNumber(0), ShotNumbers[Shot]);
        if (Shot > 0)
            Test->TestEqual(TEXT("Older history tracer is the previous shot"), Ball->GetHistoryTrailShotNumber(1), ShotNumbers[Shot - 1]);
        Mode->ResetBallToTee();
        Test->TestEqual(TEXT("Reset releases path"), Ball->GetTrailPointCount(), 0);
        bSawFade = bSawPath = false;
        Stage = 0;
        if (++Shot < 3)
            return false;
        if (bCapture && FParse::Param(FCommandLine::Get(), TEXT("RangeCsvTiming")))
            GEngine->Exec(World, TEXT("csvprofile stop"));
        Ball->ClearShotTrailHistory();
        Test->TestEqual(TEXT("Clearing history removes history tracers"), Ball->GetHistoryTrailCount(), 0);
        return true;
    }

private:
    FAutomationTestBase* Test;
    bool bSizedViewport = false;
    int32 RenderWidth = 1280, RenderHeight = 720;
    TArray<double> FrameTimes;
    double WarmupStarted = 0;
    bool bCapture, bSawFade = false, bSawPath = false;
    int32 Stage = 0, Shot = 0, Frame = 0;
    int32 ShotNumbers[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
    float PreviousOpacity = 0;
    double StartTime = 0, NextCapture = 0, FrameSeconds = 0;
    int32 FrameCount = 0;
    FString RunId = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShotTrailTest, "UOpenGolf.Range.ShotTrail",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShotTrailVisualTest, "UOpenGolf.Visual.ShotTrail",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
static bool QueueTrail(FAutomationTestBase* Test, bool bCapture)
{
    FString Map = TEXT("/Game/UOpenGolf/Maps/UOpenGolf");
    if (bCapture)
        FParse::Value(FCommandLine::Get(), TEXT("RangeVisualMap="), Map);
    if (!FEditorFileUtils::LoadMap(Map, false, true))
        return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCheckShotTrail>(Test, bCapture));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
bool FShotTrailTest::RunTest(const FString&)
{
    return QueueTrail(this, false);
}
bool FShotTrailVisualTest::RunTest(const FString&)
{
    return QueueTrail(this, true);
}
#endif
