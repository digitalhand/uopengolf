// SPDX-License-Identifier: MIT
#include "UI/Menu/SUpdatePanel.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "GameUpdateFormat.h"
#include "GameUpdateSubsystem.h"
#include "Styling/CoreStyle.h"
#include "UI/Common/HudStyle.h"
#include "UI/Common/MenuStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace UpdateText
{
    FString Title(const UGameUpdateSubsystem& Updater)
    {
        return TEXT("Update to v") + Updater.GetLatest().Version;
    }

    FString ButtonLabel(const UGameUpdateSubsystem* Updater)
    {
        if (!Updater) return TEXT("Check for updates");
        if (Updater->GetState() == EGameUpdateState::Checking) return TEXT("Checking...");
        return Updater->IsUpdateAvailable() ? Title(*Updater) : TEXT("Check for updates");
    }

    FString Status(const UGameUpdateSubsystem* Updater)
    {
        if (!Updater) return TEXT("Updates are unavailable.");
        const FString Current = TEXT("v") + Updater->GetConfig().CurrentVersion, Latest = TEXT("v") + Updater->GetLatest().Version;
        switch (Updater->GetState())
        {
        case EGameUpdateState::Checking: return TEXT("Checking for a newer version...");
        case EGameUpdateState::UpToDate: return FString::Printf(TEXT("Up to date (%s)."), *Current);
        case EGameUpdateState::Available: return FString::Printf(TEXT("%s is available. You have %s."), *Latest, *Current);
        case EGameUpdateState::Downloading: return FString::Printf(TEXT("Downloading %s..."), *Latest);
        case EGameUpdateState::Verifying: return TEXT("Checking the download...");
        case EGameUpdateState::Installing: return FString::Printf(TEXT("Installing %s..."), *Latest);
        case EGameUpdateState::Failed: return Updater->GetMessage();
        default: return FString::Printf(TEXT("You have %s."), *Current);
        }
    }

    FString PlainNotes(const FString& Markdown)
    {
        TArray<FString> Lines;
        Markdown.ParseIntoArrayLines(Lines, false);
        FString Out;
        for (FString Line : Lines)
        {
            Line.TrimEndInline();
            while (Line.StartsWith(TEXT("#"))) Line.RightChopInline(1);
            Line.TrimStartInline();
            if (Line.StartsWith(TEXT("* ")) || Line.StartsWith(TEXT("- "))) Line = TEXT("•  ") + Line.RightChop(2);
            Line.ReplaceInline(TEXT("**"), TEXT(""));
            Out += Line + TEXT("\n");
        }
        Out.TrimStartAndEndInline();
        return Out;
    }
}

TAttribute<EVisibility> SUpdatePanel::VisibleWhen(TFunction<bool(const UGameUpdateSubsystem&)> Test) const
{
    return TAttribute<EVisibility>::CreateLambda([Weak = Updater, Test]
    {
        const UGameUpdateSubsystem* U = Weak.Get();
        return U && Test(*U) ? EVisibility::Visible : EVisibility::Collapsed;
    });
}

TSharedRef<SWidget> SUpdatePanel::MakeButton(TAttribute<FText> Label, TFunction<void()> Action, bool bPrimary, TAttribute<EVisibility> Visible)
{
    return SNew(SBox).WidthOverride(MenuStyle::MenuButtonWidth).Padding(0, 0, 0, MenuStyle::ButtonGap).Visibility(Visible)
        [SNew(SButton).ButtonStyle(bPrimary ? &Style->PrimaryButton : &Style->Button)
            .OnClicked_Lambda([Action] { Action(); return FReply::Handled(); })
            [SNew(STextBlock).Text(Label).Font(Style->GetFont(MenuStyle::BodySize))
                .ColorAndOpacity(bPrimary ? Style->PrimaryLabelColor : Style->TextColor)]];
}

void SUpdatePanel::Construct(const FArguments& Args)
{
    Updater = Args._Updater;
    Style = Args._Style;
    check(Style);
    const FSimpleDelegate OnClose = Args._OnClose;
    const TWeakObjectPtr<UGameUpdateSubsystem> Weak = Updater;
    auto Text = [Weak](TFunction<FString(const UGameUpdateSubsystem&)> Get)
    {
        return TAttribute<FText>::CreateLambda([Weak, Get]
        {
            const UGameUpdateSubsystem* U = Weak.Get();
            return FText::FromString(U ? Get(*U) : FString());
        });
    };
    using S = EGameUpdateState;
    const auto Offered = [](const UGameUpdateSubsystem& U) { return U.IsUpdateAvailable() && (U.GetState() == S::Available || U.GetState() == S::Failed); };
    const auto InProgress = [](const UGameUpdateSubsystem& U) { return U.GetState() == S::Downloading || U.GetState() == S::Verifying || U.GetState() == S::Installing; };

    BarStyle = FCoreStyle::Get().GetWidgetStyle<FProgressBarStyle>("ProgressBar");
    BarStyle.SetBackgroundImage(FSlateRoundedBoxBrush(HudStyle::HoverTileColor(), 5.f));
    BarStyle.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 5.f));
    BarStyle.SetMarqueeImage(FSlateRoundedBoxBrush(FLinearColor::White, 5.f));

    ChildSlot
    [SNew(SBox).MaxDesiredWidth(760.f).HAlign(HAlign_Left)
        [SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
            [SNew(STextBlock).Font(Style->GetFont(MenuStyle::TitleSize)).ColorAndOpacity(Style->TextColor)
                .Text(Text([](const UGameUpdateSubsystem& U)
                {
                    return U.IsUpdateAvailable() ? UpdateText::Title(U) : FString(TEXT("Updates"));
                }))]
        // What happens next, or why it failed.
        +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)
            [SNew(STextBlock).Font(Style->GetFont(MenuStyle::BodySize)).ColorAndOpacity(Style->LabelColor).AutoWrapText(true)
                .Text(Text([](const UGameUpdateSubsystem& U) -> FString
                {
                    const FString Latest = TEXT("v") + U.GetLatest().Version;
                    switch (U.GetState())
                    {
                    case S::Available:
                        if (U.CanInstall())
                            return FString::Printf(TEXT("Downloads %s, then the game closes, installs the update and restarts. Your settings, sign-in and recordings are kept."),
                                *GameUpdate::FormatSize(U.GetLatest().InstallerSize));
                        return PLATFORM_WINDOWS
                            ? FString::Printf(TEXT("This copy of the game can't update itself. Download %s from its release page."), *Latest)
                            : FString::Printf(TEXT("Download %s from its release page."), *Latest);
                    case S::Downloading: return TEXT("Downloading the update.");
                    case S::Verifying: return TEXT("Checking the download.");
                    case S::Installing: return TEXT("Starting the installer. The game will close and restart.");
                    default: return UpdateText::Status(&U);
                    }
                }))]
        // Release notes, before the download starts.
        +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 20)
            [SNew(SBox).MaxDesiredHeight(280.f).Visibility(VisibleWhen([](const UGameUpdateSubsystem& U)
                { return U.GetState() == S::Available && !UpdateText::PlainNotes(U.GetLatest().Notes).IsEmpty(); }))
                [SNew(SBorder).BorderImage(&Style->Section).Padding(MenuStyle::RowPadding)
                    [SNew(SScrollBox)+SScrollBox::Slot()
                        [SNew(STextBlock).Font(Style->GetFont(MenuStyle::SmallSize)).ColorAndOpacity(Style->TextColor).AutoWrapText(true)
                            .Text(Text([](const UGameUpdateSubsystem& U) { return UpdateText::PlainNotes(U.GetLatest().Notes); }))]]]]
        // Progress.
        +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
            [SNew(SBox).HeightOverride(10.f).Visibility(VisibleWhen(InProgress))
                [SNew(SProgressBar).Style(&BarStyle).FillColorAndOpacity(Style->AccentColor)
                    .Percent_Lambda([Weak] { const UGameUpdateSubsystem* U = Weak.Get(); return U ? U->GetProgress() : TOptional<float>(); })]]
        +SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 20)
            [SNew(STextBlock).Font(Style->GetFont(MenuStyle::SmallSize)).ColorAndOpacity(Style->LabelColor).Visibility(VisibleWhen(InProgress))
                .Text(Text([](const UGameUpdateSubsystem& U)
                {
                    const TOptional<float> Progress = U.GetProgress();
                    const FString Percent = Progress.IsSet() ? FString::Printf(TEXT("%d%%"), FMath::FloorToInt(Progress.GetValue() * 100)) : FString();
                    const FString Detail = U.GetProgressText();
                    return Detail.IsEmpty() || Percent.IsEmpty() ? Percent + Detail : Percent + TEXT("  ·  ") + Detail;
                }))]
        // Actions.
        +SVerticalBox::Slot().AutoHeight()
            [MakeButton(Text([](const UGameUpdateSubsystem& U) { return U.GetState() == S::Failed ? FString(TEXT("Try again")) : FString(TEXT("Download and install")); }),
                [Weak] { if (UGameUpdateSubsystem* U = Weak.Get()) U->DownloadAndInstall(); }, true,
                VisibleWhen([Offered](const UGameUpdateSubsystem& U) { return Offered(U) && U.CanInstall(); }))]
        +SVerticalBox::Slot().AutoHeight()
            [MakeButton(Text([](const UGameUpdateSubsystem& U) { return U.CanInstall() ? FString(TEXT("Release page")) : FString(TEXT("Open release page")); }),
                [Weak] { if (const UGameUpdateSubsystem* U = Weak.Get()) U->OpenReleasePage(); }, false, VisibleWhen(Offered))]
        +SVerticalBox::Slot().AutoHeight()
            [MakeButton(FText::FromString(TEXT("Cancel")), [Weak] { if (UGameUpdateSubsystem* U = Weak.Get()) U->Cancel(); }, false,
                VisibleWhen([](const UGameUpdateSubsystem& U) { return U.GetState() == S::Downloading || U.GetState() == S::Verifying; }))]
        +SVerticalBox::Slot().AutoHeight()
            [MakeButton(Text([](const UGameUpdateSubsystem& U) { return U.GetState() == S::Available ? FString(TEXT("Later")) : FString(TEXT("Close")); }),
                [OnClose] { OnClose.ExecuteIfBound(); }, false, VisibleWhen([InProgress](const UGameUpdateSubsystem& U) { return !InProgress(U); }))]]];
}
