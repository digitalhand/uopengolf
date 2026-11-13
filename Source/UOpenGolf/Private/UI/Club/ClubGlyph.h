// SPDX-License-Identifier: MIT
#pragma once
#include "Widgets/SLeafWidget.h"
#include "LaunchMonitorTypes.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "UI/Common/HudStyle.h"

// Original club illustrations drawn at the display resolution.
class SClubGlyph : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SClubGlyph) : _Club(ELaunchMonitorClub::Driver) {}
    SLATE_ATTRIBUTE(ELaunchMonitorClub, Club)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Club=Args._Club; SetVisibility(EVisibility::HitTestInvisible); }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(56,40); }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,
        int32 Layer,const FWidgetStyle& Style,bool Enabled) const override
    {
        const auto C=Club.Get();
        const bool Wood=C==ELaunchMonitorClub::Driver||C==ELaunchMonitorClub::Wood3||C==ELaunchMonitorClub::Wood5||C==ELaunchMonitorClub::Wood7;
        const bool Putter=C==ELaunchMonitorClub::Putter;
        const FVector2f Scale=FVector2f(G.GetLocalSize())/FVector2f(56,40);
        const FLinearColor Tint=Style.GetColorAndOpacityTint()*(Enabled?1.f:0.45f);
        const FLinearColor Metal=FLinearColor(.82f,.88f,.86f)*Tint;
        auto Line=[&](TArray<FVector2f> Points,FLinearColor Color,float Width)
        {
            for(auto& P:Points)P*=Scale;
            FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Width*Scale.Y);
        };
        auto Box=[&](float X,float Y,float W,float H,FLinearColor Color)
        {
            FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2f(W,H)*Scale,FSlateLayoutTransform(FVector2f(X,Y)*Scale)),&Head,ESlateDrawEffect::None,Color*Tint);
        };
        if(Putter)
        {
            Box(7,23,39,10,FLinearColor(.2f,.27f,.25f));
            Line({{9,24},{44,24},{44,32},{9,32},{9,24}},Metal,2);
            Line({{28,24},{33,19},{39,5}},Metal,3);
            Line({{25,25},{25,31}},HudStyle::AccentColor()*Tint,2);
        }
        else if(Wood)
        {
            const bool Driver=C==ELaunchMonitorClub::Driver;
            Box(6,13,Driver?33:28,22,FLinearColor(.15f,.23f,.2f));
            Line({{39,5},{34,18},{29,14},{19,12},{11,15},{6,22},{8,29},{16,34},{27,33},{33,28},{34,18}},Metal,2.2f);
            Line({{11,19},{15,16},{23,16}},HudStyle::AccentColor()*Tint,2);
            Line({{27,20},{24,30}},Metal*.65f,1.5f);
        }
        else
        {
            Box(7,19,28,15,FLinearColor(.24f,.31f,.28f));
            Line({{43,5},{35,25},{30,31},{12,34},{7,30},{8,24},{13,18},{21,17},{31,23},{35,25}},Metal,2.2f);
            Line({{12,25},{27,27}},Metal*.65f,1.4f);
            Line({{11,29},{25,30}},Metal*.65f,1.4f);
            Line({{14,20},{21,20}},HudStyle::AccentColor()*Tint,2);
        }
        return Layer+3;
    }
private:
    TAttribute<ELaunchMonitorClub> Club;
    FSlateRoundedBoxBrush Head{FLinearColor::White,7.f};
};
