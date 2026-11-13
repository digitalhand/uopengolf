// SPDX-License-Identifier: MIT
#pragma once
#include "Framework/Application/IInputProcessor.h"
#include "UOpenGolfPlayerController.h"

/**
 * Keyboard and gamepad control of the Training HUD, ahead of Slate's widget routing. F6 or the gamepad View button
 * moves focus onto the HUD controls (tags TrainingHud, PlayControls, WindControls, TestShotControls); arrows, Tab and
 * the D-pad move between them; Escape or B leaves. The gamepad Menu button opens the menu, or goes back in it. While a
 * ball is being placed, the left stick moves the cursor and A places the ball. Active only while the game viewport's
 * window is the active window; while the menu is open, keys go to the menu.
 */
class FHudNavigation : public IInputProcessor
{
public:
    explicit FHudNavigation(AUOpenGolfPlayerController* InPlayer) : Player(InPlayer) {}
    virtual void Tick(float DeltaTime,FSlateApplication& App,TSharedRef<ICursor>) override;
    virtual bool HandleKeyDownEvent(FSlateApplication& App,const FKeyEvent& Event) override;
    virtual bool HandleKeyUpEvent(FSlateApplication& App,const FKeyEvent& Event) override;
    virtual bool HandleAnalogInputEvent(FSlateApplication& App,const FAnalogInputEvent& Event) override;
private:
    bool Active(FSlateApplication& App) const;
    void FocusHud(FSlateApplication& App);
    void LeaveHud(FSlateApplication& App);
    TWeakObjectPtr<AUOpenGolfPlayerController> Player;
    bool bFocused=false;
    FVector2D Stick=FVector2D::ZeroVector;
};
