// SPDX-License-Identifier: MIT
// Picking a club in the toolbar's drawer takes one click, as a player clicks: the press and the release frames apart.
// Rows low in the list used to scroll to the centre on the press, so the release missed them and nothing was picked.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "TestMaps.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/BundledCourse.h"
#include "Tests/RenderedPlay.h"
#include "UOpenGolfGameMode.h"

namespace ClubDrawerTestDetail
{
    /** The drawer rows the owner found needing two clicks, below the middle of a list opened on the driver. */
    struct FPick
    {
        const TCHAR* Label;
        ELaunchMonitorClub Club;
    };
    const FPick Picks[] = {{TEXT("7i"), ELaunchMonitorClub::Iron7}, {TEXT("6i"), ELaunchMonitorClub::Iron6}};

    /** The front-most visible window with a button reading Label (the open drawer), and that button. */
    bool FindRow(const FString& Label, TSharedPtr<SWindow>& OutWindow, TSharedPtr<SWidget>& OutRow)
    {
        TArray<TSharedRef<SWindow>> Windows;
        FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
        for (int32 Index = Windows.Num() - 1; Index >= 0; --Index)
        {
            if (const TSharedPtr<SWidget> Row = RenderedPlay::FindButton(Windows[Index], Label))
            {
                OutWindow = Windows[Index];
                OutRow = Row;
                return true;
            }
        }
        return false;
    }
} // namespace ClubDrawerTestDetail

class FPickClubsInDrawer : public IAutomationLatentCommand
{
public:
    explicit FPickClubsInDrawer(FAutomationTestBase& InTest) : Test(InTest) {}

    virtual bool Update() override
    {
        using namespace ClubDrawerTestDetail;
        if (GetCurrentRunTime() > 90.0)
        {
            Test.AddError(FString::Printf(TEXT("Timed out at stage %d."), Stage));
            return true;
        }
        if (GetCurrentRunTime() < NextAt)
            return false;
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        auto* Monitor = World ? World->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>() : nullptr;
        const TSharedPtr<SWindow> GameWindow = World && World->GetGameViewport() ? World->GetGameViewport()->GetWindow() : nullptr;
        if (!Mode || !Monitor || !GameWindow)
            return false;
        const TSharedPtr<SWidget> Picker = RenderedPlay::WithTag(GameWindow.ToSharedRef(), TEXT("ClubPicker"));
        if (!Picker || Picker->GetTickSpaceGeometry().GetLocalSize().X < 1)
            return false;
        FSlateApplication& Slate = FSlateApplication::Get();
        const FPick& Pick = Picks[PickIndex];

        switch (Stage)
        {
        case 0:
            if (PickIndex == 0)
            {
                StartClub = Monitor->GetClub();
                // As the owner found it: a course round, the drawer opened on the driver.
                if (const UGolfCourseDefinition* Course = BundledCourse::Load())
                    Mode->SetPlayMode(EOpenGolfPlayMode::Hole, Course);
            }
            Monitor->SetClub(ELaunchMonitorClub::Driver);
            if (!RenderedPlay::ClickTagged(Test, TEXT("ClubPicker")))
                return true;
            return Next(1, 0.5);
        case 1:
        {
            TSharedPtr<SWindow> Drawer;
            TSharedPtr<SWidget> Row;
            if (!Test.TestTrue(FString::Printf(TEXT("The drawer shows %s"), Pick.Label), Slate.AnyMenusVisible() && FindRow(Pick.Label, Drawer, Row)))
                return true;
            // Press only; the release comes frames later, as from a hand.
            PressAt = RenderedPlay::PointOn(*Row);
            Slate.RoutePointerMoveEvent(RenderedPlay::PathAt(Drawer.ToSharedRef(), PressAt), RenderedPlay::MouseAt(PressAt, PressAt, false, EKeys::Invalid), false);
            Slate.RoutePointerDownEvent(RenderedPlay::PathAt(Drawer.ToSharedRef(), PressAt), RenderedPlay::MouseAt(PressAt, PressAt, true, EKeys::LeftMouseButton));
            PressWindow = Drawer;
            return Next(2, 0.25);
        }
        case 2:
        {
            if (PressWindow.IsValid())
            {
                // The pointer has not moved; Slate re-reads what is under it before the release.
                const TSharedRef<SWindow> Window = PressWindow.Pin().ToSharedRef();
                Slate.RoutePointerMoveEvent(RenderedPlay::PathAt(Window, PressAt), RenderedPlay::MouseAt(PressAt, PressAt, true, EKeys::Invalid), false);
                Slate.RoutePointerUpEvent(RenderedPlay::PathAt(Window, PressAt), RenderedPlay::MouseAt(PressAt, PressAt, false, EKeys::LeftMouseButton));
            }
            return Next(3, 0.3);
        }
        case 3:
            Test.TestEqual(FString::Printf(TEXT("One click on %s picks it"), Pick.Label), Monitor->GetClub(), Pick.Club);
            Test.TestFalse(FString::Printf(TEXT("Picking %s closes the drawer"), Pick.Label), Slate.AnyMenusVisible());
            Slate.DismissAllMenus();
            if (++PickIndex < static_cast<int32>(UE_ARRAY_COUNT(Picks)))
                return Next(0, 0.3);
            Mode->SetPlayMode(EOpenGolfPlayMode::Range);
            Monitor->SetClub(StartClub);
            return true;
        default:
            return true;
        }
    }

private:
    bool Next(int32 InStage, double Seconds)
    {
        Stage = InStage;
        NextAt = GetCurrentRunTime() + Seconds;
        return false;
    }

    FAutomationTestBase& Test;
    int32 Stage = 0;
    int32 PickIndex = 0;
    double NextAt = 0.0;
    FVector2D PressAt = FVector2D::ZeroVector;
    TWeakPtr<SWindow> PressWindow;
    ELaunchMonitorClub StartClub = ELaunchMonitorClub::Driver;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClubDrawerClickTest, "UOpenGolf.Visual.ClubDrawerClick",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FClubDrawerClickTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(TestMaps::Range, false, true))
        return false;
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<RenderedPlay::FStartInWindow>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FPickClubsInDrawer>(*this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
