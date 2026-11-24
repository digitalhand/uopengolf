// SPDX-License-Identifier: MIT
#include "UI/Menu/MainMenu.h"
#include "OpenGolfEnvironment.h"
#include "UOpenGolfPlayerController.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/SBoxPanel.h"

void SMainMenu::BuildEnvironment(TSharedRef<SVerticalBox> Rows)
{
    const TWeakObjectPtr<AOpenGolfEnvironment> Environment = AOpenGolfEnvironment::Find(Controller.IsValid() ? Controller->GetWorld() : nullptr);
    if (!Environment.IsValid())
    {
        Disabled(Rows, TEXT("Environment"), TEXT("This course has no environment controls."));
        return;
    }

    Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
        [Text(TEXT("Changes apply immediately for this course visit."), 16, true)];
    if (Matches(TEXT("Daylight preset"), TEXT("Morning mist, day, sunset")))
    {
        auto Presets = SNew(SHorizontalBox);
        for (const auto& [Label, Preset] : {
                 TPair<const TCHAR*, EOpenGolfDaylightPreset>(TEXT("Morning"), EOpenGolfDaylightPreset::Morning),
                 TPair<const TCHAR*, EOpenGolfDaylightPreset>(TEXT("Day"), EOpenGolfDaylightPreset::Day),
                 TPair<const TCHAR*, EOpenGolfDaylightPreset>(TEXT("Sunset"), EOpenGolfDaylightPreset::Sunset)})
        {
            Presets->AddSlot().FillWidth(1).Padding(2, 0)
                [Button(Label, [Environment, Preset]
                    {
                    if (auto* Actor = Environment.Get())
                        Actor->SelectPreset(Preset); }, false, MenuStyle::NavSize)];
        }
        AddRow(Rows, TEXT("Daylight preset"), FString(), Presets, true, 280);
    }

    auto CommitSlider = [Environment]
    {
        if (auto* Actor = Environment.Get())
            Actor->ApplySettings(Actor->GetSettings());
    };
    auto Slider = [this, Rows, Environment, CommitSlider](const TCHAR* Label, const TCHAR* Help, float FOpenGolfEnvironmentSettings::* Member, float Scale)
    {
        if (!Matches(Label, Help))
            return;
        AddRow(Rows, Label, Help,
            SNew(SSlider)
                .Value_Lambda([Environment, Member, Scale]
                    {
                    const auto* Actor = Environment.Get();
                    return Actor ? (Actor->GetSettings().*Member) / Scale : 0.f; })
                .OnValueChanged_Lambda([Environment, Member, Scale](float Value)
                    {
                    if (auto* Actor = Environment.Get())
                    {
                        auto Settings = Actor->GetSettings();
                        Settings.*Member = Value * Scale;
                        Actor->ApplySettings(Settings, false);
                    } })
                .OnMouseCaptureEnd_Lambda(CommitSlider)
                .OnControllerCaptureEnd_Lambda(CommitSlider));
    };
    Slider(TEXT("Time of day"), TEXT("Morning to sunset."), &FOpenGolfEnvironmentSettings::Daylight, 1);
    Slider(TEXT("Cloud coverage"), TEXT("Clear to cloudy."), &FOpenGolfEnvironmentSettings::CloudCoverage, 1);
    Slider(TEXT("Cloud movement"), TEXT("Still to faster drifting clouds."), &FOpenGolfEnvironmentSettings::CloudSpeed, 5);
    AddChoice(Rows, TEXT("Automatic daylight"), TEXT("Advances to sunset, then holds."), {TEXT("Off"), TEXT("On")}, {0, 1}, [Environment]
        { return Environment.IsValid() && Environment->GetSettings().bAutomaticTime ? 1 : 0; }, [Environment](int32 Value)
        {
            if (auto* Actor = Environment.Get())
            {
                auto Settings = Actor->GetSettings();
                Settings.bAutomaticTime = Value != 0;
                Actor->ApplySettings(Settings);
            } });
    AddChoice(Rows, TEXT("Daylight duration"), TEXT("Real minutes from morning to sunset."), {TEXT("15 minutes"), TEXT("30 minutes"), TEXT("60 minutes"), TEXT("120 minutes")}, {15, 30, 60, 120}, [Environment]
        { return Environment.IsValid() ? FMath::RoundToInt(Environment->GetSettings().DurationMinutes) : 60; }, [Environment](int32 Value)
        {
            if (auto* Actor = Environment.Get())
            {
                auto Settings = Actor->GetSettings();
                Settings.DurationMinutes = Value;
                Actor->ApplySettings(Settings);
            } });
    if (Matches(TEXT("Reset to course defaults"), TEXT("Environment")))
        Rows->AddSlot().AutoHeight().HAlign(HAlign_Right)
            [Button(TEXT("Reset to course defaults"), [Environment]
                {
                if (auto* Actor = Environment.Get())
                    Actor->ResetToCourseDefaults(); })];
}
