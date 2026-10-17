#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteAI/Public/AI/AthenaAISystem.h"

void CreateSubsystems(UAthenaAISystem* AISystem, UObject* GameMode)
{
    if (!AISystem->PerceptionManager)
        AISystem->PerceptionManager = (UAthenaAIPerceptionManager*)UGameplayStatics::SpawnObject(UAthenaAIPerceptionManager::StaticClass(), AISystem);

    if (!AISystem->AIDropper)
        AISystem->AIDropper = (UAthenaAIDropper*)UGameplayStatics::SpawnObject(UAthenaAIDropper::StaticClass(), AISystem);

    if (!AISystem->AIVehicleAvoidanceManager)
        AISystem->AIVehicleAvoidanceManager = (UAthenaAIVehicleAvoidanceManager*)UGameplayStatics::SpawnObject(UAthenaAIVehicleAvoidanceManager::StaticClass(), AISystem);

    if (AISystem->AIPopulationTracker)
        return;

    auto PopulationTracker = (UAthenaAIPopulationTracker*)UGameplayStatics::SpawnObject(UAthenaAIPopulationTracker::StaticClass(), AISystem);

    if (!PopulationTracker)
        return;

    PopulationTracker->CachedGameMode = (AFortGameModeAthena*)GameMode;

    if (AISystem->AISpawner)
        AISystem->AISpawner->OnPawnSpawnedEvent.Bind(PopulationTracker, FName(L"OnAISpawned"));

    AISystem->AIPopulationTracker = PopulationTracker;
}
