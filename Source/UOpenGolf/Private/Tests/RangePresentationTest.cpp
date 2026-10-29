// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "OpenFairway/WindProfile.h"
#include "OpenGolfFlagstickComponent.h"
#include "ProceduralMeshComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfBall.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"
#include "UnrealClient.h"
#include "ShaderCompiler.h"

class FCheckRangePresentation : public IAutomationLatentCommand
{
  public:
    FCheckRangePresentation(FAutomationTestBase *InTest, bool bInCapture)
        : Test(InTest), bCapture(bInCapture)
    {
        GrassVar = IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.Grass3D"));
        OriginalGrass = GrassVar->GetInt();
    }
    ~FCheckRangePresentation()
    {
        GrassVar->Set(OriginalGrass, ECVF_SetByConsole);
    }

    bool Update() override
    {
        UWorld *World = GEditor->PlayWorld;
        auto *Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        if (GetCurrentRunTime() > 540)
        {
            Test->AddError(TEXT("Timed out checking range presentation."));
            FScreenshotRequest::Reset();
            return true;
        }
        if (!Mode || !World->HasBegunPlay())
            return false;
        if (bCapture && GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
            return false;
        auto *Range = Mode->GetRange();
        if (Stage == 0)
        {
            Test->TestNotNull(TEXT("Game ball presentation subclass"), Cast<AUOpenGolfBall>(Mode->GetBall()));
            const auto *Mesh = Mode->GetBall()->FindComponentByClass<UStaticMeshComponent>();
            Test->TestEqual(TEXT("Imported ball mesh"), Mesh->GetStaticMesh()->GetName(), FString(TEXT("SM_GolfBall")));
            Test->TestEqual(TEXT("Ball mesh diameter before actor scale"), Mesh->GetStaticMesh()->GetBounds().BoxExtent.GetMax() * 2, 100.0, 0.1);
            Test->TestEqual(TEXT("Ball visuals have no collision"), Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
            Test->TestEqual(TEXT("Ball world diameter in cm"), Mesh->Bounds.BoxExtent.GetMax() * 2, 4.267, 0.01);
            TArray<UStaticMeshComponent *> Parts;
            Range->GetLayoutComponents(Parts);
            for (auto *Part : Parts)
            {
                if (Part->GetFName() == TEXT("Flagstick"))
                {
                    Test->TestTrue(TEXT("Flag is the course-kit flagstick"), Part->IsA<UOpenGolfFlagstickComponent>());
                    const FBox Bounds = Part->GetStaticMesh()->GetBoundingBox();
                    Test->TestEqual(TEXT("Flag top above ground in cm"), Bounds.Max.Z, static_cast<double>(UOpenGolfFlagstickComponent::PoleHeightCm), 0.01);
                    Test->TestEqual(TEXT("Pole diameter in cm"), Bounds.GetSize().X, static_cast<double>(UOpenGolfFlagstickComponent::PoleDiameterCm), 0.01);
                    // The flag is simulated cloth drawn by a procedural mesh under the pole.
                    UProceduralMeshComponent *Cloth = Range->GetFlagstick()->GetFlagCloth();
                    if (Test->TestNotNull(TEXT("Flag cloth component"), Cloth))
                    {
                        Test->TestEqual(TEXT("Flag cloth vertices"), Cloth->GetProcMeshSection(0) ? Cloth->GetProcMeshSection(0)->ProcVertexBuffer.Num() : 0,
                                        FOpenGolfFlagCloth::Columns * FOpenGolfFlagCloth::Rows);
                        Test->TestEqual(TEXT("Flag cloth has no collision"), Cloth->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
                    }
                    // Still air at the start: the flag hangs below the pole top.
                    const FVector PoleTop = Part->GetComponentLocation() + FVector(0.0, 0.0, UOpenGolfFlagstickComponent::PoleHeightCm);
                    const FVector FreeCorner = Range->GetFlagstick()->GetCloth().GetPosition(FOpenGolfFlagCloth::Columns - 1, 0);
                    Test->TestTrue(FString::Printf(TEXT("Calm flag hangs (free corner %.1f cm below the top)"), PoleTop.Z - FreeCorner.Z),
                                   FreeCorner.Z < PoleTop.Z - 30.0);
                }
            }
            // A 10 mph wind from the right through the console variables reaches the ball and then the flag;
            // a 13 ft green speed reaches the ball.
            IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindMph"))->Set(10.0f, ECVF_SetByConsole);
            IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindFromDeg"))->Set(90.0f, ECVF_SetByConsole);
            IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.GreenStimp"))->Set(13.0f, ECVF_SetByConsole);
            GrassVar->Set(1, ECVF_SetByConsole);
            ReadyAt = GetCurrentRunTime() + 0.5;
            Stage = 1;
            return false;
        }
        if (GetCurrentRunTime() < ReadyAt)
            return false;
        if (Stage == 1)
        {
            // UOpenGolf.Course.FlagCloth checks how the cloth flies; here the wind must reach it.
            Test->TestEqual(TEXT("Wind console variable reaches the ball"), Mode->GetBall()->WindSpeedMph, 10.0f, 1e-4f);
            const float FlagShare = OpenFairway::WindProfile::Factor(UOpenGolfFlagstickComponent::FlagCentreHeightCm / 100.0f,
                Mode->GetBall()->GetWindReferenceHeightM(), Mode->GetBall()->GetWindRoughnessM());
            Test->TestTrue(TEXT("Ball wind at flag height reaches the flag"),
                           Range->GetFlagstick()->GetCloth().GetWindMps().Equals(Mode->GetBall()->GetWindVelocityMps() * FlagShare, 0.01));
            Test->TestEqual(TEXT("Green stimp console variable reaches the ball"), Mode->GetBall()->GreenStimpFeet, 13.0f, 1e-4f);
            IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindMph"))->Set(0.0f, ECVF_SetByConsole);
            IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindFromDeg"))->Set(0.0f, ECVF_SetByConsole);
            IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.GreenStimp"))->Set(10.0f, ECVF_SetByConsole);
            // The switch shows and hides the terrain's 3D grass and ground cover; its instances stay saved in the map.
            InitialCount = Range->GetGrassInstanceCount();
            Test->TestTrue(TEXT("Grass is instantiated"), InitialCount > 1000);
            GrassVar->Set(0, ECVF_SetByConsole);
            ReadyAt = GetCurrentRunTime() + 0.5;
            Stage = 2;
            return false;
        }
        if (Stage == 2)
        {
            Test->TestEqual(TEXT("Disabling grass clears all instances"), Range->GetGrassInstanceCount(), 0);
            GrassVar->Set(1, ECVF_SetByConsole);
            ReadyAt = GetCurrentRunTime() + 0.5;
            Stage = 3;
            return false;
        }
        if (Stage == 3)
        {
            Test->TestEqual(TEXT("Re-enabling grass restores deterministic population"), Range->GetGrassInstanceCount(), InitialCount);
            if (!bCapture)
                return true;
            auto *PC = Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController());
            PC->SetReplayPanelOpen(false);
            Camera = World->SpawnActor<ACameraActor>();
            Camera->GetCameraComponent()->SetFieldOfView(70);
            Camera->GetCameraComponent()->bConstrainAspectRatio = false;
            PC->SetViewTarget(Camera);
            Stage = 4;
        }
        if (Stage == 4)
        {
            const FVector Locations[] = {FVector(-600, 0, 220), FVector(-600, 0, 220),
                                         FVector(-18, -18, 10), FVector(AUOpenGolfRange::GreenDistanceM * 100 - 400, -200, 180),
                                         FVector(-330, -400, 15), FVector(1600, -200, 100),
                                         FVector(AUOpenGolfRange::GreenDistanceM * 100 - 400, -200, 180)};
            const FVector Targets[] = {FVector(6000, 0, 0), FVector(6000, 0, 0),
                                       Mode->GetBall()->GetActorLocation(), FVector(AUOpenGolfRange::GreenDistanceM * 100, 0, 140),
                                       FVector(-200, -380, 0), FVector(2300, 0, 0), FVector(AUOpenGolfRange::GreenDistanceM * 100, 0, 140)};
            Camera->SetActorLocation(Locations[View]);
            Camera->SetActorRotation((Targets[View] - Locations[View]).Rotation());
            GrassVar->Set(View == 1 ? 0 : 1, ECVF_SetByConsole);
            if (View == 6)
            {
                IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindFromDeg"))->Set(90.0f, ECVF_SetByConsole);
                IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindMph"))->Set(10.0f, ECVF_SetByConsole);
            }
            ReadyAt = GetCurrentRunTime() + 5;
            Stage = 5;
            return false;
        }
        if (Stage == 5)
        {
            SampleStart = FPlatformTime::Seconds();
            Frames = 0;
            Stage = 6;
        }
        if (Stage == 6)
        {
            if (++Frames < 120)
                return false;
            Test->AddInfo(FString::Printf(TEXT("View %d: mean editor frame %.3f ms over 119 intervals; grass instances %d"),
                                          View, (FPlatformTime::Seconds() - SampleStart) * 1000 / 119, Range->GetGrassInstanceCount()));
            const FIntPoint Size = World->GetGameViewport()->Viewport->GetSizeXY();
            Test->AddInfo(FString::Printf(TEXT("Capture size %dx%d; camera FOV %.1f"), Size.X, Size.Y,
                                          Camera->GetCameraComponent()->FieldOfView));
            for (const TCHAR *Name : {TEXT("r.AntiAliasingMethod"), TEXT("r.ScreenPercentage"),
                                      TEXT("sg.AntiAliasingQuality"), TEXT("r.TSR.ShadingRejection.Flickering"), TEXT("r.TSR.ThinGeometryDetection")})
            {
                if (const IConsoleVariable *Variable = IConsoleManager::Get().FindConsoleVariable(Name))
                    Test->AddInfo(FString::Printf(TEXT("%s = %s"), Name, *Variable->GetString()));
            }
            const TCHAR *Names[] = {TEXT("TeeGrassOn"), TEXT("TeeGrassOff"), TEXT("Ball"), TEXT("Flag"), TEXT("GrassClose"), TEXT("GrassEdge"), TEXT("FlagWind10mph")};
            Filename = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/RangeAssets") /
                                                         FString::Printf(TEXT("%s-%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")), Names[View]));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
            FScreenshotRequest::RequestScreenshot(Filename, true, false, false, FIntRect(), true);
            Stage = 7;
            return false;
        }
        if (Stage == 7)
        {
            if (!IFileManager::Get().FileExists(*Filename))
                return false;
            Test->AddInfo(FString::Printf(TEXT("Range visual capture: %s"), *Filename));
            if (!FParse::Param(FCommandLine::Get(), TEXT("RangeStaticOnly")) && SequenceFrame < 20 && (View == 0 || View == 1 || View == 4 || View == 5))
            {
                ++SequenceFrame;
                ReadyAt = GetCurrentRunTime() + 0.5;
                Stage = 8;
                return false;
            }
            SequenceFrame = 0;
            if (++View == 7)
            {
                IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindMph"))->Set(0.0f, ECVF_SetByConsole);
                IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindFromDeg"))->Set(0.0f, ECVF_SetByConsole);
                return true;
            }
            Stage = 4;
        }
        if (Stage == 8)
        {
            Filename = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/RangeAssets") /
                                                         FString::Printf(TEXT("%s-View%d-Frame%02d.png"), *SequenceId, View, SequenceFrame));
            FScreenshotRequest::RequestScreenshot(Filename, true, false, false, FIntRect(), true);
            Stage = 7;
        }
        return false;
    }

  private:
    FAutomationTestBase *Test;
    IConsoleVariable *GrassVar;
    int32 OriginalGrass = 1, InitialCount = 0, Stage = 0, View = 0, Frames = 0;
    bool bCapture;
    double ReadyAt = 0, SampleStart = 0;
    ACameraActor *Camera = nullptr;
    FString Filename;
    int32 SequenceFrame = 0;
    FString SequenceId = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangePresentationTest, "UOpenGolf.Range.Presentation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangeAssetsVisualTest, "UOpenGolf.Visual.RangeAssets",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

static bool QueuePresentationCheck(FAutomationTestBase *Test, bool bCapture)
{
    FString Map = TEXT("/Game/UOpenGolf/Maps/UOpenGolf");
    if (bCapture)
        FParse::Value(FCommandLine::Get(), TEXT("RangeVisualMap="), Map);
    if (!FEditorFileUtils::LoadMap(Map, false, true))
    {
        Test->AddError(TEXT("Could not load UOpenGolf map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCheckRangePresentation>(Test, bCapture));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

bool FRangePresentationTest::RunTest(const FString &)
{
    return QueuePresentationCheck(this, false);
}
bool FRangeAssetsVisualTest::RunTest(const FString &)
{
    return QueuePresentationCheck(this, true);
}
#endif
