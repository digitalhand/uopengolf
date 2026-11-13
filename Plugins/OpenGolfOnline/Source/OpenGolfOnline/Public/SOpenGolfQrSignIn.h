// SPDX-License-Identifier: MIT
#pragma once
#include "Widgets/SCompoundWidget.h"
#include "OpenGolfAccountStyle.h"
class UOpenGolfOnlineSubsystem;
class SOpenGolfQrCode;

/**
 * A standing QR sign-in: the code as a square, "Sign-In" under it, then the short code the login page asks the player
 * to compare. A host places it anywhere; it needs no other wiring.
 *
 * It shows only while signed out and collapses itself otherwise. While shown it keeps the service's QR code ready
 * (UOpenGolfOnlineSubsystem::KeepQrSignInReady) from its Tick, so the code is requested, polled and renewed only while
 * the player can see it.
 */
class OPENGOLFONLINE_API SOpenGolfQrSignIn : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SOpenGolfQrSignIn) : _Online(nullptr), _Size(200.0f) {}
    SLATE_ARGUMENT(UOpenGolfOnlineSubsystem*, Online)
    SLATE_ARGUMENT(FOpenGolfAccountStyle, Style)
    /** Side of the QR square, and the width of the text under it. */
    SLATE_ARGUMENT(float, Size)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual void Tick(const FGeometry&, double, float) override;

private:
    /** Signed out, or signing in with the browser, on a configured service. */
    bool IsOffered() const;
    /** What the square says while there is no code to draw. */
    FText Placeholder() const;
    TWeakObjectPtr<UOpenGolfOnlineSubsystem> Online;
    /** Fonts and brushes used by the child widgets point at this copy. */
    FOpenGolfAccountStyle Style;
    TSharedPtr<SOpenGolfQrCode> Code;
};
