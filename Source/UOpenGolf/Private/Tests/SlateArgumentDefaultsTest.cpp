// SPDX-License-Identifier: MIT
// Raw-pointer arguments of the game's and the online plugin's Slate widgets default to null. SLATE_ARGUMENT declares
// its member without an initializer, so an argument a caller omits otherwise holds whatever the memory held before.
// SMainMenu's Online did (2026-11-11): the player controller omits it, and the DebugGame editor crashed starting Play
// when the leftover value resolved to an unrelated object. Development builds happened to see null and passed.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SOpenGolfAccountButton.h"
#include "SOpenGolfAccountPanel.h"
#include "UI/Club/ClubPicker.h"
#include "UI/Menu/MainMenu.h"

namespace SlateArgumentDefaultsTestDetail
{
    /**
     * Constructs TArguments in memory first filled with a non-zero pattern, then runs Check on it. A member the
     * arguments' constructor leaves uninitialized keeps the pattern, so it reads as non-null instead of happening to be
     * null. The pattern is written through a volatile pointer so the compiler cannot drop it.
     */
    template <typename TArguments, typename TCheck>
    void WithPatternedArguments(TCheck Check)
    {
        alignas(TArguments) uint8 Storage[sizeof(TArguments)];
        volatile uint8* Bytes = Storage;
        for (SIZE_T Index = 0; Index < sizeof(TArguments); ++Index)
            Bytes[Index] = 0xA5;
        TArguments* Args = new (Storage) TArguments();
        Check(*Args);
        Args->~TArguments();
    }
} // namespace SlateArgumentDefaultsTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlateArgumentDefaultsTest, "UOpenGolf.UI.ArgumentDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlateArgumentDefaultsTest::RunTest(const FString&)
{
    using namespace SlateArgumentDefaultsTestDetail;
    WithPatternedArguments<SMainMenu::FArguments>([this](const SMainMenu::FArguments& Args)
    {
        TestNull(TEXT("Menu controller defaults to null"), Args._Controller);
        // The player controller omits Online: null means the game instance's service.
        TestNull(TEXT("Menu online override defaults to null"), Args._Online);
    });
    WithPatternedArguments<SClubPicker::FArguments>([this](const SClubPicker::FArguments& Args)
    {
        TestNull(TEXT("Club picker monitor defaults to null"), Args._Monitor);
    });
    WithPatternedArguments<SOpenGolfAccountButton::FArguments>([this](const SOpenGolfAccountButton::FArguments& Args)
    {
        TestNull(TEXT("Account button service defaults to null"), Args._Online);
        TestFalse(TEXT("Account button defaults to sign in"), Args._SignOut);
    });
    WithPatternedArguments<SOpenGolfAccountPanel::FArguments>([this](const SOpenGolfAccountPanel::FArguments& Args)
    {
        TestNull(TEXT("Account panel service defaults to null"), Args._Online);
        TestTrue(TEXT("Account panel draws its heading by default"), Args._ShowHeading);
        TestTrue(TEXT("Account panel defaults to the account page"), Args._Page == EOpenGolfAccountPage::Account);
    });
    return true;
}

#endif
