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

    /** Presses the left button at From, moves to To and releases there, through Slate's routing as a real mouse would. */
    inline void Drag(const TSharedRef<SWindow>& Window, const FVector2D& From, const FVector2D& To)
    {
        FSlateApplication& Slate = FSlateApplication::Get();
        Slate.RoutePointerDownEvent(PathAt(Window, From), MouseAt(From, From, true, EKeys::LeftMouseButton));
        if (!To.Equals(From))
            Slate.RoutePointerMoveEvent(PathAt(Window, To), MouseAt(To, From, true, EKeys::Invalid), false);
        Slate.RoutePointerUpEvent(PathAt(Window, To), MouseAt(To, To, false, EKeys::LeftMouseButton));
    }

    inline void Click(const TSharedRef<SWindow>& Window, const FVector2D& At)
    {
        Drag(Window, At, At);
    }
} // namespace RenderedPlay

#endif
