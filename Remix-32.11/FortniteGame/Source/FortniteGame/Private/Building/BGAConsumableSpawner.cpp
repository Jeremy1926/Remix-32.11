#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Building/BGAConsumableSpawner.h"
#include "FortniteGame/Source/FortniteGame/Public/FortKismetLibrary.h"

INIT_TICKER(ABGAConsumableSpawner);

struct FPendingConsumableQuery
{
    TWeakObjectPtr<ABGAConsumableSpawner> Spawner;
    TWeakObjectPtr<UEnvQueryInstanceBlueprintWrapper> Query;
    std::vector<std::pair<UClass*, int32>> Consumables;
    int32 Total;
    double StartTime;
};

static std::vector<FPendingConsumableQuery> PendingQueries;

void WorldReady(ABGAConsumableSpawner* Spawner)
{
    if (!Spawner || Spawner->Role != ENetRole::ROLE_Authority)
        return;

    auto World = UWorld::GetWorld();

    if (!World || !World->AuthorityGameMode || !World->AuthorityGameMode->IsA(AFortGameModeAthena::StaticClass()) || !World->GameState)
        return;

    auto QueryTemplate = Spawner->QueryTemplate;

    if (!QueryTemplate)
        return;

    TArray<FFortItemEntry*> LootDrops{};
    ChooseLootForContainer(LootDrops, Spawner->SpawnLootTierGroup);

    std::vector<std::pair<UClass*, int32>> Consumables;
    int32 Total = 0;

    for (auto& LootDrop : LootDrops)
    {
        auto WrapperDefinition = LootDrop->ItemDefinition && LootDrop->ItemDefinition->IsA(UBGAConsumableWrapperItemDefinition::StaticClass()) ? (UBGAConsumableWrapperItemDefinition*)LootDrop->ItemDefinition : nullptr;
        auto ConsumableClass = WrapperDefinition ? WrapperDefinition->ConsumableClass.Get() : nullptr;

        if (ConsumableClass)
        {
            auto Min = (int32)WrapperDefinition->MinNumberConsumablesToSpawn.Evaluate();
            auto Max = (int32)WrapperDefinition->MaxNumberConsumablesToSpawn.Evaluate();
            int32 Count = 0;

            for (int32 i = 0; i < LootDrop->Count; i++)
                Count += Max > Min ? Min + rand() % (Max - Min + 1) : Min;

            if (Count > 0)
            {
                Consumables.push_back({ ConsumableClass, Count });
                Total += Count;
            }
        }

        free(LootDrop);
    }

    LootDrops.Free();

    if (Total <= 0)
        return;

    auto Query = UEnvQueryManager::RunEQSQuery(Spawner, QueryTemplate, Spawner, EEnvQueryRunMode::AllMatching, UEnvQueryInstanceBlueprintWrapper::StaticClass());

    if (!Query)
        return;

    for (auto& Param : Spawner->QueryConfig)
        Query->SetNamedParam(Param.ParamName, Param.Value);

    PendingQueries.push_back({ TWeakObjectPtr<ABGAConsumableSpawner>(Spawner), TWeakObjectPtr<UEnvQueryInstanceBlueprintWrapper>(Query), std::move(Consumables), Total, UGameplayStatics::GetTimeSeconds(World) });
}

void ABGAConsumableSpawner__Tick()
{
    if (PendingQueries.empty())
        return;

    auto World = UWorld::GetWorld();

    if (!World)
        return;

    auto Time = UGameplayStatics::GetTimeSeconds(World);

    for (size_t i = 0; i < PendingQueries.size();)
    {
        auto& Pending = PendingQueries[i];
        auto Spawner = Pending.Spawner.Get();
        auto Query = Pending.Query.Get();

        if (Spawner && Query && !Query->ItemType && Time - Pending.StartTime < 30.)
        {
            i++;
            continue;
        }

        if (Spawner && Query && Query->ItemType)
        {
            auto Locations = Query->GetResultsAsLocations();
            int32 LocationIndex = 0;

            for (auto& [ConsumableClass, Count] : Pending.Consumables)
            {
                auto Consumable = (ABuildingGameplayActorConsumable*)ConsumableClass->ClassDefaultObject;
                auto bRandomRotation = Consumable && Consumable->IsA(ABuildingGameplayActorConsumable::StaticClass()) && Consumable->bSpawnerCalculateRandomRotation;

                for (int32 j = 0; j < Count && LocationIndex < Locations.Num(); j++)
                {
                    FRotator Rotation{};

                    if (bRandomRotation)
                        Rotation.Yaw = ((float)rand() / 32767.f) * 360.f;

                    SpawnActor(ConsumableClass, Locations[LocationIndex++], Rotation);
                }
            }

            Locations.Free();
        }

        PendingQueries.erase(PendingQueries.begin() + i);
    }
}
