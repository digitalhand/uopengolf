// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameViewportClient.h"
#include "StaticMeshResources.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "OpenGolfCourse.h"
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

    /** Mowing bands split the fairway into an odd count, so one is centered on the target line and the edges fall on band boundaries. */
    void CheckStripeBands(const UMaterialInterface *Fairway, const AUOpenGolfRange *Range)
    {
        float BandCm = 0.0f;
        if (!Test->TestTrue(TEXT("Fairway has a stripe width"), Fairway && Fairway->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("StripeWidthCm")), BandCm) && BandCm > 0.0f))
            return;
        const double Bands = AUOpenGolfRange::FairwayHalfWidthM * 200.0 / BandCm;
        const int64 Rounded = FMath::RoundToInt64(Bands);
        Test->TestEqual(TEXT("Whole mowing bands across the fairway"), Bands, static_cast<double>(Rounded), 1e-3);
        Test->TestTrue(TEXT("Odd band count centers a band on the target line"), Rounded % 2 == 1);
        TArray<UHierarchicalInstancedStaticMeshComponent *> GrassParts;
        Range->GetLayoutComponents(GrassParts);
        float BladeBandCm = 0.0f;
        const UMaterialInterface *Blades = GrassParts.IsEmpty() ? nullptr : GrassParts[0]->GetMaterial(0);
        if (Blades && Blades->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("StripeWidthCm")), BladeBandCm))
            Test->TestEqual(TEXT("Grass blades stripe with the turf"), BladeBandCm, BandCm, 1e-3f);
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
                if (Part->GetFName() == TEXT("PuttingGreen"))
                {
                    const FVector Hole = Range->GetFlagstick()->GetComponentLocation();
                    Test->TestTrue(TEXT("Putting green is centered on the flag"), Part->GetComponentLocation().Equals(Hole, 0.01));
                    Test->TestEqual(TEXT("Putting green radius in cm"), Part->Bounds.BoxExtent.X, static_cast<double>(OpenGolfCourse::GreenDiscRadiusCm), 0.5);
                    Test->TestTrue(TEXT("Putting green collides"), Part->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
                    Test->TestFalse(TEXT("Putting green casts no shadow onto the turf under it"), Part->CastShadow);
                    Test->TestTrue(TEXT("Putting green tag"), Part->ComponentHasTag(AUOpenGolfRange::GreenTag) && !Part->ComponentHasTag(AUOpenGolfRange::TurfTag));
                    const UMaterialInterface *GreenSurface = Part->GetMaterial(0);
                    const UMaterialInterface *GreenKit = GreenSurface && GreenSurface->IsA<UMaterialInstanceDynamic>()
                                                             ? Cast<UMaterialInstanceDynamic>(GreenSurface)->Parent.Get()
                                                             : GreenSurface;
                    Test->TestEqual(TEXT("Putting green surface material"), GreenKit ? GreenKit->GetName() : FString(), FString(TEXT("MI_Green")));
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
            Test->TestTrue(TEXT("Ball wind reaches the flag"),
                           Range->GetFlagstick()->GetCloth().GetWindMps().Equals(Mode->GetBall()->GetWindVelocityMps(), 1e-3));
            Test->TestEqual(TEXT("Green stimp console variable reaches the ball"), Mode->GetBall()->GreenStimpFeet, 13.0f, 1e-4f);
            IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindMph"))->Set(0.0f, ECVF_SetByConsole);
            IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.WindFromDeg"))->Set(0.0f, ECVF_SetByConsole);
            IConsoleManager::Get().FindConsoleVariable(TEXT("uopengolf.GreenStimp"))->Set(10.0f, ECVF_SetByConsole);
            InitialCount = Range->GetGrassInstanceCount();
            Test->TestTrue(TEXT("Grass is instantiated"), InitialCount > 1000);
            int32 Outside = 0;
            TArray<UHierarchicalInstancedStaticMeshComponent *> GrassParts;
            Range->GetLayoutComponents(GrassParts);
            for (const auto *Part : GrassParts)
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
            for (const auto *Part : GrassParts)
            {
                // Public blades are ~1 cm wide and end before they fall below two pixels from the tee camera.
                const UStaticMesh *Clump = Part->GetStaticMesh();
                if (!Clump || Clump->GetName() != TEXT("SM_GrassClump"))
                    continue;
                Test->TestEqual(TEXT("Public clump blade count"), Clump->GetRenderData()->LODResources[0].GetNumTriangles(), 16);
                Test->TestTrue(TEXT("Public clump blades 2-3 cm tall"), Clump->GetBoundingBox().Max.Z > 2.2 && Clump->GetBoundingBox().Max.Z < 3.1);
                Test->TestEqual(TEXT("Public grass cull start in cm"), Part->InstanceStartCullDistance, 700);
                Test->TestEqual(TEXT("Public grass cull end in cm"), Part->InstanceEndCullDistance, 1000);
                const UMaterial *Material = Part->GetMaterial(0) ? Part->GetMaterial(0)->GetMaterial() : nullptr;
                if (Test->TestNotNull(TEXT("Public grass material"), Material))
                {
                    Test->TestTrue(TEXT("Public grass material supports instances"), Material->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));
                    Test->TestFalse(TEXT("Public grass normals use world space"), Material->bTangentSpaceNormal != 0);
                }
            }
            TArray<UStaticMeshComponent *> Pieces;
            Range->GetLayoutComponents(Pieces);
            for (const auto *Piece : Pieces)
            {
                const FString PieceName = Piece->GetName();
                const bool bTurf = PieceName.StartsWith(TEXT("Fairway")) || PieceName.StartsWith(TEXT("Fringe")) || PieceName == TEXT("Green");
                const bool bRough = PieceName.StartsWith(TEXT("Rough")) || PieceName == TEXT("Backdrop");
                if (bTurf || bRough)
                {
                    Test->TestEqual(*FString::Printf(TEXT("Turf tag on %s"), *PieceName), Piece->ComponentHasTag(AUOpenGolfRange::TurfTag), bTurf);
                    Test->TestEqual(*FString::Printf(TEXT("Rough tag on %s"), *PieceName), Piece->ComponentHasTag(AUOpenGolfRange::RoughTag), bRough);
                    // The public range has no material overrides, so each piece shows its kit surface.
                    const UMaterialInterface *Surface = Piece->GetMaterial(0);
                    Test->TestEqual(*FString::Printf(TEXT("Surface material on %s"), *PieceName), Surface ? Surface->GetName() : FString(),
                                    FString(bTurf ? TEXT("MI_Fairway") : TEXT("MI_Rough")));
                    Test->TestEqual(*FString::Printf(TEXT("Turf parent on %s"), *PieceName),
                                    Surface && Surface->GetMaterial() ? Surface->GetMaterial()->GetName() : FString(), FString(TEXT("M_Turf")));
                    if (PieceName == TEXT("FairwayFront"))
                        CheckStripeBands(Surface, Range);
                }
                if (PieceName == TEXT("Backdrop"))
                    Test->TestEqual(TEXT("Backdrop has no collision"), Piece->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
                if (PieceName == TEXT("TeeBox"))
                {
                    // Textured turf in the tee color (sRGB #406B1F), not a flat untextured slab.
                    const UMaterialInterface *TeeMaterial = Piece->GetMaterial(0);
                    const UMaterial *TeeBase = TeeMaterial ? TeeMaterial->GetMaterial() : nullptr;
                    Test->TestEqual(TEXT("Tee uses the turf material"), TeeBase ? TeeBase->GetName() : FString(), FString(TEXT("M_Turf")));
                    const FLinearColor TeeColor = FLinearColor::FromSRGBColor(FColor(0x40, 0x6B, 0x1F));
                    for (const TCHAR *Parameter : {TEXT("GrassDarkColor"), TEXT("GrassLightColor")})
                    {
                        FLinearColor Color;
                        const bool bFound = TeeMaterial && TeeMaterial->GetVectorParameterValue(FHashedMaterialParameterInfo(Parameter), Color);
                        Test->TestTrue(*FString::Printf(TEXT("Tee %s is the tee color"), Parameter), bFound && Color.Equals(TeeColor, 1e-4f));
                    }
                }
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
            Test->AddInfo(FString::Printf(TEXT("View %d: mean editor frame %.3f ms over 119 intervals; grass clumps %d"),
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
