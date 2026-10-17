#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortAthenaVehicleSpawner.h"

INIT_MODULE(AFortAthenaVehicleSpawner);

static bool (*ShouldSelfSpawnVehicleOG)(AFortAthenaVehicleSpawner*);
static bool ShouldSelfSpawnVehicle(AFortAthenaVehicleSpawner* Spawner)
{
    if (ShouldSelfSpawnVehicleOG(Spawner))
        return true;

    auto World = UWorld::GetWorld();
    auto GameState = World ? (AFortGameStateAthena*)World->GameState : nullptr;

    if ((!GameState || !GameState->IsA(AFortGameStateAthena::StaticClass()) || !GameState->bPlaylistDataIsLoaded) &&
        std::find(MissedSelfSpawns.begin(), MissedSelfSpawns.end(), Spawner) == MissedSelfSpawns.end())
        MissedSelfSpawns.push_back(Spawner);

    return false;
}

void ReplayMissedSelfSpawns()
{
    if (MissedSelfSpawns.empty())
        return;

    TArray<AFortAthenaVehicleSpawner*> Spawners;
    GetAll<AFortAthenaVehicleSpawner>(Spawners);

    for (auto& Spawner : Spawners)
    {
        if (std::find(MissedSelfSpawns.begin(), MissedSelfSpawns.end(), Spawner) == MissedSelfSpawns.end())
            continue;

        if (ShouldSelfSpawnVehicleOG(Spawner))
            Spawner->SpawnVehicle();
    }

    MissedSelfSpawns.clear();
    Spawners.Free();
}

void AFortAthenaVehicleSpawner__Init()
{
    Hook(ImageBase + 0x9163780, ShouldSelfSpawnVehicle, ShouldSelfSpawnVehicleOG);
}
