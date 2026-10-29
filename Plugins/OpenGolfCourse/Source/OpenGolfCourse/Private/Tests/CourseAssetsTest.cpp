// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayPhysicalMaterial.h"
#include "OpenGolfCourse.h"
#include "OpenGolfFlagstickComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfCourseAssetsTest, "UOpenGolf.Course.Assets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenGolfCourseAssetsTest::RunTest(const FString& Parameters)
{
    // Every surface has a physical material, so a future green or bunker material only needs to point at one.
    const UEnum* Surfaces = StaticEnum<EOpenFairwaySurface>();
    for (int32 Index = 0; Index < Surfaces->NumEnums() - 1; ++Index)
    {
        const FString Name = Surfaces->GetNameStringByIndex(Index);
        const auto* Physical = LoadObject<UOpenFairwayPhysicalMaterial>(nullptr, *OpenGolfCourse::SurfacePhysicalMaterialPath(Name));
        if (TestNotNull(*FString::Printf(TEXT("PM_%s"), *Name), Physical))
            TestEqual(*FString::Printf(TEXT("PM_%s declares its surface"), *Name), static_cast<int64>(Physical->Surface), Surfaces->GetValueByIndex(Index));
    }

    // Assigning a surface material declares both the look and the physics.
    for (const auto& [Path, Surface] : {TPair<const TCHAR*, EOpenFairwaySurface>(OpenGolfCourse::FairwayMaterialPath, EOpenFairwaySurface::Fairway),
             TPair<const TCHAR*, EOpenFairwaySurface>(OpenGolfCourse::RoughMaterialPath, EOpenFairwaySurface::Rough),
             TPair<const TCHAR*, EOpenFairwaySurface>(OpenGolfCourse::GreenMaterialPath, EOpenFairwaySurface::Green)})
    {
        const UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Path);
        if (!TestNotNull(Path, Material))
            continue;
        const auto* Physical = Cast<UOpenFairwayPhysicalMaterial>(Material->GetPhysicalMaterial());
        TestTrue(*FString::Printf(TEXT("%s plays as %s"), *Material->GetName(), *Surfaces->GetNameStringByValue(static_cast<int64>(Surface))),
            Physical && Physical->Surface == Surface);
        TestEqual(*FString::Printf(TEXT("%s shares the turf shader"), *Material->GetName()),
            Material->GetMaterial() ? Material->GetMaterial()->GetName() : FString(), FString(TEXT("M_Turf")));
    }

    const UOpenGolfFlagstickComponent* Flagstick = GetDefault<UOpenGolfFlagstickComponent>();
    const UStaticMesh* FlagMesh = Flagstick->GetStaticMesh();
    if (TestNotNull(TEXT("Flagstick mesh"), FlagMesh))
    {
        TestEqual(TEXT("Flagstick top in cm"), FlagMesh->GetBoundingBox().Max.Z, static_cast<double>(UOpenGolfFlagstickComponent::PoleHeightCm), 0.01);
        // The mesh is the pole alone; the flag is simulated cloth.
        TestEqual(TEXT("Flagstick mesh is the pole only"), FlagMesh->GetStaticMaterials().Num(), 1);
        TestEqual(TEXT("Pole diameter in cm"), FlagMesh->GetBoundingBox().GetSize().X, static_cast<double>(UOpenGolfFlagstickComponent::PoleDiameterCm), 0.01);
    }
    const UMaterialInterface* Cloth = Flagstick->FlagMaterial;
    const UMaterial* ClothBase = Cloth ? Cloth->GetMaterial() : nullptr;
    if (TestNotNull(TEXT("Flag cloth material"), ClothBase))
    {
        TestTrue(TEXT("Flag cloth is two-sided"), ClothBase->IsTwoSided());
        TestFalse(TEXT("The cloth simulation moves the flag, not its material"), ClothBase->HasVertexPositionOffsetConnected());
        FLinearColor Color;
        TestTrue(TEXT("Flag color parameter"), Cloth->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("FlagColor")), Color));
    }
    TestEqual(TEXT("Flagstick has no collision"), Flagstick->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

    // Putting greens are not striped.
    float GreenStripes = -1.0f;
    const UMaterialInterface* Green = LoadObject<UMaterialInterface>(nullptr, OpenGolfCourse::GreenMaterialPath);
    TestTrue(TEXT("Green has no mowing stripes"), Green && Green->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("StripeContrast")), GreenStripes) && GreenStripes == 0.0f);

    const UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, OpenGolfCourse::GolfBallMeshPath);
    const UMaterialInterface* BallMaterial = Ball ? Ball->GetMaterial(0) : nullptr;
    if (TestNotNull(TEXT("Golf ball material"), BallMaterial))
    {
        FLinearColor Color;
        for (const TCHAR* Parameter : {TEXT("BallColor"), TEXT("LineColor"), TEXT("DotColor")})
            TestTrue(*FString::Printf(TEXT("Ball marking parameter %s"), Parameter), BallMaterial->GetVectorParameterValue(FHashedMaterialParameterInfo(Parameter), Color));
    }
    return true;
}

#endif
