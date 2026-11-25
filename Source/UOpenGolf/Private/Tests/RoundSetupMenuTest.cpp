// SPDX-License-Identifier: MIT
// The Round setup page, driven through its own buttons: the Players, Match and Course tabs, the course's information
// and conditions beside them, and the setup Start round hands over.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Course/RoundConditions.h"
#include "Engine/GameInstance.h"
#include "GolfCourseDefinition.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "OpenGolfOnlineSubsystem.h"
#include "OpenGolfSettings.h"
#include "Play/OpenGolfWindSetting.h"
#include "Tests/RenderedPlay.h"
#include "UI/Common/ChoiceChips.h"
#include "UI/Common/MenuStyle.h"
#include "UI/Common/PageHeader.h"
#include "UI/Menu/RoundSetup.h"
#include "UI/Menu/RoundSetupPanels.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Engine/Texture2D.h"
#include "Widgets/Input/SSlider.h"

namespace RoundSetupMenuTestDetail
{
    using namespace OpenGolfPlay;
    const TCHAR* const HunterPath = TEXT("/HunterRanch/Course/DA_HunterRanch.DA_HunterRanch");
    const TCHAR* const SavannaPath = TEXT("/Game/UOpenGolf/Courses/DA_SavannaRange.DA_SavannaRange");

    // The test drives an unmounted page, so Slate has no window tick to refresh its bound attributes.
    void Refresh(const TSharedRef<SWidget>& Widget)
    {
        Widget->UpdateAllAttributes();
        FChildren* Children = Widget->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
            Refresh(Children->GetChildAt(Index));
    }

    bool Click(FAutomationTestBase& Test, const TSharedRef<SWidget>& Root, FName Tag)
    {
        // The tagged control is a button, or (the page's Back icon) wraps one.
        const TSharedPtr<SWidget> Tagged = RenderedPlay::WithTag(Root, Tag);
        const TSharedPtr<SWidget> Found = Tagged.IsValid() ? RenderedPlay::FindWidget(Tagged.ToSharedRef(), TEXT("SButton")) : nullptr;
        if (!Test.TestTrue(FString::Printf(TEXT("%s is on the page"), *Tag.ToString()), Found.IsValid() && Found->GetTypeAsString() == TEXT("SButton")))
            return false;
        StaticCastSharedPtr<SButton>(Found)->SimulateClick();
        Refresh(Root);
        return true;
    }

    bool Tick(FAutomationTestBase& Test, const TSharedRef<SWidget>& Root, FName Tag)
    {
        const TSharedPtr<SWidget> Found = RenderedPlay::WithTag(Root, Tag);
        if (!Test.TestTrue(FString::Printf(TEXT("%s is on the page"), *Tag.ToString()), Found.IsValid() && Found->GetTypeAsString() == TEXT("SCheckBox")))
            return false;
        StaticCastSharedPtr<SCheckBox>(Found)->ToggleCheckedState();
        Refresh(Root);
        return true;
    }

    /** Whether Tag is under Root with every widget on the way visible: a tab's controls show only on that tab. */
    bool Shown(const TSharedRef<SWidget>& Root, FName Tag)
    {
        if (!Root->GetVisibility().IsVisible())
            return false;
        if (Root->GetTag() == Tag)
            return true;
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
            if (Shown(Children->GetChildAt(Index), Tag))
                return true;
        return false;
    }

    /** The text of the tagged text block, or empty. */
    FString TextOf(const TSharedRef<SWidget>& Root, FName Tag)
    {
        const TSharedPtr<SWidget> Found = RenderedPlay::WithTag(Root, Tag);
        return Found.IsValid() && Found->GetTypeAsString() == TEXT("STextBlock") ? StaticCastSharedPtr<STextBlock>(Found)->GetText().ToString() : FString();
    }

    void OpenTab(FAutomationTestBase& Test, const TSharedRef<SRoundSetup>& Page, SRoundSetup::ETab Tab)
    {
        Click(Test, Page, SChoiceChips::ChipName(SRoundSetup::TabTag, static_cast<int32>(Tab)));
        Test.TestEqual(TEXT("The tab opens"), Page->GetTab(), Tab);
    }
} // namespace RoundSetupMenuTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundSetupMenuTest, "UOpenGolf.Menu.RoundSetup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRoundSetupMenuTest::RunTest(const FString&)
{
    using namespace RoundSetupMenuTestDetail;
    using ETab = SRoundSetup::ETab;
    const auto* Hunter = LoadObject<UGolfCourseDefinition>(nullptr, HunterPath);
    const auto* Savanna = LoadObject<UGolfCourseDefinition>(nullptr, SavannaPath);
    if (!TestNotNull(TEXT("Hunter Ranch loads"), Hunter) || !TestNotNull(TEXT("Savanna Range loads"), Savanna))
        return false;
    TestEqual(TEXT("Hunter Ranch is named plainly"), Hunter->DisplayName.ToString(), FString(TEXT("Hunter Ranch")));
    TestTrue(TEXT("Hunter Ranch names its location, designer and elevation"),
        !Hunter->Location.IsEmpty() && !Hunter->Designer.IsEmpty() && Hunter->ElevationM > 0.0f);

    const FOpenGolfAccountStyle Style = MenuStyle::Make();
    GolfSettings::FPreferences Preferences;
    Preferences.Stimp = 11;
    Preferences.GimmieFeet = 3;
    FOpenGolfRoundSetup Defaults = RoundConditions::Defaults(*Hunter, Preferences);
    TestTrue(TEXT("A new round starts from Game Defaults"), Defaults.GreenStimpFeet == 11 && Defaults.bGimmie && Defaults.GimmieFeet == 3);
    TestEqual(TEXT("A new round plays every hole"), Defaults.Holes.Num(), 18);
    TestEqual(TEXT("The defaults pick no wind; the round rolls one"), Defaults.WindSpeedMph, 0);
    // The rolled wind the page shows: 9 mph from the right of hole 1's tee shot.
    const FName FirstTees = Hunter->Layout[0].Tees[0].SetName;
    Defaults.WindSpeedMph = 9;
    Defaults.WindFromDeg = static_cast<float>(FMath::RoundToInt(FRotator::ClampAxis(RoundConditions::TeeShotBearingDeg(*Hunter, 0, FirstTees) + 90.0f)) % 360);

    TOptional<FOpenGolfRoundSetup> Started;
    bool bBack = false;
    const TSharedRef<SRoundSetup> Page = SNew(SRoundSetup).Style(&Style).Course(Hunter).Setup(Defaults).Defaults(Defaults)
        .OnStart_Lambda([&Started](const FOpenGolfRoundSetup& Setup) { Started = Setup; })
        .OnBack_Lambda([&bBack] { bBack = true; });

    Refresh(Page);
    TestNotNull(TEXT("Hunter Ranch has a loadable course preview"), Hunter->Preview.LoadSynchronous());
    TestTrue(TEXT("Club House is the page title"), RenderedPlay::HasText(Page, TEXT("Club House")));
    TestTrue(TEXT("The page names the course"), RenderedPlay::HasText(Page, Hunter->DisplayName.ToString()));
    TArray<FString> ToolTips;
    RenderedPlay::WidgetsWithToolTips(Page, ToolTips);
    TestTrue(FString::Printf(TEXT("No tooltips on the page: %s"), *FString::Join(ToolTips, TEXT(", "))), ToolTips.IsEmpty());

    TestEqual(TEXT("The page opens on Players"), Page->GetTab(), ETab::Players);
    TestTrue(TEXT("The active card names the guest"), Shown(Page, SRoundSetup::PlayerTag) && RenderedPlay::HasText(Page, TEXT("Guest")));
    TestTrue(TEXT("One open seat is shown"), RenderedPlay::HasText(Page, TEXT("Open seat")));
    const auto TeePicker = StaticCastSharedPtr<SComboBox<FName>>(RenderedPlay::WithTag(Page, SRoundSetup::TeesTag));
    if (!TestTrue(TEXT("The active player picks tees"), TeePicker.IsValid()))
        return false;
    const FString FirstDistance = TextOf(Page, SCourseInformation::ValueTag(SCourseInformation::EField::Distance));
    TestEqual(TEXT("Total distance from the first tees"), FirstDistance, GolfSettings::RoundDistance(Hunter->TeeSetYards(FirstTees) * 0.9144f));

    // The sidebar shows on every tab: the course's information and the round's wind.
    TestTrue(TEXT("Course information"), RenderedPlay::HasText(Page, TEXT("Course information")));
    TestEqual(TEXT("The course's par"), TextOf(Page, SCourseInformation::ValueTag(SCourseInformation::EField::Par)), FString(TEXT("72")));
    TestTrue(TEXT("Course conditions"), RenderedPlay::HasText(Page, TEXT("Course conditions")));
    TestTrue(TEXT("The round's wind speed"), RenderedPlay::HasText(Page, TEXT("9 mph")));
    TestEqual(TEXT("The wind against the first hole's tee shot"), TextOf(Page, SCourseConditions::WindTag),
        WindDirectionLabel(90.0f) + TEXT(" · hole 1"));
    // Wind and time of day are not choices any more.
    for (const TCHAR* Gone : {TEXT("RoundWind.0"), TEXT("RoundWindSpeed"), TEXT("RoundWindFrom.0"), TEXT("RoundTime.0")})
        TestFalse(FString::Printf(TEXT("No %s control"), Gone), RenderedPlay::WithTag(Page, Gone).IsValid());

    // Match: the Gimmie and its radius.
    TestFalse(TEXT("The Gimmie is on the Match tab"), Shown(Page, SRoundSetup::GimmieSwitchTag));
    OpenTab(*this, Page, ETab::Match);
    TestTrue(TEXT("The Gimmie switch shows on Match"), Shown(Page, SRoundSetup::GimmieSwitchTag));
    Click(*this, Page, SChoiceChips::ChipName(SRoundSetup::GimmieTag, 1));
    TestEqual(TEXT("6 ft"), Page->GetSetup().GimmieFeet, LongGimmieFeet);
    const TSharedPtr<SWidget> Switch = RenderedPlay::WithTag(Page, SRoundSetup::GimmieSwitchTag);
    const FKeyEvent Space(EKeys::SpaceBar, FModifierKeysState(), 0, false, 0, 0);
    TestTrue(TEXT("The switch turns the Gimmie off"), Switch.IsValid() && Switch->OnKeyDown(FGeometry(), Space).IsEventHandled() && !Page->GetSetup().bGimmie);
    TestTrue(TEXT("And back on"), Switch.IsValid() && Switch->OnKeyDown(FGeometry(), Space).IsEventHandled() && Page->GetSetup().bGimmie);

    OpenTab(*this, Page, ETab::Players);
    TeePicker->SetSelectedItem(Hunter->Layout[0].Tees[1].SetName);
    Refresh(Page);
    TestEqual(TEXT("The player picks the second tee set"), Page->GetSetup().TeeIndex, 1);
    TestNotEqual(TEXT("The total distance follows the tees"), TextOf(Page, SCourseInformation::ValueTag(SCourseInformation::EField::Distance)), FirstDistance);

    OpenTab(*this, Page, ETab::Course);
    TestFalse(TEXT("Tees are on the player card"), Shown(Page, SRoundSetup::TeesTag));
    const auto Slider = RenderedPlay::WithTag(Page, SRoundSetup::GreenSpeedTag);
    if (!TestTrue(TEXT("Green speed has a slider"), Slider && Slider->GetTypeAsString() == TEXT("SSlider")))
        return false;
    auto Adjust = [&Slider](EUINavigation Direction, ENavigationGenesis Input)
    {
        Slider->OnNavigation(FGeometry(), FNavigationEvent(FModifierKeysState(), 0, Direction, Input));
        Slider->UpdateAllAttributes();
    };
    Adjust(EUINavigation::Right, ENavigationGenesis::Keyboard);
    TestEqual(TEXT("One step faster"), Page->GetSetup().GreenStimpFeet, 12);

    Click(*this, Page, SChoiceChips::ChipName(SRoundSetup::HoleSetTag, 2));
    TestEqual(TEXT("Back 9 picks holes 10-18"), Page->GetSetup().Holes, HolesFor(EOpenGolfHoleSet::Back, 18));
    TestTrue(TEXT("The summary counts the picked holes"), RenderedPlay::HasText(Page, TEXT("9 holes · Par"), true));
    TestTrue(TEXT("The wind is read against the first picked hole"), TextOf(Page, SCourseConditions::WindTag).EndsWith(TEXT(" · hole 10")));
    Tick(*this, Page, SRoundSetup::HoleTag(3));
    TestEqual(TEXT("Ticking hole 3 adds it"), Page->GetSetup().Holes, TArray<int32>({3, 10, 11, 12, 13, 14, 15, 16, 17, 18}));
    Click(*this, Page, SChoiceChips::ChipName(SRoundSetup::HoleSetTag, 1));
    TestEqual(TEXT("Front 9 picks holes 1-9"), Page->GetSetup().Holes, HolesFor(EOpenGolfHoleSet::Front, 18));
    for (int32 Hole = 1; Hole <= 9; ++Hole)
        Tick(*this, Page, SRoundSetup::HoleTag(Hole));
    TestFalse(TEXT("No holes ticked: no round"), Page->CanStart());
    TestTrue(TEXT("The page says why"), RenderedPlay::HasText(Page, TEXT("Pick at least one hole")));
    const TSharedPtr<SWidget> Start = RenderedPlay::WithTag(Page, SRoundSetup::StartTag);
    TestTrue(TEXT("Start round waits for a hole"), Start.IsValid() && !Start->IsEnabled());
    Tick(*this, Page, SRoundSetup::HoleTag(7));
    Tick(*this, Page, SRoundSetup::HoleTag(12));
    TestTrue(TEXT("Start round is ready"), Page->CanStart() && Start->IsEnabled());

    Click(*this, Page, SRoundSetup::StartTag);
    if (TestTrue(TEXT("Start round hands over the setup"), Started.IsSet()))
    {
        const FOpenGolfRoundSetup& Setup = Started.GetValue();
        TestEqual(TEXT("Picked holes"), Setup.Holes, TArray<int32>({7, 12}));
        TestTrue(TEXT("The rolled wind"), Setup.WindSpeedMph == 9 && Setup.WindFromDeg == Defaults.WindFromDeg);
        TestTrue(TEXT("Gimmie 6 ft"), Setup.bGimmie && Setup.GimmieFeet == LongGimmieFeet);
        TestEqual(TEXT("The picked tees"), Setup.TeeIndex, 1);
        TestEqual(TEXT("One step faster"), Setup.GreenStimpFeet, 12);
    }
    Click(*this, Page, SRoundSetup::ResetTag);
    TestTrue(TEXT("Reset to defaults restores Game Defaults and every hole, and keeps the wind"), Page->GetSetup() == Defaults);
    for (int32 Step = 0; Step < 10; ++Step)
        Adjust(EUINavigation::Right, ENavigationGenesis::Controller);
    TestEqual(TEXT("The slider stops at the fastest green"), Page->GetSetup().GreenStimpFeet, MaxRoundStimpFeet);
    for (int32 Step = 0; Step < 10; ++Step)
        Adjust(EUINavigation::Left, ENavigationGenesis::Keyboard);
    TestEqual(TEXT("The slider stops at the slowest green"), Page->GetSetup().GreenStimpFeet, MinRoundStimpFeet);
    TestEqual(TEXT("Reset also resets the tee dropdown"), TeePicker->GetSelectedItem(), FirstTees);
    Click(*this, Page, PageHeader::BackTag());
    TestTrue(TEXT("Back leaves the page"), bBack);

    // Signed in, Players shows who is playing.
    {
        using namespace OpenGolfOnline;
        auto* Session = NewObject<UOpenGolfOnlineSubsystem>(NewObject<UGameInstance>());
        FConfig Config;
        Config.Authority = TEXT("https://login.example.test/");
        Config.ClientId = TEXT("test-client");
        Config.Audience = TEXT("https://api.example.test");
        Config.ApiOrigin = Config.Audience;
        Config.ApplicationName = TEXT("UOpenGolf");
        Config.StorageName = TEXT("RoundSetupMenuTest");
        Config.CallbackPorts = {47802};
        Session->SetupForTests(Config, [](const FString&, const FString&, const FString&, const FString&, FHttpCallback)
        {
            auto Cancel = MakeShared<FRequestCancellation, ESPMode::ThreadSafe>();
            Cancel->Finished.store(true);
            return Cancel;
        }, FPaths::ProjectSavedDir() / TEXT("Automation") / FGuid::NewGuid().ToString());
        Session->AccountKey = TEXT("round-setup-test");
        Session->RefreshToken = TEXT("test-refresh");
        Session->AccessToken = TEXT("test-access");
        Session->TokenExpiresAt = FPlatformTime::Seconds() + 3600;
        Session->Profile = MakeShared<FJsonObject>();
        Session->Profile->SetStringField(TEXT("display_name"), TEXT("Pat Putter"));
        const TSharedRef<SRoundSetup> Signed = SNew(SRoundSetup).Style(&Style).Course(Hunter).Setup(Defaults).Defaults(Defaults).Online(Session);
        Refresh(Signed);
        TestFalse(TEXT("The signed-out message goes away"), RenderedPlay::HasText(Signed, TEXT("Guest")));
        TestTrue(TEXT("Signed in: the player card"), Shown(Signed, SRoundSetup::PlayerTag));
        TestTrue(TEXT("The player's name"), RenderedPlay::HasText(Signed, TEXT("Pat Putter")));
        TestTrue(TEXT("The player's avatar"), Shown(Signed, SRoundSetup::PlayerAvatarTag));
    }

    // Course information from the definition: a course with every field, and the bundled range without them.
    {
        UGolfCourseDefinition* Described = DuplicateObject(Hunter, GetTransientPackage());
        Described->Location = FText::FromString(TEXT("Somewhere, Someplace"));
        Described->Designer = FText::FromString(TEXT("A. Architect"));
        Described->ElevationM = 263.0f;
        const TSharedRef<SRoundSetup> Info = SNew(SRoundSetup).Style(&Style).Course(Described).Setup(Defaults).Defaults(Defaults);
        TestTrue(TEXT("Location"), RenderedPlay::HasText(Info, TEXT("Somewhere, Someplace")));
        TestTrue(TEXT("Designer"), RenderedPlay::HasText(Info, TEXT("A. Architect")));
        TestEqual(TEXT("Elevation in the short units"), TextOf(Info, SCourseInformation::ValueTag(SCourseInformation::EField::Elevation)),
            GolfSettings::RoundDistance(263.0f, true));
    }
    FOpenGolfRoundSetup Still = RoundConditions::Defaults(*Savanna, Preferences);
    const TSharedRef<SRoundSetup> Single = SNew(SRoundSetup).Style(&Style).Course(Savanna).Setup(Still).Defaults(Still);
    TestFalse(TEXT("A course without tee sets omits the field"), RenderedPlay::HasText(Single, TEXT("Tees")));
    OpenTab(*this, Single, ETab::Course);
    TestFalse(TEXT("No hole picker on a single hole"), RenderedPlay::WithTag(Single, SChoiceChips::ChipName(SRoundSetup::HoleSetTag, 0)).IsValid());
    TestTrue(TEXT("A single hole can always start"), Single->CanStart());
    TestFalse(TEXT("No location without one"), Shown(Single, SCourseInformation::ValueTag(SCourseInformation::EField::Location)));
    TestFalse(TEXT("No designer without one"), Shown(Single, SCourseInformation::ValueTag(SCourseInformation::EField::Designer)));
    TestEqual(TEXT("Elevation 0 is sea level"), TextOf(Single, SCourseInformation::ValueTag(SCourseInformation::EField::Elevation)), FString(TEXT("Sea level")));
    TestTrue(TEXT("A calm round says so"), RenderedPlay::HasText(Single, TEXT("No wind")));
    return true;
}
#endif
