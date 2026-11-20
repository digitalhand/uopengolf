// SPDX-License-Identifier: MIT
#include "UI/Hud/HoleMinimap.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "OpenFairwayBall.h"
#include "UI/Common/HudPen.h"
#include "UI/Common/HudToolTip.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"
#include "Widgets/SToolTip.h"

void SHoleMinimap::Construct(const FArguments& Args)
{
    Controller = Args._Controller;
    SetTag(TEXT("HoleMinimap"));
    SetClipping(EWidgetClipping::ClipToBounds);
    SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SHoleMinimap::GetMapVisibility));
    SetToolTip(HudToolTip::Make(FText::FromString(TEXT("Click to aim from the ball"))));
#if WITH_ACCESSIBILITY
    SetAccessibleBehavior(EAccessibleBehavior::Custom, FText::FromString(TEXT("Hole map")));
#endif
    Aerial.DrawAs = ESlateBrushDrawType::Image;
}

const AUOpenGolfGameMode* SHoleMinimap::GetMode() const
{
    const UWorld* World = Controller.IsValid() ? Controller->GetWorld() : nullptr;
    return World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
}

const AUOpenGolfRange* SHoleMinimap::GetRange() const
{
    const AUOpenGolfGameMode* Mode = GetMode();
    return Mode ? Mode->GetRange() : nullptr;
}

EVisibility SHoleMinimap::GetMapVisibility() const
{
    // Read from the range, not the brush: a hidden widget is not ticked, so the brush would never catch up. Hidden, not
    // Collapsed: SScaleBox stops updating a collapsed child (SScaleBox::CustomPrepass), so it could never come back.
    const AUOpenGolfRange* Range = GetRange();
    return Range && Range->GetHoleMapTexture() ? EVisibility::Visible : EVisibility::Hidden;
}

void SHoleMinimap::Tick(const FGeometry&, const double, const float)
{
    // The range captures once per Play (and on grass changes), so this is a pointer compare almost every frame.
    const AUOpenGolfRange* Range = GetRange();
    UObject* Picture = Range ? Range->GetHoleMapTexture() : nullptr;
    if (Aerial.GetResourceObject() != Picture)
        Aerial.SetResourceObject(Picture);
}

int32 SHoleMinimap::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out,
    int32 Layer, const FWidgetStyle&, bool) const
{
    const AUOpenGolfGameMode* Mode = GetMode();
    const AUOpenGolfRange* Range = Mode ? Mode->GetRange() : nullptr;
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    if (!Range || !Ball || !Aerial.GetResourceObject())
        return Layer;

    // A scene capture's alpha is not opacity, so the picture is drawn opaque.
    FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(), &Aerial, ESlateDrawEffect::IgnoreTextureAlpha);

    const FHoleMapFrame& Frame = Range->GetHoleMapFrame();
    const FVector2f Size = Geometry.GetLocalSize();
    const auto ToMap = [&Frame, &Size](const FVector& World) { return FVector2f(Frame.ToUV(FVector2D(World))) * Size; };
    const FLinearColor Ink = HudStyle::MapInkColor();
    const FLinearColor White = FLinearColor::White;
    // Every mark is outlined in ink: outlines on the layer above the picture, marks above them, the ball dot on top.
    const FHudPen Pen{Geometry, Out, Layer + 1};
    const int32 TopLayer = Layer + 3;

    Pen.Line({{1, 1}, {Size.X - 1, 1}, Size - FVector2f(1, 1), {1, Size.Y - 1}, {1, 1}}, Ink, 2);
    const FVector2f Tee = ToMap(Range->GetTeeLocation());
    Pen.OutlinedLine({Tee - FVector2f(6, 0), Tee + FVector2f(6, 0)}, Ink, 5, White, 2);
    const FVector2f Pin = ToMap(Mode->GetCupLocation());
    Pen.OutlinedLine({Pin, Pin + FVector2f(0, -13), Pin + FVector2f(8, -10), Pin + FVector2f(0, -7)}, Ink, 4, HudStyle::MapFlagColor(), 2);

    const FVector2f Position = ToMap(Ball->GetActorLocation());
    const FVector2f Marker(FMath::Clamp(Position.X, EdgeInset, Size.X - EdgeInset), FMath::Clamp(Position.Y, EdgeInset, Size.Y - EdgeInset));
    if (!Position.Equals(Marker))
    {
        // Off the map: an arrow on the edge, pointing at the ball.
        const FVector2f Direction = (Position - Marker).GetSafeNormal();
        const FVector2f Side(-Direction.Y, Direction.X);
        Pen.OutlinedLine({Marker - Direction * 5 + Side * 4, Marker + Direction * 4, Marker - Direction * 5 - Side * 4}, Ink, 5, White, 2);
        return TopLayer;
    }
    if (Mode->IsBallAtAddress())
    {
        const FVector Ahead = Ball->GetActorLocation() + FRotator(0, Ball->AimYawDeg, 0).Vector() * 100;
        const FVector2f Tip = Marker + (ToMap(Ahead) - Marker).GetSafeNormal() * AimLineLength;
        Pen.OutlinedLine({Marker, Tip}, Ink, 3, White, 1);
    }
    FSlateDrawElement::MakeBox(Out, TopLayer,
        Geometry.ToPaintGeometry(FVector2f(BallDotSize), FSlateLayoutTransform(Marker - FVector2f(BallDotSize * 0.5f))), &BallDot);
    return TopLayer;
}

FReply SHoleMinimap::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    // Only the left button aims; the others reach the game, where a right click cancels ball placement.
    const AUOpenGolfRange* Range = GetRange();
    if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !Range || !Controller.IsValid())
        return FReply::Unhandled();
    const FVector2D Size = Geometry.GetLocalSize();
    const FHoleMapFrame& Frame = Range->GetHoleMapFrame();
    const FVector2D UV = FVector2D(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())) / Size;
    // A click on the ball's own pixel has no direction.
    Controller->AimToward(Frame.ToWorld(UV), Frame.Length / Size.Y);
    // Handled even when the ball cannot aim now, so a click on the map never places the ball in the world under it.
    return FReply::Handled();
}
