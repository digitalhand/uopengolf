// SPDX-License-Identifier: MIT
#include "UI/Hud/HoleMinimap.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HoleMap/HoleMapLabels.h"
#include "OpenFairwayBall.h"
#include "OpenGolfSettings.h"
#include "UI/Common/HudPen.h"
#include "UI/Common/HudToolTip.h"
#include "UI/Hud/HoleMapOverlay.h"
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

void SHoleMinimap::Tick(const FGeometry&, const double, const float DeltaTime)
{
    // The range captures once per Play (and on grass and daylight changes), so this is a pointer compare almost every frame.
    const AUOpenGolfGameMode* Mode = GetMode();
    const AUOpenGolfRange* Range = Mode ? Mode->GetRange() : nullptr;
    UObject* Picture = Range ? Range->GetHoleMapTexture() : nullptr;
    if (Aerial.GetResourceObject() != Picture)
        Aerial.SetResourceObject(Picture);
    if (Range && Mode->GetBall())
        UpdateView(*Range, *Mode->GetBall(), Mode->GetCupLocation(), DeltaTime);
}

void SHoleMinimap::UpdateView(const AUOpenGolfRange& Range, const AOpenFairwayBall& Ball, const FVector& Cup, float DeltaTime)
{
    // A new capture (the first, or the hole moved): start at its framing rather than easing in from a stale one.
    const bool bNewCapture = !bHasView || !Range.GetHoleMapFrame().Equals(Capture);
    Capture = Range.GetHoleMapFrame();
    // In flight the view holds still, so the shot is seen crossing the ground it was aimed over.
    if (bNewCapture || !Ball.IsMoving())
    {
        const FVector2D Points[] = {FVector2D(Ball.GetActorLocation()), FVector2D(Cup)};
        Goal = FHoleMapFrame::Fit(Capture, Points, ViewMarginCm, Range.MapMinimumViewLength);
    }
    const bool bSnap = bNewCapture || GolfSettings::Get().ReducedMotion != 0;
    View = bSnap ? Goal : FHoleMapFrame::Lerp(View, Goal, 1.0 - FMath::Exp(-ViewEaseRate * DeltaTime));
    bHasView = true;
    Aerial.SetUVRegion(View.UVRegionWithin(Capture));
}

int32 SHoleMinimap::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out,
    int32 Layer, const FWidgetStyle&, bool) const
{
    const AUOpenGolfGameMode* Mode = GetMode();
    const AUOpenGolfRange* Range = Mode ? Mode->GetRange() : nullptr;
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    if (!Range || !Ball || !Aerial.GetResourceObject() || !bHasView)
        return Layer;

    // The finished picture is opaque.
    FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(), &Aerial, ESlateDrawEffect::IgnoreTextureAlpha);

    const FVector2f Size = Geometry.GetLocalSize();
    const FLinearColor Ink = HudStyle::MapInkColor();
    const FLinearColor White = FLinearColor::White;
    // Lines and their ink outlines on the two layers above the picture, chips above them, the ball dot on top.
    const FHudPen Pen{Geometry, Out, Layer + 1};
    const int32 ChipLayer = Layer + 3;
    const int32 TopLayer = Layer + 5;

    const FVector2f Tee = ToMap(FVector2D(Range->GetTeeLocation()), Size);
    Pen.OutlinedLine({Tee - FVector2f(6, 0), Tee + FVector2f(6, 0)}, Ink, 5, White, 2);
    const FVector2f Pin = ToMap(FVector2D(Mode->GetCupLocation()), Size);
    Pen.OutlinedLine({Pin, Pin + FVector2f(0, -13), Pin + FVector2f(8, -10), Pin + FVector2f(0, -7)}, Ink, 4, HudStyle::MapFlagColor(), 2);

    // The pin's chip shows its distance; its elevation is in the panel's footer.
    const HoleMapOverlay::FChip PinChip{GolfSettings::RoundDistance(Mode->GetDistanceToCupM()), FString()};
    const HoleMapOverlay::FChipLayout PinLayout = HoleMapOverlay::LayOutChip(PinChip);
    const FVector2f PinChipAt = HoleMapOverlay::PlaceChip(Pin + FVector2f(0, -10), 12, PinLayout.Size, Size);
    HoleMapOverlay::DrawChip(Out, Geometry, ChipLayer, ChipBrush, PinChipAt, PinChip, PinLayout);

    const FVector2f Position = ToMap(FVector2D(Ball->GetActorLocation()), Size);
    const FVector2f Marker(FMath::Clamp(Position.X, EdgeInset, Size.X - EdgeInset), FMath::Clamp(Position.Y, EdgeInset, Size.Y - EdgeInset));
    if (!Position.Equals(Marker))
    {
        // Off the view: an arrow on the edge, pointing at the ball.
        const FVector2f Direction = (Position - Marker).GetSafeNormal();
        const FVector2f Side(-Direction.Y, Direction.X);
        Pen.OutlinedLine({Marker - Direction * 5 + Side * 4, Marker + Direction * 4, Marker - Direction * 5 - Side * 4}, Ink, 5, White, 2);
        return TopLayer;
    }
    if (Mode->IsBallAtAddress() && Controller.IsValid())
        PaintAim(Pen, ChipLayer, *Range, *Ball, Marker, Pin, FBox2f(PinChipAt, PinChipAt + PinLayout.Size));
    FSlateDrawElement::MakeBox(Out, TopLayer,
        Geometry.ToPaintGeometry(FVector2f(BallDotSize), FSlateLayoutTransform(Marker - FVector2f(BallDotSize * 0.5f))), &BallDot);
    return TopLayer;
}

void SHoleMinimap::PaintAim(const FHudPen& Pen, int32 ChipLayer, const AUOpenGolfRange& Range, const AOpenFairwayBall& Ball, FVector2f Marker,
    FVector2f Pin, const FBox2f& PinChipBox) const
{
    const FLinearColor Ink = HudStyle::MapInkColor();
    const FLinearColor White = FLinearColor::White;
    const FVector2f Size = Pen.Geometry.GetLocalSize();
    const FVector2D BallXY(Ball.GetActorLocation());
    const double TargetCm = Controller->GetAimTargetDistanceCm();
    const FVector2D TargetXY = BallXY + FVector2D(FRotator(0, Ball.AimYawDeg, 0).Vector()) * TargetCm;
    const FVector2f Target = ToMap(TargetXY, Size);
    if (FVector2f::Distance(Target, Pin) < MergeDistance)
    {
        Pen.OutlinedLine({Marker, Pin}, Ink, 3, White, 1.5f);
        return;
    }
    // On from the target to the pin, thinner: what the next shot leaves.
    Pen.Line({Target, Pin}, FLinearColor(1, 1, 1, 0.55f), 1.0f);
    Pen.OutlinedLine({Marker, Target}, Ink, 3, White, 1.5f);
    Pen.OutlinedLine(HoleMapOverlay::Circle(Target, TargetRadius), Ink, 3.5f, White, 1.5f);

    // The height change is measured on the sampled ground at both ends, so a level target reads level.
    const FHoleMapField& Heights = Range.GetHoleMapHeights();
    const FString Rise = Heights.IsValid() ? HoleMapLabels::HeightChange((Heights.At(TargetXY) - Heights.At(BallXY)) / 100.0, HoleMapLabels::MetricHeights())
        : FString();
    const HoleMapOverlay::FChip Chip{GolfSettings::RoundDistance(float(TargetCm / 100.0)), Rise};
    const HoleMapOverlay::FChipLayout Layout = HoleMapOverlay::LayOutChip(Chip);
    FVector2f ChipAt = HoleMapOverlay::PlaceChip(Target, TargetRadius + 4, Layout.Size, Size);
    // Never over the pin's chip: below the ring instead.
    if (FBox2f(ChipAt, ChipAt + Layout.Size).Intersect(PinChipBox))
        ChipAt = HoleMapOverlay::PlaceChip(Target + FVector2f(0, TargetRadius + 4 + Layout.Size.Y * 0.5f), 0, Layout.Size, Size);
    HoleMapOverlay::DrawChip(Pen.Elements, Pen.Geometry, ChipLayer, ChipBrush, ChipAt, Chip, Layout);
}

FReply SHoleMinimap::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    // Only the left button aims; the others reach the game, where a right click cancels ball placement.
    if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !bHasView || !Controller.IsValid())
        return FReply::Unhandled();
    const FVector2D Size = Geometry.GetLocalSize();
    const FVector2D UV = FVector2D(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())) / Size;
    // A click on the ball's own pixel has no direction.
    Controller->AimToward(View.ToWorld(UV), View.Length / Size.Y);
    // Handled even when the ball cannot aim now, so a click on the map never places the ball in the world under it.
    return FReply::Handled();
}
