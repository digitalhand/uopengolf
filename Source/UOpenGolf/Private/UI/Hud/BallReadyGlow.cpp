// SPDX-License-Identifier: MIT
#include "UI/Hud/BallReadyGlow.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "OpenFairwayBall.h"
#include "Rendering/DrawElements.h"
#include "UI/Common/HudStyle.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"

namespace BallReadyGlowDetail
{
    /** One stroke of the glow: its distance outside the ball's edge, its width and its opacity, in local units. */
    struct FRing
    {
        float Offset;
        float Width;
        float Alpha;
    };

    /** Outermost first, so the bright core draws last: a thin line on the edge fading out over a few units. */
    constexpr FRing Rings[] = {{6.0f, 4.0f, 0.16f}, {3.0f, 3.0f, 0.45f}, {1.0f, 2.0f, 0.95f}};
    constexpr int32 Segments = 32;
    /** A far ball still gets a visible glow. */
    constexpr float MinRadius = 3.0f;
} // namespace BallReadyGlowDetail

void SBallReadyGlow::Construct(const FArguments& Args)
{
    Controller = Args._Controller;
    Ready = Args._Ready;
    SetTag(TEXT("BallReadyGlow"));
    SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SBallReadyGlow::GetGlowVisibility));
    // Repainted every frame: the camera moves without anything in the widget changing.
    ForceVolatile(true);
}

EVisibility SBallReadyGlow::GetGlowVisibility() const
{
    const AUOpenGolfPlayerController* Player = Controller.Get();
    const UWorld* World = Player ? Player->GetWorld() : nullptr;
    const AUOpenGolfGameMode* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    // A placed (practice) ball is never armed by the launch monitor, so it gets no glow.
    return Mode && Mode->IsBallAtAddress() && !Mode->IsPractice() && !Player->IsPlacingBall() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

bool SBallReadyGlow::FindBall(const FGeometry& Geometry, FVector2D& OutCentre, float& OutRadius) const
{
    const AUOpenGolfPlayerController* Player = Controller.Get();
    const UWorld* World = Player ? Player->GetWorld() : nullptr;
    const AUOpenGolfGameMode* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    const USceneComponent* Mesh = Ball ? Ball->GetRootComponent() : nullptr;
    if (!Mesh || !Player->PlayerCameraManager)
        return false;

    // The mesh's bounds include the ball's visual scale; a sphere's outline is a circle of its radius around its centre.
    const FVector Centre = Mesh->Bounds.Origin;
    const FVector Edge = Centre + Player->PlayerCameraManager->GetCameraRotation().Quaternion().GetRightVector() * Mesh->Bounds.SphereRadius;
    FVector2D CentrePixels;
    FVector2D EdgePixels;
    int32 ViewportX = 0;
    int32 ViewportY = 0;
    Player->GetViewportSize(ViewportX, ViewportY);
    if (ViewportX <= 0 || ViewportY <= 0 || !Player->ProjectWorldLocationToScreen(Centre, CentrePixels, true) ||
        !Player->ProjectWorldLocationToScreen(Edge, EdgePixels, true))
        return false;

    // Viewport pixels to this widget's units, whatever DPI scale it sits under.
    const FVector2D PixelToLocal = Geometry.GetLocalSize() / FVector2D(ViewportX, ViewportY);
    OutCentre = CentrePixels * PixelToLocal;
    OutRadius = FMath::Max(static_cast<float>(FVector2D::Distance(CentrePixels, EdgePixels) * PixelToLocal.X), BallReadyGlowDetail::MinRadius);
    return true;
}

int32 SBallReadyGlow::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    using namespace BallReadyGlowDetail;
    FVector2D Centre;
    float Radius = 0.0f;
    if (!FindBall(AllottedGeometry, Centre, Radius))
        return LayerId;

    const FLinearColor Color = HudStyle::BallReadyColor(Ready.Get());
    TArray<FVector2f> Points;
    Points.SetNumUninitialized(Segments + 1);
    for (const FRing& Ring : Rings)
    {
        const float RingRadius = Radius + Ring.Offset;
        for (int32 Index = 0; Index <= Segments; ++Index)
        {
            const float Angle = 2.0f * UE_PI * Index / Segments;
            Points[Index] = FVector2f(Centre) + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * RingRadius;
        }
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
            Color.CopyWithNewOpacity(Ring.Alpha) * InWidgetStyle.GetColorAndOpacityTint(), true, Ring.Width);
    }
    return LayerId;
}
