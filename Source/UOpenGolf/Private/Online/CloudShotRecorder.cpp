// SPDX-License-Identifier: MIT
#include "Online/CloudShotRecorder.h"

namespace OpenGolfOnline
{
    void FCloudShotRecorder::Begin(const FLaunchMonitorShot& Shot, ELaunchMonitorClub Club, const FShotOwner& Owner, bool bRange, bool bRealDevice)
    {
        Release(); Pending.Reset();
        if (!Owner.bSave || Owner.Key.IsEmpty() || !bRange || !bRealDevice || ClubKey(Club).IsEmpty()) return;
        FPending Next;
        Next.Owner = Owner.Key; Next.Shot.Id = FGuid::NewGuid(); Next.Shot.SessionId = SessionId;
        Next.Shot.ShotAt = Shot.ReceivedUtc; Next.Shot.Club = Club; Next.Shot.Launch = Shot;
        Pending = MoveTemp(Next);
    }
    void FCloudShotRecorder::BindSimulation(int32 DeviceShot, int32 SimulationShot)
    {
        if (Pending && Pending->Shot.Launch.ShotNumber == DeviceShot) Pending->SimulationShot = SimulationShot;
    }
    void FCloudShotRecorder::OnClubData(const FLaunchMonitorClubData& Data)
    {
        if (!Pending || Pending->Shot.Launch.ShotNumber != Data.ShotNumber) return;
        Pending->Shot.ClubData = Data;
        if (Pending->bRest) Release();
    }
    void FCloudShotRecorder::OnRest(const FOpenFairwayShotResult& Result)
    {
        if (!Pending || Pending->SimulationShot != Result.ShotNumber || !Result.bAtRest || !Result.bLanded || Result.TerminationReason == EOpenFairwayShotTermination::SafetyStop) return;
        Pending->Shot.CarryYards = Result.CarryM / 0.9144;
        Pending->Shot.RollYards = Result.RollM / 0.9144;
        Pending->bRest = true;
        if (Pending->Shot.ClubData) Release();
    }
    void FCloudShotRecorder::Release()
    {
        if (!Pending || !Pending->bRest) return;
        auto Shot = EncodeShot(Pending->Shot);
        if (Shot && !Sink(Shot,Pending->Owner)) Deferred.Add(*Pending);
        Pending.Reset();
    }
    void FCloudShotRecorder::Cancel() { Release(); Pending.Reset(); }
    void FCloudShotRecorder::Retry()
    {
        Deferred.RemoveAll([this](const FPending& P){return Sink(EncodeShot(P.Shot),P.Owner);});
    }
}
