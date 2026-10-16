// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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
    FCheckRangePresentation(FAutomationTestBase* InTest, bool bInCapture)
        : Test(InTest), bCapture(bInCapture)
    {
        GrassVar = IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.Grass3D"));
        OriginalGrass = GrassVar->GetInt();
    }
    ~FCheckRangePresentation() { GrassVar->Set(OriginalGrass, ECVF_SetByConsole); }

    bool Update() override
    {
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
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
        auto* Range = Mode->GetRange();
        if (Stage == 0)
        {
            Test->TestNotNull(TEXT("Game ball presentation subclass"), Cast<AUOpenGolfBall>(Mode->GetBall()));
            const auto* Mesh = Mode->GetBall()->FindComponentByClass<UStaticMeshComponent>();
            Test->TestEqual(TEXT("Imported ball mesh"), Mesh->GetStaticMesh()->GetName(), FString(TEXT("SM_GolfBall")));
            Test->TestEqual(TEXT("Ball mesh diameter before actor scale"), Mesh->GetStaticMesh()->GetBounds().BoxExtent.GetMax() * 2, 100.0, 0.1);
            Test->TestEqual(TEXT("Ball visuals have no collision"), Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
            Test->TestEqual(TEXT("Ball world diameter in cm"), Mesh->Bounds.BoxExtent.GetMax() * 2, 4.267, 0.01);
            TArray<UStaticMeshComponent*> Parts;
            Range->GetComponents(Parts);
            for (auto* Part : Parts)
            {
                if (Part->GetFName() == TEXT("Flagstick"))
                {
                    const FBox Bounds = Part->GetStaticMesh()->GetBoundingBox();
                    Test->TestEqual(TEXT("Flag top above ground in cm"), Bounds.Max.Z, 213.36, 0.01);
                    Test->TestEqual(TEXT("Pole diameter in cm"), Bounds.GetSize().X, 1.27, 0.01);
                }
            }
            GrassVar->Set(1, ECVF_SetByConsole);
            ReadyAt = GetCurrentRunTime() + 0.5;
            Stage = 1;
            return false;
        }
        if (GetCurrentRunTime() < ReadyAt)
            return false;
        if (Stage == 1)
        {
            InitialCount = Range->GetGrassInstanceCount();
            Test->TestTrue(TEXT("Grass is instantiated"), InitialCount > 1000);
            int32 Outside = 0;
            TArray<UHierarchicalInstancedStaticMeshComponent*> GrassParts;
            Range->GetComponents(GrassParts);
            for (const auto* Part : GrassParts)
            {
                for (int32 Index = 0; Index < Part->GetInstanceCount(); ++Index)
                {
                    FTransform Instance;
                    Part->GetInstanceTransform(Index, Instance, false);
                    const FVector Location = Instance.GetLocation();
                    if (FMath::Abs(Location.Y) > AUOpenGolfRange::FairwayHalfWidthM * 100 || Location.X < -1000)
                        ++Outside;
                }
            }
            Test->TestEqual(TEXT("Grass stays on the fairway strip"), Outside, 0);
            TArray<UStaticMeshComponent*> Pieces;
            Range->GetComponents(Pieces);
            for (const auto* Piece : Pieces)
            {
                const FString PieceName = Piece->GetName();
                const bool bTurf = PieceName.StartsWith(TEXT("Fairway")) || PieceName.StartsWith(TEXT("Fringe")) || PieceName == TEXT("Green");
                const bool bRough = PieceName.StartsWith(TEXT("Rough")) || PieceName == TEXT("Backdrop");
                if (bTurf || bRough)
                {
                    Test->TestEqual(*FString::Printf(TEXT("Turf tag on %s"), *PieceName), Piece->ComponentHasTag(AUOpenGolfRange::TurfTag), bTurf);
                    Test->TestEqual(*FString::Printf(TEXT("Rough tag on %s"), *PieceName), Piece->ComponentHasTag(AUOpenGolfRange::RoughTag), bRough);
                }
                if (PieceName == TEXT("Backdrop"))
                    Test->TestEqual(TEXT("Backdrop has no collision"), Piece->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
            }
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
            auto* PC = Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController());
            PC->SetReplayPanelOpen(false);
            Camera = World->SpawnActor<ACameraActor>();
            Camera->GetCameraComponent()->SetFieldOfView(65);
            PC->SetViewTarget(Camera);
            Stage = 4;
        }
        if (Stage == 4)
        {
            const FVector Locations[] = {FVector(-600, 0, 220), FVector(-600, 0, 220),
                FVector(-18, -18, 10), FVector(AUOpenGolfRange::GreenDistanceM * 100 - 400, -200, 180),
                FVector(-330, -400, 15), FVector(1600, -200, 100)};
            const FVector Targets[] = {FVector(6000, 0, 0), FVector(6000, 0, 0),
                Mode->GetBall()->GetActorLocation(), FVector(AUOpenGolfRange::GreenDistanceM * 100, 0, 140),
                FVector(-200, -380, 0), FVector(2300, 0, 0)};
            Camera->SetActorLocation(Locations[View]);
            Camera->SetActorRotation((Targets[View] - Locations[View]).Rotation());
            GrassVar->Set(View == 1 ? 0 : 1, ECVF_SetByConsole);
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
            Test->AddInfo(FString::Printf(TEXT("View %d: mean editor frame %.3f ms over 119 intervals; grass clumps %d"),
                View, (FPlatformTime::Seconds() - SampleStart) * 1000 / 119, Range->GetGrassInstanceCount()));
            const TCHAR* Names[] = {TEXT("TeeGrassOn"), TEXT("TeeGrassOff"), TEXT("Ball"), TEXT("Flag"), TEXT("GrassClose"), TEXT("GrassEdge")};
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
            if (++View == 6)
                return true;
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
    FAutomationTestBase* Test;
    IConsoleVariable* GrassVar;
    int32 OriginalGrass = 1, InitialCount = 0, Stage = 0, View = 0, Frames = 0;
    bool bCapture;
    double ReadyAt = 0, SampleStart = 0;
    ACameraActor* Camera = nullptr;
    FString Filename;
    int32 SequenceFrame = 0;
    FString SequenceId = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangePresentationTest, "UOpenGolf.Range.Presentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangeAssetsVisualTest, "UOpenGolf.Visual.RangeAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

static bool QueuePresentationCheck(FAutomationTestBase* Test, bool bCapture)
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

bool FRangePresentationTest::RunTest(const FString&)
{
    return QueuePresentationCheck(this, false);
}
bool FRangeAssetsVisualTest::RunTest(const FString&)
{
    return QueuePresentationCheck(this, true);
}
#endif
