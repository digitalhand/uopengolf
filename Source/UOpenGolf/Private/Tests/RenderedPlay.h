// SPDX-License-Identifier: MIT
// Shared by the rendered (UOpenGolf.Visual) tests: Play in a fixed-size window, finding Slate widgets, and real clicks.
#pragma once

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/HittestGrid.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PlayInEditorDataTypes.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"

namespace RenderedPlay
{
    /** The rendered size: -ResX and -ResY on the command line, otherwise 1280×720. */
    inline FIntPoint CaptureSize()
    {
        FIntPoint Size(1280, 720);
        FParse::Value(FCommandLine::Get(), TEXT("ResX="), Size.X);
        FParse::Value(FCommandLine::Get(), TEXT("ResY="), Size.Y);
        return Size;
    }

    /** Starts Play in its own window of CaptureSize, so every machine renders and hit-tests the same viewport. */
    class FStartInWindow : public IAutomationLatentCommand
    {
    public:
        bool Update() override
        {
            const TSharedRef<SWindow> Window = SNew(SWindow)
                                                   .ClientSize(FVector2D(CaptureSize()))
                                                   .CreateTitleBar(false)
                                                   .UseOSWindowBorder(false)
                                                   .AdjustInitialSizeAndPositionForDPIScale(false)
                                                   .SaneWindowPlacement(false)
                                                   .SizingRule(ESizingRule::FixedSize);
            FSlateApplication::Get().AddWindow(Window);
            FRequestPlaySessionParams Params;
            Params.CustomPIEWindow = Window;
            GEditor->RequestPlaySession(Params);
            return true;
        }
    };

    /** The first widget under Root, depth first, whose type name contains Type. */
    inline TSharedPtr<SWidget> FindWidget(const TSharedRef<SWidget>& Root, const FString& Type)
    {
        if (Root->GetTypeAsString().Contains(Type))
            return Root;
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            if (TSharedPtr<SWidget> Found = FindWidget(Children->GetChildAt(Index), Type))
                return Found;
        }
        return nullptr;
    }

    /** The first widget under Root, depth first, with this Slate tag. */
    inline TSharedPtr<SWidget> WithTag(const TSharedRef<SWidget>& Root, FName Tag)
    {
        if (Root->GetTag() == Tag)
            return Root;
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            if (TSharedPtr<SWidget> Found = WithTag(Children->GetChildAt(Index), Tag))
                return Found;
        }
        return nullptr;
    }

    /**
     * Every shown widget under Root whose tooltip would appear on hover, as "Type#Tag: text"; the game shows none
     * (captions instead). Slate shows only non-empty tooltips (SlateUser.cpp), and engine widgets carry empty ones:
     * SComboButton binds its button's tooltip to its own, unset text, so those are not counted. Collapsed and hidden
     * subtrees cannot be hovered and are skipped.
     */
    inline void WidgetsWithToolTips(const TSharedRef<SWidget>& Root, TArray<FString>& Out)
    {
        if (!Root->GetVisibility().IsVisible())
            return;
        const TSharedPtr<IToolTip> ToolTip = Root->GetToolTip();
        if (ToolTip.IsValid() && !ToolTip->IsEmpty())
        {
            const TSharedRef<SWidget> Content = ToolTip->AsWidget();
            const FString Text = Content->GetTypeAsString() == TEXT("SToolTip") ? StaticCastSharedRef<SToolTip>(Content)->GetTextTooltip().ToString() : FString();
            Out.Add(Root->GetTypeAsString() + TEXT("#") + Root->GetTag().ToString() + TEXT(": ") + Text);
        }
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
            WidgetsWithToolTips(Children->GetChildAt(Index), Out);
    }

    /**
     * Fails Test for every tooltip in the game window and its child windows (drop-down menus), naming the widgets and
     * the screen (Where). The editor's own windows are not the game's and are skipped.
     */
    inline void ExpectNoToolTips(FAutomationTestBase& Test, const TSharedRef<SWindow>& GameWindow, const FString& Where)
    {
        TArray<FString> Found;
        TArray<TSharedRef<SWindow>> Pending = {GameWindow};
        while (!Pending.IsEmpty())
        {
            const TSharedRef<SWindow> Window = Pending.Pop();
            WidgetsWithToolTips(Window, Found);
            Pending.Append(Window->GetChildWindows());
        }
        Test.TestTrue(FString::Printf(TEXT("No tooltips on %s: %s"), *Where, *FString::Join(Found, TEXT(", "))), Found.IsEmpty());
    }

    /** A desktop point on a drawn widget, at Alpha across its last painted size (0.5, 0.5 is its centre). */
    inline FVector2D PointOn(const SWidget& Widget, const FVector2D& Alpha = FVector2D(0.5, 0.5))
    {
        const FGeometry& Geometry = Widget.GetTickSpaceGeometry();
        return Geometry.LocalToAbsolute(FVector2D(Geometry.GetLocalSize()) * Alpha);
    }

    /** The widgets under a desktop point, as Slate's hit test routes a click there: the front-most hit-testable one last. */
    inline FWidgetPath PathAt(const TSharedRef<SWindow>& Window, const FVector2D& At)
    {
        TArray<FWidgetAndPointer> Hits = Window->GetHittestGrid().GetBubblePath(At, 0.0f, false, 0);
        return FWidgetPath(Hits);
    }

    inline FPointerEvent MouseAt(const FVector2D& At, const FVector2D& From, bool bDown, const FKey& Button)
    {
        return FPointerEvent(0, At, From, bDown ? TSet<FKey>{EKeys::LeftMouseButton} : TSet<FKey>(), Button, 0.0f, FModifierKeysState());
    }

    /** Hovers From, presses the left button there, moves to To and releases there, through Slate's routing as a real mouse would. */
    inline void Drag(const TSharedRef<SWindow>& Window, const FVector2D& From, const FVector2D& To)
    {
        FSlateApplication& Slate = FSlateApplication::Get();
        // SButton only clicks when hovered, and hover comes from a move.
        Slate.RoutePointerMoveEvent(PathAt(Window, From), MouseAt(From, From, false, EKeys::Invalid), false);
        Slate.RoutePointerDownEvent(PathAt(Window, From), MouseAt(From, From, true, EKeys::LeftMouseButton));
        if (!To.Equals(From))
            Slate.RoutePointerMoveEvent(PathAt(Window, To), MouseAt(To, From, true, EKeys::Invalid), false);
        Slate.RoutePointerUpEvent(PathAt(Window, To), MouseAt(To, To, false, EKeys::LeftMouseButton));
    }

    inline void Click(const TSharedRef<SWindow>& Window, const FVector2D& At)
    {
        Drag(Window, At, At);
    }

    /** Whether a shown text block under Root reads Label, or with bContains includes it. Collapsed subtrees are skipped. */
    inline bool HasText(const TSharedRef<SWidget>& Root, const FString& Label, bool bContains = false)
    {
        if (!Root->GetVisibility().IsVisible())
            return false;
        if (Root->GetTypeAsString() == TEXT("STextBlock"))
        {
            const FString Text = StaticCastSharedRef<STextBlock>(Root)->GetText().ToString();
            if (bContains ? Text.Contains(Label) : Text == Label)
                return true;
        }
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            if (HasText(Children->GetChildAt(Index), Label, bContains))
                return true;
        }
        return false;
    }

    /** The first shown button under Root, depth first, whose content reads Label. */
    inline TSharedPtr<SWidget> FindButton(const TSharedRef<SWidget>& Root, const FString& Label)
    {
        if (!Root->GetVisibility().IsVisible())
            return nullptr;
        if (Root->GetTypeAsString() == TEXT("SButton") && HasText(Root, Label))
            return Root;
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            if (TSharedPtr<SWidget> Found = FindButton(Children->GetChildAt(Index), Label))
                return Found;
        }
        return nullptr;
    }

    /** Clicks the centre of Widget in Window, failing Test and returning false when Slate's hit test would not reach it. */
    inline bool ClickOn(FAutomationTestBase& Test, const TSharedRef<SWindow>& Window, const TSharedPtr<SWidget>& Widget, const FString& Name)
    {
        if (!Widget.IsValid())
        {
            Test.AddError(TEXT("Missing control: ") + Name);
            return false;
        }
        const FVector2D At = PointOn(*Widget);
        const FWidgetPath Path = PathAt(Window, At);
        if (!Path.IsValid() || !Path.ContainsWidget(Widget.Get()))
        {
            Test.AddError(FString::Printf(TEXT("A click on %s at %s does not reach it."), *Name, *At.ToString()));
            return false;
        }
        Click(Window, At);
        return true;
    }

    /** Clicks the button reading Label in the front-most visible window that has one, such as an open drop-down. */
    inline bool ClickLabelled(FAutomationTestBase& Test, const FString& Label)
    {
        TArray<TSharedRef<SWindow>> Windows;
        FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
        for (int32 Index = Windows.Num() - 1; Index >= 0; --Index)
        {
            if (const TSharedPtr<SWidget> Button = FindButton(Windows[Index], Label))
                return ClickOn(Test, Windows[Index], Button, Label);
        }
        Test.AddError(TEXT("Missing button: ") + Label);
        return false;
    }
} // namespace RenderedPlay

#endif
