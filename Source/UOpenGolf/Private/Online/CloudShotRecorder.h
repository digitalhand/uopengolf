// SPDX-License-Identifier: MIT
#pragma once
#include "Online/OnlineContract.h"
#include "OpenFairwayTypes.h"

namespace OpenGolfOnline
{
    struct FShotOwner { FString Key; bool bSave = false; };
    class FCloudShotRecorder
    {
    public:
        using FSink = TFunction<bool(const TSharedPtr<FJsonObject>&, const FString&)>;
        explicit FCloudShotRecorder(FSink InSink) : Sink(MoveTemp(InSink)), SessionId(FGuid::NewGuid()) {}
        void Begin(const FLaunchMonitorShot& Shot, ELaunchMonitorClub Club, const FShotOwner& Owner, bool bRange, bool bRealDevice);
        void BindSimulation(int32 DeviceShot, int32 SimulationShot);
        void OnClubData(const FLaunchMonitorClubData& Data);
        void OnRest(const FOpenFairwayShotResult& Result);
        void Release();
        void Cancel();
        void Retry();
        void Clear() { Pending.Reset(); Deferred.Empty(); }
        int32 GetUnsavedCount() const { return Deferred.Num(); }
    private:
        struct FPending
        {
            FRangeShot Shot;
            FString Owner;
            int32 SimulationShot = INDEX_NONE;
            bool bRest = false;
        };
        TOptional<FPending> Pending;
        TArray<FPending> Deferred;
        FSink Sink;
        FGuid SessionId;
    };
}
