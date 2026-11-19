// SPDX-License-Identifier: MIT
#include "OpenFairway/GolfPhysics.h"
#include "OpenFairway/WindProfile.h"

#include "CoreMath.h"

namespace OpenFairway
{
    FBallState GolfPhysics::StepFlight(const FBallState& State, const FFlightEnvironment& Environment,
        const FFlightProfile& Profile, float Dt, const FVector3f& GroundNormal)
    {
        auto Derivative = [&](const FBallState& S)
        {
            const FVector3f AirVelocity = S.Velocity - WindProfile::At(Environment, Dot(S.Position, GroundNormal));
            const auto Aero = FlightAerodynamics::Sample(AirVelocity, S.Spin, Environment.AirDensity, Environment.AirViscosity, Profile);
            FVector3f Acceleration(0, -Gravity, 0);
            if (Aero.HasAerodynamics())
            {
                const float Q = 0.5f * Environment.AirDensity * CrossSection * Aero.Speed / Mass;
                Acceleration -= Q * Aero.DragCoefficient * AirVelocity;
                const float SpinLength = S.Spin.Size();
                if (SpinLength > 0.1f)
                {
                    Acceleration += Q * Aero.LiftCoefficient * Cross(S.Spin, AirVelocity) / SpinLength;
                }
            }
            return FBallState{S.Velocity, Acceleration, -Aero.SpinDecayRatePerSecond * S.Spin};
        };
        auto Advance = [](const FBallState& S, const FBallState& K, float H)
        {
            return FBallState{S.Position + H * K.Position, S.Velocity + H * K.Velocity, S.Spin + H * K.Spin};
        };
        const auto K1 = Derivative(State);
        const auto K2 = Derivative(Advance(State, K1, Dt * 0.5f));
        const auto K3 = Derivative(Advance(State, K2, Dt * 0.5f));
        const auto K4 = Derivative(Advance(State, K3, Dt));
        return {State.Position + (Dt / 6) * (K1.Position + 2 * K2.Position + 2 * K3.Position + K4.Position),
            State.Velocity + (Dt / 6) * (K1.Velocity + 2 * K2.Velocity + 2 * K3.Velocity + K4.Velocity),
            State.Spin + (Dt / 6) * (K1.Spin + 2 * K2.Spin + 2 * K3.Spin + K4.Spin)};
    }

    float GolfPhysics::FlightContactTime(const FBallState& State, const FFlightEnvironment& Environment,
        const FFlightProfile& Profile, float Dt, const FVector3f& Normal, float PlaneHeight)
    {
        float Low = 0, High = Dt;
        for (int32 Iteration = 0; Iteration < 20; ++Iteration)
        {
            const float Mid = (Low + High) * 0.5f;
            const auto AtMid = StepFlight(State, Environment, Profile, Mid);
            if (Dot(AtMid.Position, Normal) > PlaneHeight) Low = Mid;
            else High = Mid;
        }
        return High;
    }
} // namespace OpenFairway
