// SPDX-License-Identifier: MIT
#pragma once

#include "Widgets/SCompoundWidget.h"
#include "OpenGolfAccountStyle.h"

class AUOpenGolfPlayerController;
class SEditableTextBox;
template <typename OptionType>
class SComboBox;

/**
 * Local range controls; presets and the custom shot still enter through the launch-monitor boundary. Custom opens
 * five fields (speed in the player's units, launch, direction, spin, axis) that start as a basic straight shot.
 */
class SReplayShotPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SReplayShotPanel) {}
    SLATE_ARGUMENT(TWeakObjectPtr<AUOpenGolfPlayerController>, Controller)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event) override;

    /** The custom shot's fields: 0 speed, 1 launch angle, 2 direction, 3 total spin, 4 spin axis. */
    static constexpr int32 CustomFieldCount = 5;
    /** The Slate tag of a custom field's text box. */
    static FName CustomFieldTag(int32 Field);

private:
    FText SelectedName() const;
    bool IsLaunchMonitorConnected() const;
    bool IsCustom() const;
    /** True while a custom field holds keyboard focus. */
    bool IsEditingField() const;
    FReply HitShot();
    TSharedRef<SWidget> MakeFieldRow(int32 Field);
    /** The stored custom value of Field as it is shown: speed in the player's units, angles in degrees, spin in rpm. */
    FText FieldText(int32 Field) const;
    /** Stores Text in Field if it is a number (limited to what the launch monitor accepts), then shows the stored value. */
    void CommitField(int32 Field, const FText& Text);
    FOpenGolfAccountStyle Style;
    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    TArray<TSharedPtr<FString>> Options;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> ShotList;
    TSharedPtr<SEditableTextBox> Fields[CustomFieldCount];
};
