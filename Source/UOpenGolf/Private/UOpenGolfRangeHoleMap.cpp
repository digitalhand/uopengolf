// SPDX-License-Identifier: MIT
// The range's hole map: the top-down capture the HUD minimap draws, the ground sampled under it, and its finishing.
#include "UOpenGolfRange.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformTime.h"
#include "HoleMap/HoleMapBuild.h"
#include "HoleMap/HoleMapCompositor.h"
#include "HoleMap/HoleMapLook.h"
#include "Misc/App.h"
#include "OpenGolfFlagstickComponent.h"
#include "OpenGolfPlayDistance.h"
#include "OpenGolfTerrain.h"
#include "Tasks/Task.h"
#include "TextureResource.h"
#include "UOpenGolfBall.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogUOpenGolfHoleMap, Log, All);

namespace UOpenGolfRangeHoleMapDetail
{
    double MillisecondsSince(double Start) { return (FPlatformTime::Seconds() - Start) * 1000.0; }

    /** A map, not the game view: no fog or clouds over the ground, and none of the camera's lens effects. */
    void ConfigureForMap(USceneCaptureComponent2D& Capture)
    {
        FEngineShowFlags& Show = Capture.ShowFlags;
        Show.SetFog(false);
        Show.SetVolumetricFog(false);
        // Also removes the clouds' shadows from the ground.
        Show.SetCloud(false);
        Show.SetMotionBlur(false);
        Show.SetDepthOfField(false);
        Show.SetBloom(false);
        // Fixed on in Shipping builds, so the post-process settings below zero them as well.
        Show.SetLensFlares(false);
        Show.SetVignette(false);
        Show.SetGrain(false);

        FPostProcessSettings& Post = Capture.PostProcessSettings;
        Post.bOverride_LensFlareIntensity = true;
        Post.LensFlareIntensity = 0;
        Post.bOverride_VignetteIntensity = true;
        Post.VignetteIntensity = 0;
        Post.bOverride_FilmGrainIntensity = true;
        Post.FilmGrainIntensity = 0;
        // A touch more colour and contrast than the game view, as aerial photography is graded.
        Post.bOverride_ColorSaturation = true;
        Post.ColorSaturation = FVector4(1.08, 1.08, 1.08, 1);
        Post.bOverride_ColorContrast = true;
        Post.ColorContrast = FVector4(1.05, 1.05, 1.05, 1);
        Capture.PostProcessBlendWeight = 1;
    }

    /** A world XY position in the terrain actor's own frame, in metres: how the terrain definition is laid out. */
    FVector2D ToTerrainM(const FTransform& Terrain, FVector2D World)
    {
        return FVector2D(Terrain.InverseTransformPosition(FVector(World, Terrain.GetLocation().Z))) / 100.0;
    }

    /** The play regions that can affect the dimming anywhere the distance field samples Frame. */
    TSharedPtr<const FOpenGolfPlayDistance> PlayDistanceIndex(const UOpenGolfTerrainDefinition& Definition, const FTransform& Terrain,
        const FHoleMapFrame& Frame)
    {
        FBox2D Area(ForceInit);
        for (const FVector2D UV : {FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1), FVector2D(1, 1)})
            Area += ToTerrainM(Terrain, Frame.ToWorld(UV));
        // The field samples up to two spacings beyond the frame's edges.
        return MakeShared<const FOpenGolfPlayDistance>(Definition, Area.ExpandBy(3 * HoleMapLook::FieldSpacingCm / 100.0),
            HoleMapLook::PlayDistanceCapCm / 100.0);
    }

    /** The worker thread's part: distance from play (when the frame changed) and the finished picture with its mips. */
    FHoleMapFinished Finish(const FHoleMapFrame& Frame, const FHoleMapField& Heights, FHoleMapField PlayDistance,
        const FOpenGolfPlayDistance* Index, const FTransform& Terrain, TArray<FColor> Pixels)
    {
        FHoleMapFinished Result;
        double Start = FPlatformTime::Seconds();
        // Without any play regions the distance is infinite everywhere, so nothing is dimmed.
        if (Index && Index->HasPlayRegions())
        {
            PlayDistance = FHoleMapField::Build(Frame, HoleMapLook::FieldSpacingCm, [Index, &Terrain](FVector2D World, double& Distance)
            {
                Distance = Index->At(ToTerrainM(Terrain, World)) * 100.0;
                return true;
            }, EHoleMapSampling::Parallel);
        }
        Result.DistanceMs = MillisecondsSince(Start);

        Start = FPlatformTime::Seconds();
        HoleMapCompositor::FPicture Picture;
        Picture.Width = FHoleMapFrame::TextureWidth;
        Picture.Height = FHoleMapFrame::TextureHeight;
        Picture.Pixels = MoveTemp(Pixels);
        Picture = HoleMapCompositor::WithSurroundDimmed(Picture, Frame, PlayDistance);
        Picture = HoleMapCompositor::WithRelief(Picture, Frame, Heights);
        Picture = HoleMapCompositor::WithContours(Picture, Frame, Heights);
        Result.Mips = HoleMapCompositor::BuildMips(Picture);
        Result.PlayDistance = MoveTemp(PlayDistance);
        Result.FinishMs = MillisecondsSince(Start);
        return Result;
    }
} // namespace UOpenGolfRangeHoleMapDetail

void AUOpenGolfRange::InvalidateHoleMap()
{
    // A map still being built shows what was there before; it is dropped when it finishes.
    ++HoleMapGeneration;
    if (bHoleMapDirty)
        return;
    bHoleMapDirty = true;
    bHoleMapInterim = false;
    HoleMapStaleSince = FPlatformTime::Seconds();
}

bool AUOpenGolfRange::IsSceneReadyForHoleMap()
{
    if (IsAsyncLoading())
        return false;
#if WITH_EDITOR
    // Editor builds (the editor, and -game runs of it) rebuild reloaded textures, meshes and shaders after level
    // travel; until they finish, the renderer draws checkerboard placeholders, which a one-off capture would keep.
    if (FAssetCompilingManager::Get().GetNumRemainingAssets() > 0)
        return false;
    if (GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0)
        return false;
#endif
    return true;
}

void AUOpenGolfRange::UpdateHoleMap()
{
    if (HoleMapBuild)
    {
        ContinueHoleMap();
    }
    else if (bHoleMapDirty)
    {
        const double Waited = FPlatformTime::Seconds() - HoleMapStaleSince;
        const bool bReady = IsSceneReadyForHoleMap();
        // Not ready: wait, up to the time limit, after which one interim picture beats none.
        if (bReady || (!bHoleMapInterim && Waited >= HoleMapReadyTimeoutS))
            StartHoleMap(bReady, Waited);
    }
    // A build takes a step per tick (capture, read-back, worker, publish). At the idle interval the new hole's map would
    // reach the minimap about 0.75 s late, so the range ticks every frame until it is published.
    const float Interval = HoleMapBuild || bHoleMapDirty ? 0.0f : IdleTickIntervalS;
    if (GetActorTickInterval() != Interval)
        SetActorTickInterval(Interval);
}

void AUOpenGolfRange::StartHoleMap(bool bReady, double WaitedS)
{
    using namespace UOpenGolfRangeHoleMapDetail;
    const double Start = FPlatformTime::Seconds();
    FHoleMapFrame Frame = FHoleMapFrame::FromHole(GetTeeLocation(), GetOriginalCup().Position, MapEndMargin, MapMinimumWidth, MapSideOffset);
    for (const FVector& Point : CourseRouting)
    {
        const FVector2D Delta(Point - Frame.Center);
        Frame.Length = FMath::Max3(Frame.Length, 2 * (FMath::Abs(Delta | Frame.Up) + MapEndMargin),
            2 * (FMath::Abs(Delta | Frame.Right()) + MapEndMargin) / FHoleMapFrame::Aspect);
    }
    bHoleMapDirty = !bReady;
    bHoleMapInterim = !bReady;
    if (bReady)
    {
        UE_LOG(LogUOpenGolfHoleMap, Log, TEXT("Hole map capture started %.2f s after it went stale."), WaitedS);
    }
    else
    {
        UE_LOG(LogUOpenGolfHoleMap, Warning, TEXT("Hole map captured after %.0f s with the scene still loading; it is captured again once ready."), WaitedS);
    }
    const bool bResample = !HoleMapHeights.IsValid() || !Frame.Equals(HoleMapFrame);
    // Without a renderer there is no picture, but the minimap's height labels still read the ground.
    if (!FApp::CanEverRender())
    {
        if (bResample)
            HoleMapHeights = SampleHoleMapHeights(Frame);
        HoleMapFrame = Frame;
        return;
    }

    const TSharedRef<FHoleMapBuild> Build = MakeShared<FHoleMapBuild>();
    Build->Generation = HoleMapGeneration;
    Build->StartedAt = Start;
    Build->Frame = Frame;
    if (bResample)
    {
        Build->Heights = SampleHoleMapHeights(Frame);
        if (IsValid(Terrain) && Terrain->Definition)
        {
            Build->TerrainTransform = Terrain->GetActorTransform();
            Build->PlayDistanceIndex = PlayDistanceIndex(*Terrain->Definition, Build->TerrainTransform, Frame);
        }
    }
    else
    {
        Build->Heights = HoleMapHeights;
        Build->PlayDistance = HoleMapPlayDistance;
    }
    EnsureHoleMapCapture();
    HoleMapCapture->OrthoWidth = Frame.Length * FHoleMapFrame::Aspect;
    HoleMapCapture->MaxViewDistanceOverride = Frame.Length * 3;
    HoleMapCapture->SetWorldLocationAndRotation(Frame.Center + FVector::UpVector * Frame.Length,
        FRotator(-90, FVector(Frame.Up, 0).Rotation().Yaw, 0));
    HoleMapCapture->CaptureScene();
    Build->Readback.Request(*HoleMapCaptureTarget);
    Build->GameThreadMs = MillisecondsSince(Start);
    HoleMapBuild = Build;
}

void AUOpenGolfRange::ContinueHoleMap()
{
    using namespace UOpenGolfRangeHoleMapDetail;
    FHoleMapBuild& Build = *HoleMapBuild;
    const double Start = FPlatformTime::Seconds();
    if (!Build.Finishing.IsValid())
    {
        TArray<FColor> Pixels;
        const FHoleMapReadback::EStatus Status = Build.Readback.Poll(Pixels);
        if (Status == FHoleMapReadback::EStatus::Pending)
            return;
        if (Status != FHoleMapReadback::EStatus::Ready)
        {
            // An unexpected target format: read it the old way, which waits for the GPU.
            FTextureRenderTargetResource* Target = HoleMapCaptureTarget ? HoleMapCaptureTarget->GameThread_GetRenderTargetResource() : nullptr;
            if (!Target || !Target->ReadPixels(Pixels))
                Pixels.Reset();
        }
        if (Pixels.Num() != FHoleMapFrame::TextureWidth * FHoleMapFrame::TextureHeight)
        {
            UE_LOG(LogUOpenGolfHoleMap, Warning, TEXT("Hole map capture could not be read back; the minimap keeps its previous picture."));
            HoleMapBuild.Reset();
            return;
        }
        Build.PixelsMs = MillisecondsSince(Build.StartedAt);
        // The worker gets copies only: the range may be gone, or have started another map, before it finishes.
        Build.Finishing = UE::Tasks::Launch(UE_SOURCE_LOCATION,
            [Frame = Build.Frame, Heights = Build.Heights, PlayDistance = Build.PlayDistance, Index = Build.PlayDistanceIndex,
                Terrain = Build.TerrainTransform, Pixels = MoveTemp(Pixels), LaunchedAt = FPlatformTime::Seconds()]() mutable
            {
                const double WaitedMs = MillisecondsSince(LaunchedAt);
                FHoleMapFinished Finished = Finish(Frame, Heights, MoveTemp(PlayDistance), Index.Get(), Terrain, MoveTemp(Pixels));
                Finished.WaitedMs = WaitedMs;
                return Finished;
            });
        Build.GameThreadMs = FMath::Max(Build.GameThreadMs, MillisecondsSince(Start));
        return;
    }
    if (!Build.Finishing.IsCompleted())
        return;
    FHoleMapFinished Finished = MoveTemp(Build.Finishing.GetResult());
    if (Build.Generation != HoleMapGeneration)
    {
        UE_LOG(LogUOpenGolfHoleMap, Log, TEXT("Hole map dropped: what it shows changed while it was finished."));
        // The interim wait applies to a published picture; this one never was.
        bHoleMapInterim = false;
        HoleMapBuild.Reset();
        return;
    }
    HoleMapPicture = HoleMapCompositor::MakeTexture(Finished.Mips, TEXT("HoleMapPicture"));
    HoleMapFrame = Build.Frame;
    HoleMapHeights = MoveTemp(Build.Heights);
    HoleMapPlayDistance = MoveTemp(Finished.PlayDistance);
    HoleMapGameThreadMs = FMath::Max(Build.GameThreadMs, MillisecondsSince(Start));
    UE_LOG(LogUOpenGolfHoleMap, Log, TEXT("Hole map published %.0f ms after its capture; pixels read back after %.0f ms. "
        "Worker: waited %.0f ms, distance from play %.0f ms, picture %.0f ms. Longest game-thread step %.1f ms."),
        MillisecondsSince(Build.StartedAt), Build.PixelsMs, Finished.WaitedMs, Finished.DistanceMs, Finished.FinishMs, HoleMapGameThreadMs);
    HoleMapBuild.Reset();
}

void AUOpenGolfRange::EnsureHoleMapCapture()
{
    if (!HoleMapCaptureTarget)
    {
        HoleMapCaptureTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("HoleMap"), RF_Transient);
        HoleMapCaptureTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
        HoleMapCaptureTarget->InitAutoFormat(FHoleMapFrame::TextureWidth, FHoleMapFrame::TextureHeight);
        HoleMapCaptureTarget->UpdateResourceImmediate();
    }
    if (!HoleMapCapture)
    {
        // Exposure comes from the range's unbound post-process volume, as in the game view.
        HoleMapCapture = NewObject<USceneCaptureComponent2D>(this, TEXT("HoleMapCapture"), RF_Transient);
        HoleMapCapture->bCaptureEveryFrame = false;
        HoleMapCapture->bCaptureOnMovement = false;
        HoleMapCapture->ProjectionType = ECameraProjectionMode::Orthographic;
        HoleMapCapture->CaptureSource = SCS_FinalColorLDR;
        HoleMapCapture->TextureTarget = HoleMapCaptureTarget;
        UOpenGolfRangeHoleMapDetail::ConfigureForMap(*HoleMapCapture);
        // The minimap draws the tee and flag itself; the ball and gimmie ring are hidden in scene captures.
        HoleMapCapture->HiddenActors.Add(this);
        if (IsValid(Flagstick))
            HoleMapCapture->HiddenActors.Add(Flagstick->GetOwner());
        HoleMapCapture->RegisterComponent();
    }
}

FHoleMapField AUOpenGolfRange::SampleHoleMapHeights(const FHoleMapFrame& Frame) const
{
    const AOpenGolfTerrainArea* Area = Terrain;
    if (!IsValid(Area))
        return FHoleMapField();
    const UOpenGolfTerrainDefinition* Definition = Area->Definition;
    // The definition's heights and outlines are in metres, in the terrain actor's own frame.
    const FTransform ToWorld = Area->GetActorTransform();
    if (Definition)
    {
        // The analytic ground the Landscape and detail mesh are built from: pure, so sampled in parallel.
        return FHoleMapField::Build(Frame, HoleMapLook::FieldSpacingCm, [Definition, &ToWorld](FVector2D World, double& Height)
        {
            const FVector2D Local = UOpenGolfRangeHoleMapDetail::ToTerrainM(ToWorld, World);
            Height = ToWorld.TransformPosition(FVector(Local * 100.0, Definition->HeightAt(Local) * 100.0)).Z;
            return true;
        }, EHoleMapSampling::Parallel);
    }
    // A course without a definition (a Landscape only): trace the ground the ball lands on, on the game thread.
    const double ReferenceZ = Frame.Center.Z;
    return FHoleMapField::Build(Frame, HoleMapLook::FieldSpacingCm, [Area, ReferenceZ](FVector2D World, double& Height)
    {
        FVector Ground, Normal;
        if (!Area->GroundAt(FVector(World, ReferenceZ), Ground, Normal, AUOpenGolfBall::ContactChannel))
            return false;
        Height = Ground.Z;
        return true;
    });
}
