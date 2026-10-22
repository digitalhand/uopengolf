// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "UOpenGolfRange.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenGolfFlagstickComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UObject/UnrealType.h"
#include "WindDisplay.h"

namespace
{
    class FCheckRangeWindSettings : public IAutomationLatentCommand
    {
    public:
        explicit FCheckRangeWindSettings(FAutomationTestBase* InTest) : Test(InTest) {}
        bool Update() override
        {
            UWorld* World = GEditor->PlayWorld;
            auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
            if (!Mode || !Mode->GetBall())
            {
                if (GetCurrentRunTime() < 30)
                    return false;
                Test->AddError(TEXT("Range did not start"));
                return true;
            }
            auto* Range = Mode->GetRange();
            auto* Ball = Mode->GetBall();
            Range->Tick(0.25f);
            Test->TestEqual(TEXT("Saved wind speed reaches the ball"), Ball->WindSpeedMph, 10.0f);
            Test->TestEqual(TEXT("Saved wind direction reaches the ball"), Ball->WindFromDeg, 90.0f);
            Test->TestTrue(TEXT("Flag shares the ball's wind"), Range->GetFlagstick()->GetCloth().GetWindMps().Equals(Ball->GetWindVelocityMps(), 1e-4));
            Test->TestEqual(TEXT("Indicator shares the ball's speed"), WindDisplay::Read(Ball, Range, 0).SpeedMph, 10.0f, 1e-4f);
            Test->TestTrue(TEXT("Indicator shows wind blowing left"), WindDisplay::Read(Ball, Range, 0).Direction.Equals(FVector2D(-1, 0), 1e-5));

            FOpenFairwayLaunch Launch;
            Launch.BallSpeedMph = 60;
            Launch.VerticalLaunchDeg = 20;
            Test->TestTrue(TEXT("Wind test shot launches"), Ball->Launch(Launch));
            const FVector ShotWind = Ball->GetWindVelocityMps();
            Range->WindSpeedMph = 20;
            Range->WindFromDeg = 270;
            Range->Tick(0.25f);
            Test->TestEqual(TEXT("Details speed edit reaches the ball"), Ball->WindSpeedMph, 20.0f);
            Test->TestTrue(TEXT("Moving shot keeps its launch wind"), Ball->GetWindVelocityMps().Equals(ShotWind, 1e-4));
            Test->TestTrue(TEXT("Flag keeps showing the active shot wind"), Range->GetFlagstick()->GetCloth().GetWindMps().Equals(ShotWind, 1e-4));
            Test->TestEqual(TEXT("Indicator keeps the active shot speed"), WindDisplay::Read(Ball, Range, 0).SpeedMph, 10.0f, 1e-4f);
            Test->TestTrue(TEXT("Indicator keeps the active shot direction"), WindDisplay::Read(Ball, Range, 0).Direction.Equals(FVector2D(-1, 0), 1e-5));
            Mode->ResetBallToTee();
            Range->Tick(0.25f);
            Test->TestTrue(TEXT("New direction applies after the shot"), Ball->GetWindVelocityMps().Y > 8.9);
            Test->TestTrue(TEXT("Flag follows the new direction"), Range->GetFlagstick()->GetCloth().GetWindMps().Equals(Ball->GetWindVelocityMps(), 1e-4));
            Test->TestEqual(TEXT("Indicator uses new speed after reset"), WindDisplay::Read(Ball, Range, 0).SpeedMph, 20.0f, 1e-4f);
            Test->TestTrue(TEXT("Indicator uses new direction after reset"), WindDisplay::Read(Ball, Range, 0).Direction.Equals(FVector2D(1, 0), 1e-5));
            Test->TestEqual(TEXT("Without a ball indicator uses range wind"), WindDisplay::Read(nullptr, Range, 0).SpeedMph, 20.0f, 1e-4f);

            IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindMph"))->Set(5.0f, ECVF_SetByConsole);
            IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindFromDeg"))->Set(180.0f, ECVF_SetByConsole);
            Range->Tick(0.25f);
            Test->TestEqual(TEXT("Console speed updates the range control"), Range->WindSpeedMph, 5.0f);
            Test->TestEqual(TEXT("Console direction updates the range control"), Range->WindFromDeg, 180.0f);
            Test->TestEqual(TEXT("Console override reaches the ball"), Ball->WindSpeedMph, 5.0f);
            Range->WindSpeedMph = 0;
            Range->Tick(0.25f);
            Test->TestTrue(TEXT("Details can restore calm after a console override"), Ball->GetWindVelocityMps().IsNearlyZero());
            Range->WindSpeedMph = -5;
            Range->WindFromDeg = -90;
            Range->Tick(0.25f);
            Test->TestEqual(TEXT("Negative wind speed becomes calm"), Ball->WindSpeedMph, 0.0f);
            Test->TestEqual(TEXT("Direction wraps around a full turn"), Ball->WindFromDeg, 270.0f);
            return true;
        }

    private:
        FAutomationTestBase* Test;
    };

    class FRestoreRangeWind : public IAutomationLatentCommand
    {
    public:
        FRestoreRangeWind(AUOpenGolfRange* InRange, float InSpeed, float InDirection, bool bInPreview, float InConsoleSpeed, float InConsoleDirection)
            : Range(InRange), Speed(InSpeed), Direction(InDirection), bPreview(bInPreview), ConsoleSpeed(InConsoleSpeed), ConsoleDirection(InConsoleDirection) {}
        bool Update() override
        {
            IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindMph"))->Set(ConsoleSpeed, ECVF_SetByConsole);
            IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindFromDeg"))->Set(ConsoleDirection, ECVF_SetByConsole);
            if (Range.IsValid())
            {
                Range->WindSpeedMph = Speed;
                Range->WindFromDeg = Direction;
                Range->bPreviewWindInEditor = bPreview;
                Range->OnConstruction(Range->GetActorTransform());
            }
            return true;
        }

    private:
        TWeakObjectPtr<AUOpenGolfRange> Range;
        float Speed, Direction;
        bool bPreview;
        float ConsoleSpeed, ConsoleDirection;
    };
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangeWindSettingsTest, "UOpenGolf.Range.WindSettings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRangeWindSettingsTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
        return false;
    AUOpenGolfRange* Range = nullptr;
    for (TActorIterator<AUOpenGolfRange> It(GEditor->GetEditorWorldContext().World()); It; ++It)
        Range = *It;
    if (!TestNotNull(TEXT("Saved range settings actor"), Range))
        return false;
    for (FName Name : {FName(TEXT("WindSpeedMph")), FName(TEXT("WindFromDeg")), FName(TEXT("bPreviewWindInEditor"))})
    {
        FProperty* Property = Range->GetClass()->FindPropertyByName(Name);
        if (TestNotNull(*Name.ToString(), Property))
        {
            TestTrue(TEXT("Wind control is editable and saved"), Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_Transient | CPF_DisableEditOnInstance));
        }
    }
    auto Restore = MakeShared<FRestoreRangeWind>(Range, Range->WindSpeedMph, Range->WindFromDeg, Range->bPreviewWindInEditor,
        IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindMph"))->GetFloat(),
        IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindFromDeg"))->GetFloat());
    Range->WindSpeedMph = 10;
    Range->WindFromDeg = 90;
    Range->bPreviewWindInEditor = true;
    FPropertyChangedEvent WindEdit(Range->GetClass()->FindPropertyByName(TEXT("WindSpeedMph")));
    Range->PostEditChangeProperty(WindEdit);
    auto* Flag = Range->GetFlagstick();
    TestTrue(TEXT("Saved preview enables range ticks after loading"), Range->ShouldTickIfViewportsOnly());
    TestTrue(TEXT("Editor flag receives the saved wind without a game ball"), Flag->GetCloth().GetWindMps().Y < -4.4);
    TestTrue(TEXT("Preview enables editor cloth ticking"), Flag->bTickInEditor);
    const FVector Before = Flag->GetCloth().GetPosition(16, 4);
    for (int32 Frame = 0; Frame < 600; ++Frame)
        Flag->TickComponent(1.0f / 60.0f, LEVELTICK_ViewportsOnly, nullptr);
    TestTrue(TEXT("Editor preview moves the cloth"), !Flag->GetCloth().GetPosition(16, 4).Equals(Before, 1.0));
    Range->bPreviewWindInEditor = false;
    Range->OnConstruction(Range->GetActorTransform());
    TestFalse(TEXT("Preview can be disabled"), Flag->bTickInEditor);
    TestFalse(TEXT("Disabled preview stops range editor ticks"), Range->ShouldTickIfViewportsOnly());
    Range->bPreviewWindInEditor = true;
    Range->OnConstruction(Range->GetActorTransform());
    IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindMph"))->Set(35.0f, ECVF_SetByConsole);
    IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindFromDeg"))->Set(180.0f, ECVF_SetByConsole);
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCheckRangeWindSettings>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    FAutomationTestFramework::Get().EnqueueLatentCommand(Restore);
    return true;
}
#endif
