// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenFairwayPhysicalMaterial.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwaySurfaceResolutionTest, "UOpenGolf.Physics.SurfaceResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenFairwaySurfaceResolutionTest::RunTest(const FString& Parameters)
{
    AOpenFairwayBall* Ball = NewObject<AOpenFairwayBall>(GetTransientPackage());
    Ball->DefaultSurface = EOpenFairwaySurface::Firm;
    AActor* Owner = NewObject<AActor>(GetTransientPackage());
    UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(Owner);
    UOpenFairwayPhysicalMaterial* Rough = NewObject<UOpenFairwayPhysicalMaterial>(GetTransientPackage());
    Rough->Surface = EOpenFairwaySurface::Rough;
    UPhysicalMaterial* Plain = NewObject<UPhysicalMaterial>(GetTransientPackage());

    auto Check = [this](const TCHAR* What, EOpenFairwaySurface Actual, EOpenFairwaySurface Expected)
    {
        TestEqual(What, StaticEnum<EOpenFairwaySurface>()->GetNameStringByValue(static_cast<int64>(Actual)),
            StaticEnum<EOpenFairwaySurface>()->GetNameStringByValue(static_cast<int64>(Expected)));
    };

    Check(TEXT("An undeclared piece uses the default"), Ball->ResolveSurface(Piece), EOpenFairwaySurface::Firm);
    Check(TEXT("No component uses the default"), Ball->ResolveSurface(nullptr), EOpenFairwaySurface::Firm);
    Check(TEXT("A physical material declares a surface without a component"), Ball->ResolveSurface(nullptr, Rough), EOpenFairwaySurface::Rough);

    Owner->Tags.Add(AOpenFairwayBall::GetSurfaceTag(EOpenFairwaySurface::Sand));
    Check(TEXT("An actor tag declares the surface"), Ball->ResolveSurface(Piece), EOpenFairwaySurface::Sand);
    Check(TEXT("A hit's physical material beats the actor tag"), Ball->ResolveSurface(Piece, Rough), EOpenFairwaySurface::Rough);
    Check(TEXT("A plain physical material is ignored"), Ball->ResolveSurface(Piece, Plain), EOpenFairwaySurface::Sand);

    // Without a hit material, the component's own simple-collision material decides.
    Piece->SetPhysMaterialOverride(Rough);
    Check(TEXT("The component's physical material is used without a hit"), Ball->ResolveSurface(Piece), EOpenFairwaySurface::Rough);
    Check(TEXT("A plain hit material falls back to the component's"), Ball->ResolveSurface(Piece, Plain), EOpenFairwaySurface::Rough);

    Piece->ComponentTags.Add(AOpenFairwayBall::GetSurfaceTag(EOpenFairwaySurface::Green));
    Check(TEXT("A component tag overrides any physical material"), Ball->ResolveSurface(Piece, Rough), EOpenFairwaySurface::Green);
    return true;
}

#endif
