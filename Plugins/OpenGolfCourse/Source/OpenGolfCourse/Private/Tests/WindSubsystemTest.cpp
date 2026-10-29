// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenGolfBeaufort.h"
#include "OpenGolfWindSubsystem.h"
#include "Tests/AutomationCommon.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfWindSubsystemTest, "UOpenGolf.Course.WindSubsystem",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenGolfWindSubsystemTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Wrapper;
    if (!Wrapper.CreateTestWorld(EWorldType::Game))
    {
        Wrapper.ForwardErrorMessages(this);
        return false;
    }
    UWorld& World = *Wrapper.GetTestWorld();
    UOpenGolfWindSubsystem* Wind = World.GetSubsystem<UOpenGolfWindSubsystem>();
    if (!TestNotNull(TEXT("Game worlds have a wind subsystem"), Wind))
        return false;
    TestTrue(TEXT("A world nobody sets is calm"), Wind->GetWindMps().IsZero());

    // 10 m/s at 10 m from -Y; the flag centre (1.96 m) gets about 72% of it.
    World.TimeSeconds = 100.0;
    Wind->SetWind(FVector(0.0, 10.0, 0.0), 1.2f);
    TestTrue(TEXT("The reference wind is stored"), Wind->GetWindMps().Equals(FVector(0.0, 10.0, 0.0), 1e-5));
    TestEqual(TEXT("Wind at flag height follows the log profile"), Wind->GetWindAtHeightMps(1.9558f).Y, 7.191, 0.01);
    TestEqual(TEXT("Wind at grass height follows the log profile"), Wind->GetWindAtHeightMps(0.3f).Y, 3.964, 0.01);
    TestTrue(TEXT("No wind below the roughness length"), Wind->GetWindAtHeightMps(0.01f).IsZero());
    // Time * speed + offset stays put: 100 s * 0 m/s before, 100 s * 10 m/s - 1000 m after.
    TestEqual(TEXT("Gusts stay in place when the speed changes"), World.TimeSeconds * 10.0 + Wind->GetGustOffsetM(), 0.0, 1e-3);

    if (UMaterialParameterCollection* Collection = Wind->GetParameterCollection())
    {
        const UMaterialParameterCollectionInstance* Instance = World.GetParameterCollectionInstance(Collection);
        FLinearColor Value;
        TestTrue(TEXT("MPC_Wind has the wind vector"), Instance->GetVectorParameterValue(UOpenGolfWindSubsystem::WindParameterName, Value));
        TestTrue(TEXT("MPC_Wind carries the wind, speed and reference height"), Value.Equals(FLinearColor(0.0f, 10.0f, 10.0f, 10.0f), 1e-4f));
        TestTrue(TEXT("MPC_Wind has the air vector"), Instance->GetVectorParameterValue(UOpenGolfWindSubsystem::AirParameterName, Value));
        TestEqual(TEXT("MPC_Wind carries the roughness"), Value.R, 0.03f, 1e-5f);
        TestEqual(TEXT("MPC_Wind carries the gust offset"), Value.G, -1000.0f, 1e-2f);
        TestEqual(TEXT("MPC_Wind carries the air density"), Value.B, 1.2f, 1e-5f);
    }
    else
    {
        AddWarning(TEXT("MPC_Wind is not built yet; run ApplyVegetationWind.py. Only the wind state was checked."));
    }

    const float NaN = std::numeric_limits<float>::quiet_NaN();
    Wind->SetWind(FVector(NaN, 0.0, 0.0), NaN);
    TestTrue(TEXT("Non-finite wind is calm"), Wind->GetWindMps().IsZero());
    TestEqual(TEXT("Non-finite density is standard air"), Wind->GetAirDensityKgM3(), 1.225f);

    // The scale the overlay and the vegetation onsets read.
    TestEqual(TEXT("Calm is force 0"), OpenGolfBeaufort::ForceFor(0.0f).Number, 0);
    TestEqual(TEXT("10 mph is a gentle breeze"), OpenGolfBeaufort::ForceFor(4.47f).Number, 3);
    TestEqual(TEXT("A force starts at its WMO speed"), OpenGolfBeaufort::ForceFor(5.5f).Number, 4);
    TestEqual(TEXT("Non-finite speed is calm"), OpenGolfBeaufort::ForceFor(NaN).Number, 0);
    TestEqual(TEXT("Hurricane force tops the scale"), OpenGolfBeaufort::ForceFor(60.0f).Number, 12);
    return true;
}

#endif
