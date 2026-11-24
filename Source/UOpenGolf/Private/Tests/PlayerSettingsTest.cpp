// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/Menu/MainMenu.h"
#include "OpenGolfOnlineSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Text/STextBlock.h"

namespace PlayerSettingsTestDetail
{
    TSharedPtr<SWidget> FindTag(const TSharedRef<SWidget>& Root, const FName Tag)
    {
        if (Root->GetTag() == Tag) return Root;
        FChildren* Children = Root->GetChildren();
        for (int32 I = 0; I < Children->Num(); ++I)
            if (auto Found = FindTag(Children->GetChildAt(I), Tag)) return Found;
        return nullptr;
    }
    bool HasText(const TSharedRef<SWidget>& Root, const FString& Text)
    {
        if (!Root->GetVisibility().IsVisible()) return false;
        if (Root->GetTypeAsString() == TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Root)->GetText().ToString() == Text) return true;
        FChildren* Children = Root->GetChildren();
        for (int32 I = 0; I < Children->Num(); ++I)
            if (HasText(Children->GetChildAt(I), Text)) return true;
        return false;
    }
    TSharedPtr<SButton> FindButton(const TSharedRef<SWidget>& Root, const FString& Text = {})
    {
        if (!Root->GetVisibility().IsVisible()) return nullptr;
        if (Root->GetTypeAsString() == TEXT("SButton") && (Text.IsEmpty() || HasText(Root, Text))) return StaticCastSharedRef<SButton>(Root);
        FChildren* Children = Root->GetChildren();
        for (int32 I = 0; I < Children->Num(); ++I)
            if (auto Found = FindButton(Children->GetChildAt(I), Text)) return Found;
        return nullptr;
    }
    /** Whether Tag is under Root with every widget on the way visible. */
    bool IsTagShown(const TSharedRef<SWidget>& Root, const FName Tag)
    {
        if (!Root->GetVisibility().IsVisible()) return false;
        if (Root->GetTag() == Tag) return true;
        FChildren* Children = Root->GetChildren();
        for (int32 I = 0; I < Children->Num(); ++I)
            if (IsTagShown(Children->GetChildAt(I), Tag)) return true;
        return false;
    }
    TSharedPtr<SEditableText> FindEditableText(const TSharedRef<SWidget>& Root)
    {
        if (Root->GetTypeAsString() == TEXT("SEditableText")) return StaticCastSharedRef<SEditableText>(Root);
        for (int32 I = 0; I < Root->GetChildren()->Num(); ++I)
            if (auto Found = FindEditableText(Root->GetChildren()->GetChildAt(I))) return Found;
        return nullptr;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfPlayerSettingsTest, "UOpenGolf.Menu.PlayerProfile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOpenGolfPlayerSettingsTest::RunTest(const FString&)
{
    using namespace PlayerSettingsTestDetail;
    using namespace OpenGolfOnline;
    auto* Session = NewObject<UOpenGolfOnlineSubsystem>(NewObject<UGameInstance>());
    FConfig Config;
    Config.Authority = TEXT("https://login.example.test/"); Config.ClientId = TEXT("test-client");
    Config.Audience = TEXT("https://api.example.test"); Config.ApiOrigin = Config.Audience;
    Config.ApplicationName = TEXT("UOpenGolf"); Config.StorageName = TEXT("PlayerSettingsTest"); Config.CallbackPorts = {47801};
    struct FPending { FString Verb, Body; FHttpCallback Done; };
    TArray<FPending> Pending;
    const FString Root = FPaths::ProjectSavedDir() / TEXT("Automation") / FGuid::NewGuid().ToString();
    Session->SetupForTests(Config, [&Pending](const FString&, const FString& Verb, const FString& Body, const FString&, FHttpCallback Done)
    {
        Pending.Add({Verb, Body, MoveTemp(Done)});
        auto Cancel = MakeShared<FRequestCancellation, ESPMode::ThreadSafe>(); Cancel->Finished.store(true); return Cancel;
    }, Root);
    // A verified synthetic account; requests use the test transport exclusively.
    Session->AccountKey = TEXT("player-one"); Session->RefreshToken = TEXT("test-refresh"); Session->AccessToken = TEXT("test-access");
    Session->TokenExpiresAt = FPlatformTime::Seconds() + 3600;
    Session->Profile = MakeShared<FJsonObject>(); Session->Profile->SetStringField(TEXT("display_name"), TEXT("Golfer"));
    Session->Permissions.Add(TEXT("ogolf:admin"));
    const auto Menu = SNew(SMainMenu).Online(Session);
    auto Refresh = [&]
    {
        Menu->Tick(FGeometry(), 0, 0);
        if (auto Panel = FindTag(Menu, TEXT("PlayerAccountPanel"))) Panel->Tick(FGeometry(), 0, 0);
        Menu->SlatePrepass(1.0f);
    };
    auto Click = [&](const TSharedPtr<SButton>& Button)
    {
        if (!TestTrue(TEXT("Expected button is enabled"), Button.IsValid() && Button->IsEnabled())) return false;
        Button->SimulateClick(); Refresh(); return true;
    };
    auto ClickText = [&](const FString& Text){return Click(FindButton(Menu, Text));};
    auto Edit = [&]
    {
        auto Pencil = FindTag(Menu, TEXT("EditDisplayName"));
        return TestTrue(TEXT("Edit pencil exists"), Pencil.IsValid()) && Click(FindButton(Pencil.ToSharedRef()));
    };
    auto Field = [&]{ return StaticCastSharedPtr<SEditableTextBox>(FindTag(Menu, TEXT("DisplayNameField"))); };
    auto TypeName = [&](const FString& Name)
    {
        // Use character input: SetText would replace the field's bound text attribute.
        Field()->SelectAllText();
        TSharedPtr<SWidget> Editable = FindEditableText(Field().ToSharedRef());
        if (Name.IsEmpty()) Editable->OnKeyChar(FGeometry(), FCharacterEvent(TCHAR('\b'), FModifierKeysState(), 0, false));
        else for (TCHAR C : Name) Editable->OnKeyChar(FGeometry(), FCharacterEvent(C, FModifierKeysState(), 0, false));
    };
    Menu->Open(); Refresh();
    auto Settings = FindTag(Menu, TEXT("MenuSettings"));
    if (!Settings || !Click(FindButton(Settings.ToSharedRef())) || !TestTrue(TEXT("Name field exists"), Field().IsValid())) return false;
    TestTrue(TEXT("Display name starts read-only"), Field()->IsReadOnly());
    TestFalse(TEXT("Separate save-name button removed"), HasText(Menu, TEXT("Save display name")));
    TestTrue(TEXT("Admin permission shows label"), HasText(Menu, TEXT("admin")));
    Session->Permissions.Reset(); Refresh();
    TestFalse(TEXT("Label hidden without permission"), HasText(Menu, TEXT("admin")));
    Session->Permissions.Add(TEXT("ogolf:Admin")); Refresh();
    TestFalse(TEXT("Permission label requires exact match"), HasText(Menu, TEXT("admin")));
    {
        // The avatar picker fills the profile's right half. Apply is not clicked: it would write this computer's preferences.
        auto Panel = FindTag(Menu, TEXT("PlayerAccountPanel"));
        if (!TestTrue(TEXT("The profile holds the avatar picker"), Panel.IsValid() && FindTag(Panel.ToSharedRef(), TEXT("AvatarPicker")).IsValid()))
            return false;
        auto ShownName = [&]
        {
            auto Name = FindTag(Menu, TEXT("AvatarShownName"));
            return Name.IsValid() ? StaticCastSharedPtr<STextBlock>(Name)->GetText().ToString() : FString();
        };
        auto Pick = [&](const TCHAR* Tag) { return Click(StaticCastSharedPtr<SButton>(FindTag(Menu, Tag))); };
        TestTrue(TEXT("The picker offers at least 20 avatars"), IsTagShown(Menu, TEXT("Avatar.Birdie")) && IsTagShown(Menu, TEXT("Avatar.HoleInOne")));
        TestFalse(TEXT("Without an account picture there is no Account tile"), IsTagShown(Menu, TEXT("Avatar.Account")));
        TestEqual(TEXT("Without a pick or a picture the default avatar shows"), ShownName(), FString(TEXT("Golfer")));
        if (!Pick(TEXT("Avatar.Birdie"))) return false;
        TestEqual(TEXT("A pick shows at once"), ShownName(), FString(TEXT("Birdie")));
        TestTrue(TEXT("A pick is an unsaved change"), HasText(Menu, TEXT("Unsaved changes")));
        TestEqual(TEXT("Requests are not sent for an avatar"), Pending.Num(), 0);
        if (!ClickText(TEXT("Discard"))) return false;
        TestEqual(TEXT("Discard restores the avatar"), ShownName(), FString(TEXT("Golfer")));
        TestTrue(TEXT("Discard leaves nothing unsaved"), HasText(Menu, TEXT("All changes saved")));
        Session->Picture = UTexture2D::CreateTransient(4, 4);
        Refresh();
        TestTrue(TEXT("An account picture adds the Account tile"), IsTagShown(Menu, TEXT("Avatar.Account")));
        TestEqual(TEXT("Without a pick the account picture shows"), ShownName(), FString(TEXT("Account picture")));
        if (!Pick(TEXT("Avatar.WaterHazard"))) return false;
        TestEqual(TEXT("A pick wins over the account picture"), ShownName(), FString(TEXT("Water Hazard")));
        if (!Pick(TEXT("Avatar.Account"))) return false;
        TestEqual(TEXT("The account picture can be picked again"), ShownName(), FString(TEXT("Account picture")));
        if (!ClickText(TEXT("Discard"))) return false;
        Session->Picture = nullptr;
        Refresh();
    }
    if (!Edit()) return false;
    TestFalse(TEXT("Pencil enables editing"), Field()->IsReadOnly());
    TypeName(TEXT("Draft Golfer")); Refresh();
    TestEqual(TEXT("Typing sends no requests"), Pending.Num(), 0);
    TestTrue(TEXT("Name participates in Settings unsaved state"), HasText(Menu, TEXT("Unsaved changes")));
    if (!ClickText(TEXT("Launch Monitor")) || !ClickText(TEXT("Player"))) return false;
    TestEqual(TEXT("Category navigation keeps draft"), Field()->GetText().ToString(), FString(TEXT("Draft Golfer")));
    if (!ClickText(TEXT("Discard"))) return false;
    TestEqual(TEXT("Discard restores saved name"), Field()->GetText().ToString(), FString(TEXT("Golfer")));
    TestTrue(TEXT("Discard locks field"), Field()->IsReadOnly());
    if (!Edit()) return false;
    TypeName(TEXT("")); Refresh();
    if (!ClickText(TEXT("Apply"))) return false;
    TestEqual(TEXT("Empty name not submitted"), Pending.Num(), 0);
    TestTrue(TEXT("Empty name has an error"), HasText(Menu, TEXT("Enter a display name.")));
    TypeName(TEXT("  New Golfer  ")); Refresh();
    if (!ClickText(TEXT("Apply")) || !TestEqual(TEXT("Apply sends one request"), Pending.Num(), 1)) return false;
    TestEqual(TEXT("Apply patches profile"), Pending[0].Verb, FString(TEXT("PATCH")));
    TestTrue(TEXT("Submitted name trimmed"), Pending[0].Body.Contains(TEXT("\"New Golfer\"")));
    TestTrue(TEXT("Saving locks field"), Field()->IsReadOnly());
    auto Reply = MoveTemp(Pending[0].Done); Pending.Empty(); Reply({503, TEXT("{}"), true}); Refresh();
    TestFalse(TEXT("Failed save permits editing"), Field()->IsReadOnly());
    TestEqual(TEXT("Failed save retains draft"), Field()->GetText().ToString(), FString(TEXT("  New Golfer  ")));
    TestTrue(TEXT("Failed save offers retry"), HasText(Menu, TEXT("Could not save the display name. Try Apply again.")));
    if (!ClickText(TEXT("Apply")) || !TestEqual(TEXT("Retry sends one request"), Pending.Num(), 1)) return false;
    Reply = MoveTemp(Pending[0].Done); Pending.Empty(); Reply({200, TEXT("{\"display_name\":\"New Golfer\"}"), true}); Refresh();
    TestTrue(TEXT("Successful save locks field"), Field()->IsReadOnly());
    TestEqual(TEXT("Successful save shows accepted name"), Field()->GetText().ToString(), FString(TEXT("New Golfer")));
    TestTrue(TEXT("Successful save clears dirty state"), HasText(Menu, TEXT("All changes saved")));
    if (!Edit()) return false;
    TypeName(TEXT("Private Draft")); Refresh();
    Session->AccountKey = TEXT("player-two"); Session->Profile = MakeShared<FJsonObject>();
    Session->Profile->SetStringField(TEXT("display_name"), TEXT("Other Golfer"));
    Refresh();
    // Tick the nested panel just as Slate does in a live window.
    if (auto Panel = FindTag(Menu, TEXT("PlayerAccountPanel"))) Panel->Tick(FGeometry(), 0, 0);
    Refresh();
    TestFalse(TEXT("Account switch clears previous draft"), Field().IsValid() && Field()->GetText().ToString() == TEXT("Private Draft"));
    Session->RefreshToken.Empty(); Refresh();
    TestFalse(TEXT("Signed-out account has no admin label"), HasText(Menu, TEXT("admin")));
    IFileManager::Get().DeleteDirectory(*Root, false, true);
    return true;
}
#endif
