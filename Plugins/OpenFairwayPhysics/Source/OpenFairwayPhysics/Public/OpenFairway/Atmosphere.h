// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

namespace OpenFairway
{
    /** Air properties: the barometric formula, Tetens' vapour pressure for humid air and Sutherland's law. */
    namespace Atmosphere
    {
        /**
         * Air density in kg/m^3. RelativeHumidity is 0-1 (clamped; NaN is dry air). A station
         * pressure of 0 derives the pressure from altitude; otherwise it is used as measured.
         */
        OPENFAIRWAYPHYSICS_API float Density(float AltitudeMeters, float TemperatureCelsius, float RelativeHumidity = 0.0f, float StationPressurePa = 0.0f);

        /** Dynamic viscosity in kg/(m*s). */
        OPENFAIRWAYPHYSICS_API float DynamicViscosity(float TemperatureCelsius);

        inline float FahrenheitToCelsius(float TemperatureF)
        {
            return (TemperatureF - 32.0f) * 5.0f / 9.0f;
        }
    } // namespace Atmosphere
} // namespace OpenFairway
