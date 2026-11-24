// SPDX-License-Identifier: MIT
// The range's hole map: the top-down capture the HUD minimap draws, the ground sampled under it, and its finishing.
#include "UOpenGolfRange.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformTime.h"
#include "HoleMap/HoleMapCompositor.h"
#include "HoleMap/HoleMapLook.h"
#include "Misc/App.h"
#include "OpenGolfFlagstickComponent.h"
#include "OpenGolfTerrain.h"
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
} // namespace UOpenGolfRangeHoleMapDetail

void AUOpenGolfRange::InvalidateHoleMap()
{
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
    if (!bHoleMapDirty)
        return;
    const double Waited = FPlatformTime::Seconds() - HoleMapStaleSince;
    const bool bReady = IsSceneReadyForHoleMap();
    // Not ready: wait, up to the time limit, after which one interim picture beats none.
    if (!bReady && (bHoleMapInterim || Waited < HoleMapReadyTimeoutS))
        return;
    CaptureHoleMap();
    bHoleMapDirty = !bReady;
    bHoleMapInterim = !bReady;
    if (bReady)
    {
        UE_LOG(LogUOpenGolfHoleMap, Log, TEXT("Hole map captured %.2f s after it went stale."), Waited);
    }
    else
    {
        UE_LOG(LogUOpenGolfHoleMap, Warning, TEXT("Hole map captured after %.0f s with the scene still loading; it is captured again once ready."), Waited);
    }
}

void AUOpenGolfRange::CaptureHoleMap()
{
    const FHoleMapFrame Frame = FHoleMapFrame::FromHole(GetTeeLocation(), GetOriginalCup().Position, MapEndMargin, MapMinimumWidth, MapSideOffset);
    const bool bResample = !HoleMapHeights.IsValid() || !Frame.Equals(HoleMapFrame);
    HoleMapFrame = Frame;
    // Without a renderer there is no picture, but the minimap's height labels still read the ground.
    if (bResample)
        SampleHoleMapFields();
    if (!FApp::CanEverRender())
        return;

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
    HoleMapCapture->OrthoWidth = HoleMapFrame.Length * FHoleMapFrame::Aspect;
    HoleMapCapture->MaxViewDistanceOverride = HoleMapFrame.Length * 3;
    HoleMapCapture->SetWorldLocationAndRotation(HoleMapFrame.Center + FVector::UpVector * HoleMapFrame.Length,
        FRotator(-90, FVector(HoleMapFrame.Up, 0).Rotation().Yaw, 0));
    HoleMapCapture->CaptureScene();
    FinishHoleMapPicture();
}

void AUOpenGolfRange::SampleHoleMapFields()
{
    using UOpenGolfRangeHoleMapDetail::MillisecondsSince;
    HoleMapHeights = FHoleMapField();
    HoleMapPlayDistance = FHoleMapField();
    const AOpenGolfTerrainArea* Area = Terrain;
    if (!IsValid(Area))
        return;
    const UOpenGolfTerrainDefinition* Definition = Area->Definition;
    // The definition's heights and outlines are in metres, in the terrain actor's own frame.
    const FTransform ToWorld = Area->GetActorTransform();
    const auto ToLocalM = [&ToWorld](FVector2D World) { return FVector2D(ToWorld.InverseTransformPosition(FVector(World, ToWorld.GetLocation().Z))) / 100.0; };

    double Start = FPlatformTime::Seconds();
    if (Definition)
    {
        // The analytic ground the Landscape and detail mesh are built from: pure, so sampled in parallel.
        HoleMapHeights = FHoleMapField::Build(HoleMapFrame, HoleMapLook::FieldSpacingCm, [Definition, &ToWorld, &ToLocalM](FVector2D World, double& Height)
        {
            const FVector2D Local = ToLocalM(World);
            Height = ToWorld.TransformPosition(FVector(Local * 100.0, Definition->HeightAt(Local) * 100.0)).Z;
            return true;
        }, EHoleMapSampling::Parallel);
    }
    else
    {
        // A course without a definition (a Landscape only): trace the ground the ball lands on, on the game thread.
        const double ReferenceZ = HoleMapFrame.Center.Z;
        HoleMapHeights = FHoleMapField::Build(HoleMapFrame, HoleMapLook::FieldSpacingCm, [Area, ReferenceZ](FVector2D World, double& Height)
        {
            FVector Ground, Normal;
            if (!Area->GroundAt(FVector(World, ReferenceZ), Ground, Normal, AUOpenGolfBall::ContactChannel))
                return false;
            Height = Ground.Z;
            return true;
        });
    }
    const double HeightsMs = MillisecondsSince(Start);

    Start = FPlatformTime::Seconds();
    if (Definition)
    {
        HoleMapPlayDistance = FHoleMapField::Build(HoleMapFrame, HoleMapLook::FieldSpacingCm, [Definition, &ToLocalM](FVector2D World, double& Distance)
        {
            Distance = Definition->DistanceFromPlay(ToLocalM(World)) * 100.0;
            return FMath::IsFinite(Distance);
        }, EHoleMapSampling::Parallel);
    }
    UE_LOG(LogUOpenGolfHoleMap, Log, TEXT("Hole map ground sampled: heights %s in %.0f ms (%s), distance from play %s in %.0f ms."),
        HoleMapHeights.IsValid() ? TEXT("found") : TEXT("missing"), HeightsMs, Definition ? TEXT("terrain definition") : TEXT("traced"),
        HoleMapPlayDistance.IsValid() ? TEXT("found") : TEXT("missing"), MillisecondsSince(Start));
}

void AUOpenGolfRange::FinishHoleMapPicture()
{
    using UOpenGolfRangeHoleMapDetail::MillisecondsSince;
    double Start = FPlatformTime::Seconds();
    FTextureRenderTargetResource* Target = HoleMapCaptureTarget ? HoleMapCaptureTarget->GameThread_GetRenderTargetResource() : nullptr;
    HoleMapCompositor::FPicture Picture;
    Picture.Width = FHoleMapFrame::TextureWidth;
    Picture.Height = FHoleMapFrame::TextureHeight;
    // Waits for the capture to render: once per capture, never per frame.
    if (!Target || !Target->ReadPixels(Picture.Pixels) || Picture.Pixels.Num() != Picture.Width * Picture.Height)
    {
        UE_LOG(LogUOpenGolfHoleMap, Warning, TEXT("Hole map capture could not be read back; the minimap keeps its previous picture."));
        return;
    }
    const double ReadBackMs = MillisecondsSince(Start);

    Start = FPlatformTime::Seconds();
    Picture = HoleMapCompositor::WithSurroundDimmed(Picture, HoleMapFrame, HoleMapPlayDistance);
    Picture = HoleMapCompositor::WithRelief(Picture, HoleMapFrame, HoleMapHeights);
    Picture = HoleMapCompositor::WithContours(Picture, HoleMapFrame, HoleMapHeights);
    HoleMapPicture = HoleMapCompositor::MakeTexture(HoleMapCompositor::BuildMips(Picture), TEXT("HoleMapPicture"));
    UE_LOG(LogUOpenGolfHoleMap, Log, TEXT("Hole map picture read back in %.0f ms (capture render included) and finished in %.0f ms."), ReadBackMs,
        MillisecondsSince(Start));
}
