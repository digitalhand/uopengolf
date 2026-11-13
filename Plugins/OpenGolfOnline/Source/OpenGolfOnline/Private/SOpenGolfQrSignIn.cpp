// SPDX-License-Identifier: MIT
#include "SOpenGolfQrSignIn.h"
#include "OpenGolfOnlineSubsystem.h"
#include "ThirdParty/qrcodegen.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

/** Draws one address as a QR code that fills the widget: black modules on white, inside a thin quiet zone. */
class SOpenGolfQrCode : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SOpenGolfQrCode) {}
    SLATE_END_ARGS()
    void Construct(const FArguments&) {}
    /** Encodes Url; the widget draws nothing for an empty or unencodable one. */
    void SetUrl(const FString& InUrl)
    {
        if (Url == InUrl) return;
        Url = InUrl;
        uint8 Temp[qrcodegen_BUFFER_LEN_FOR_VERSION(MaxVersion)];
        Modules.SetNumZeroed(qrcodegen_BUFFER_LEN_FOR_VERSION(MaxVersion));
        const FTCHARToUTF8 Utf8(*Url);
        bValid = !Url.IsEmpty() && qrcodegen_encodeText(Utf8.Get(), Temp, Modules.GetData(), qrcodegen_Ecc_MEDIUM, 1, MaxVersion, qrcodegen_Mask_AUTO, true);
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    bool HasCode() const { return bValid; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements,
        int32 Layer, const FWidgetStyle&, bool) const override
    {
        if (!bValid) return Layer;
        const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
        FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(), Brush, ESlateDrawEffect::None, FLinearColor::White);
        const int32 Count = qrcodegen_getSize(Modules.GetData());
        const FVector2D Size = Geometry.GetLocalSize();
        // The code and its quiet zone fill the square exactly, whatever the code's module count. Module edges are
        // placed on whole screen pixels, so modules differ by at most a pixel and never leave a seam.
        const float Scale = FMath::Max(Geometry.Scale, UE_KINDA_SMALL_NUMBER);
        const float Pixels = FMath::Min(Size.X, Size.Y) * Scale;
        const int32 Span = Count + 2 * QuietZone;
        auto Edge = [Pixels, Span, Scale](int32 Module) { return FMath::RoundToFloat((Module + QuietZone) * Pixels / Span) / Scale; };
        const FVector2D Origin = (Size - FVector2D(Pixels / Scale)) * 0.5f;
        for (int32 Y = 0; Y < Count; ++Y)
            for (int32 X = 0; X < Count; ++X)
                if (qrcodegen_getModule(Modules.GetData(), X, Y))
                    FSlateDrawElement::MakeBox(Elements, Layer + 1,
                        Geometry.ToPaintGeometry(FVector2f(Edge(X + 1) - Edge(X), Edge(Y + 1) - Edge(Y)),
                            FSlateLayoutTransform(FVector2f(Origin + FVector2D(Edge(X), Edge(Y))))),
                        Brush, ESlateDrawEffect::None, FLinearColor::Black);
        return Layer + 1;
    }

private:
    /** QuietZone: white modules around the code. Two, not the standard's four, keeps the border thin; the square sits on a dark menu. */
    static constexpr int32 MaxVersion = 20, QuietZone = 2;
    FString Url;
    TArray<uint8> Modules;
    bool bValid = false;
};

void SOpenGolfQrSignIn::Construct(const FArguments& Args)
{
    Online = Args._Online; Style = Args._Style;
    const float Size = Args._Size;
    SetVisibility(TAttribute<EVisibility>::CreateLambda([this] { return IsOffered() ? EVisibility::Visible : EVisibility::Collapsed; }));
    ChildSlot
    [
        SNew(SBox).WidthOverride(Size)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SBox).HeightOverride(Size)
                [
                    SNew(SOverlay)
                    + SOverlay::Slot()
                    [
                        SNew(SBorder).BorderImage(&Style.Section).Padding(12).HAlign(HAlign_Center).VAlign(VAlign_Center)
                            .Visibility_Lambda([this] { return Code.IsValid() && Code->HasCode() ? EVisibility::Collapsed : EVisibility::Visible; })
                        [
                            SNew(STextBlock).Font(Style.GetFont(Style.SmallSize)).ColorAndOpacity(Style.LabelColor)
                                .AutoWrapText(true).Justification(ETextJustify::Center).Text_Lambda([this] { return Placeholder(); })
                        ]
                    ]
                    + SOverlay::Slot()[SAssignNew(Code, SOpenGolfQrCode)]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 8, 0, 0)
            [
                SNew(STextBlock).Font(Style.GetFont(Style.BodySize)).ColorAndOpacity(Style.TextColor).Text(FText::FromString(TEXT("Sign-In")))
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 2, 0, 0)
            [
                SNew(STextBlock).Font(Style.GetFont(Style.SmallSize)).ColorAndOpacity(Style.LabelColor)
                    .Text_Lambda([this] { return FText::FromString(Online.IsValid() ? TEXT("Code ") + Online->GetUserCode() : FString()); })
                    .Visibility_Lambda([this] { return Online.IsValid() && !Online->GetUserCode().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
            ]
        ]
    ];
}
bool SOpenGolfQrSignIn::IsOffered() const
{
    if (!Online.IsValid() || !Online->IsConfigured() || Online->HasSession()) return false;
    const EOpenGolfAccountState State = Online->GetAccountState();
    return State == EOpenGolfAccountState::SignedOut || State == EOpenGolfAccountState::SigningIn;
}
FText SOpenGolfQrSignIn::Placeholder() const
{
    if (!Online.IsValid()) return FText::GetEmpty();
    if (Online->IsSigningIn() && Online->GetQrUrl().IsEmpty()) return FText::FromString(TEXT("Signing in…"));
    return FText::FromString(Online->IsQrSignInFailing() ? TEXT("QR code unavailable. Retrying…") : TEXT("Getting QR code…"));
}
void SOpenGolfQrSignIn::Tick(const FGeometry& Geometry, double Time, float Delta)
{
    SCompoundWidget::Tick(Geometry, Time, Delta);
    // Tick runs only while the widget is shown and on screen, which is exactly when a code should be kept ready.
    if (!IsOffered()) return;
    Online->KeepQrSignInReady();
    Code->SetUrl(Online->GetQrUrl());
}
