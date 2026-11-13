// SPDX-License-Identifier: MIT
#include "UI/Hud/HudNavigation.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerInput.h"
#include "Widgets/SWindow.h"

namespace HudNavigationDetail
{
    void Controls(TSharedRef<SWidget> Widget,TArray<TSharedRef<SWidget>>& Out,bool InHud=false)
    {
        if(!Widget->GetVisibility().IsVisible()||!Widget->IsEnabled())return;
        const FName Tag=Widget->GetTag();
        InHud|=Tag==TEXT("TrainingHud")||Tag==TEXT("PlayControls")||Tag==TEXT("WindControls")||Tag==TEXT("TestShotControls");
        if(InHud && Widget->SupportsKeyboardFocus())Out.Add(Widget);
        for(int32 I=0;I<Widget->GetChildren()->Num();++I)Controls(Widget->GetChildren()->GetChildAt(I),Out,InHud);
    }
    bool NavigationKey(FKey Key)
    {
        return Key==EKeys::Tab||Key==EKeys::Enter||Key==EKeys::SpaceBar||Key==EKeys::Left||Key==EKeys::Right||Key==EKeys::Up||Key==EKeys::Down
            ||Key==EKeys::Gamepad_FaceButton_Bottom||Key==EKeys::Gamepad_DPad_Left||Key==EKeys::Gamepad_DPad_Right
            ||Key==EKeys::Gamepad_DPad_Up||Key==EKeys::Gamepad_DPad_Down||Key==EKeys::Gamepad_LeftStick_Left
            ||Key==EKeys::Gamepad_LeftStick_Right||Key==EKeys::Gamepad_LeftStick_Up||Key==EKeys::Gamepad_LeftStick_Down;
    }
} // namespace HudNavigationDetail
bool FHudNavigation::Active(FSlateApplication& App) const
{
    const auto* Viewport=Player.IsValid()&&Player->GetWorld()?Player->GetWorld()->GetGameViewport():nullptr;
    return Viewport && Viewport->GetWindow().IsValid() && App.GetActiveTopLevelWindow()==Viewport->GetWindow();
}
void FHudNavigation::FocusHud(FSlateApplication& App)
{
    TArray<TSharedRef<SWidget>> Items;
    HudNavigationDetail::Controls(Player->GetWorld()->GetGameViewport()->GetWindow().ToSharedRef(),Items);
    if(!Items.IsEmpty())
    {
        bFocused=true;
        App.SetKeyboardFocus(Items[0],EFocusCause::Navigation);
        if(Player->PlayerInput)Player->PlayerInput->FlushPressedKeys();
    }
}
void FHudNavigation::LeaveHud(FSlateApplication& App)
{
    bFocused=false;Stick=FVector2D::ZeroVector;App.DismissAllMenus();App.SetAllUserFocusToGameViewport();
}
bool FHudNavigation::HandleKeyDownEvent(FSlateApplication& App,const FKeyEvent& Event)
{
    if(!Active(App))return false;
    const FKey Key=Event.GetKey();
    if(Key==EKeys::Gamepad_Special_Right)
    {
        if(!Event.IsRepeat()){if(Player->IsMainMenuOpen())Player->HandleMenuBack();else Player->OpenMainMenu();}
        bFocused=false;return true;
    }
    if(Player->IsMainMenuOpen()){bFocused=false;return false;}
    if(Key==EKeys::F6||Key==EKeys::Gamepad_Special_Left)
    {
        if(!Event.IsRepeat()){if(bFocused)LeaveHud(App);else FocusHud(App);}return true;
    }
    if(Key==EKeys::Escape||Key==EKeys::Gamepad_FaceButton_Right)
    {
        if(Event.IsRepeat())return true;
        if(App.AnyMenusVisible()){App.DismissAllMenus();return true;}
        if(Player->IsPlacingBall()||Player->IsWindPanelOpen()||Player->IsReplayPanelOpen())
        {Player->HandleEscape();if(bFocused)FocusHud(App);return true;}
        if(bFocused){LeaveHud(App);return true;}
        Player->HandleEscape();return true;
    }
    if(Player->IsPlacingBall() && Key==EKeys::Gamepad_FaceButton_Bottom)
    {
        float X,Y;if(Player->GetMousePosition(X,Y)&&Player->PlaceBallAtScreen(FVector2D(X,Y)))Player->SetPlacingBall(false);
        LeaveHud(App);return true;
    }
    if(!bFocused)return false;
    if(App.AnyMenusVisible())return !HudNavigationDetail::NavigationKey(Key);
    TArray<TSharedRef<SWidget>> Items;HudNavigationDetail::Controls(Player->GetWorld()->GetGameViewport()->GetWindow().ToSharedRef(),Items);
    const auto Focus=App.GetKeyboardFocusedWidget();
    const bool Previous=Key==EKeys::Up||Key==EKeys::Gamepad_DPad_Up||Key==EKeys::Gamepad_LeftStick_Up||(Key==EKeys::Tab&&Event.IsShiftDown());
    const bool Next=Key==EKeys::Down||Key==EKeys::Gamepad_DPad_Down||Key==EKeys::Gamepad_LeftStick_Down||(Key==EKeys::Tab&&!Event.IsShiftDown());
    if((Previous||Next)&&!Items.IsEmpty())
    {
        int32 Index=Items.IndexOfByPredicate([&](const auto& W){return W==Focus;});
        Index=(Index+(Previous?-1:1)+Items.Num())%Items.Num();
        App.SetKeyboardFocus(Items[Index],EFocusCause::Navigation);return true;
    }
    return !HudNavigationDetail::NavigationKey(Key);
}
bool FHudNavigation::HandleKeyUpEvent(FSlateApplication& App,const FKeyEvent& Event)
{
    return Active(App)&&bFocused&&!HudNavigationDetail::NavigationKey(Event.GetKey());
}
bool FHudNavigation::HandleAnalogInputEvent(FSlateApplication& App,const FAnalogInputEvent& Event)
{
    if(!Active(App)||!Player->IsPlacingBall()){Stick=FVector2D::ZeroVector;return false;}
    const FKey Key=Event.GetKey();const float V=FMath::Abs(Event.GetAnalogValue())>.18f?Event.GetAnalogValue():0;
    if(Key==EKeys::Gamepad_LeftX)Stick.X=V;
    else if(Key==EKeys::Gamepad_LeftY)Stick.Y=-V;else return false;
    return true;
}
void FHudNavigation::Tick(float DeltaTime,FSlateApplication& App,TSharedRef<ICursor>)
{
    if(!Active(App)||!Player->IsPlacingBall()){Stick=FVector2D::ZeroVector;return;}
    if(Stick.IsNearlyZero())return;
    int32 W,H;Player->GetViewportSize(W,H);
    float X=W*.5f,Y=H*.5f;Player->GetMousePosition(X,Y);
    Player->SetMouseLocation(FMath::Clamp(int32(X+Stick.X*700*DeltaTime),0,W-1),FMath::Clamp(int32(Y+Stick.Y*700*DeltaTime),0,H-1));
}
