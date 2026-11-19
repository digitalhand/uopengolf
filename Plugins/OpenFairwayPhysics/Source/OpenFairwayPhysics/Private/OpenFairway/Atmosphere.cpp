// SPDX-License-Identifier: MIT
// Originally ported from ogolf-physics core/Atmosphere.cs.
#include "OpenFairway/Atmosphere.h"

namespace OpenFairway
{
    namespace AtmosphereConstants
    {
        constexpr float KelvinOffset = 273.15f;
        constexpr float SutherlandKelvin = 110.4f;
        constexpr float SeaLevelPressurePa = 101325.0f;
        constexpr float StandardGravity = 9.80665f;
        constexpr float DryAirGasConstant = 287.058f;
        constexpr float MolarMassDryAir = 0.0289644f;
        constexpr float UniversalGasConstant = 8.314462618f;
        constexpr float MolarMassWater = 0.018016f;
        constexpr float TetensSaturationPa = 610.78f;
    } // namespace AtmosphereConstants

    float Atmosphere::Density(float AltitudeMeters, float TemperatureCelsius, float RelativeHumidity, float StationPressurePa)
    {
        using namespace AtmosphereConstants;
        const float TemperatureKelvin = FMath::Max(TemperatureCelsius + KelvinOffset, 1.0f);
        const float Humidity = FMath::IsNaN(RelativeHumidity) ? 0.0f : FMath::Clamp(RelativeHumidity, 0.0f, 1.0f);
        const float Pressure = StationPressurePa > 0.0f
                                   ? StationPressurePa
                                   : SeaLevelPressurePa * FMath::Exp(-StandardGravity * MolarMassDryAir * AltitudeMeters / (UniversalGasConstant * TemperatureKelvin));
        if (Humidity <= 0.0f)
        {
            return Pressure / (DryAirGasConstant * TemperatureKelvin);
        }
        // Tetens saturation pressure; the dry air contributes only its partial pressure.
        const float VapourPressure = Humidity * TetensSaturationPa * FMath::Pow(10.0f, 7.5f * TemperatureCelsius / (TemperatureCelsius + 237.3f));
        const float DryPressure = FMath::Max(Pressure - VapourPressure, 0.0f);
        return (DryPressure * MolarMassDryAir + VapourPressure * MolarMassWater) / (UniversalGasConstant * TemperatureKelvin);
    }

    float Atmosphere::DynamicViscosity(float TemperatureCelsius)
    {
        using namespace AtmosphereConstants;
        const float TemperatureKelvin = FMath::Max(TemperatureCelsius + KelvinOffset, 1.0f);
        return 1.716e-5f * FMath::Pow(TemperatureKelvin / KelvinOffset, 1.5f) * (KelvinOffset + SutherlandKelvin) /
               (TemperatureKelvin + SutherlandKelvin);
    }
} // namespace OpenFairway
