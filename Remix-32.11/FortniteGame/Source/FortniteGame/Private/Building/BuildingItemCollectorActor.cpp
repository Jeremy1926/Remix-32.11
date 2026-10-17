#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Building/BuildingItemCollectorActor.h"
#include "FortniteGame/Source/FortniteGame/Public/FortKismetLibrary.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortPickup.h"

INIT_POSTLOAD(ABuildingItemCollectorActor);

void VendWobble__FinishedFunc(UObject* Context, FFrame& Stack, void* Ret)
{
    auto CollectorActor = (ABuildingItemCollectorActor*)Context;
    auto PlayerController = CollectorActor->ControllingPlayer;

    if (!PlayerController)
        return VendWobble__FinishedFuncOG(Context, Stack, Ret);

    FCollectorUnitInfo* Collection = nullptr;

    for (auto& ItemCollection : CollectorActor->ItemCollections)
    {
        if (ItemCollection.InputItem == CollectorActor->ClientPausedActiveInputItem)
        {
            Collection = &ItemCollection;
            break;
        }
    }

    if (!Collection)
        return VendWobble__FinishedFuncOG(Context, Stack, Ret);

    CollectorActor->ClientPausedActiveInputItem = nullptr;

    auto VMLoc = CollectorActor->K2_GetActorLocation();
    auto& SpawnLocation = CollectorActor->LootSpawnLocation;
    auto Loc = VMLoc + (CollectorActor->GetActorForwardVector() * SpawnLocation.X) + (CollectorActor->GetActorRightVector() * SpawnLocation.Y) + (CollectorActor->GetActorUpVector() * SpawnLocation.Z);

    for (auto& Item : Collection->OutputItemEntry)
    {
        SpawnPickup(Loc, Item);
        CollectorActor->PickupSpawned.Process();
    }

    return VendWobble__FinishedFuncOG(Context, Stack, Ret);
}

std::unordered_map<int, float> VendingRarityWeights;
float VendingRarityWeightSum = 0;
float VendingRarityZeroWeight;
float VendingTotalWeight;

void FillVendingMachine(ABuildingItemCollectorActor* CollectorActor)
{
    auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
    auto MapInfo = ((AFortGameStateAthena*)UWorld::GetWorld()->GameState)->MapInfo;

    if (!GameMode || !*(bool*)(__int64(&GameMode->OnPlaylistLootTablesAppliedDelegate) + 0x10) || !MapInfo || !CollectorActor->IsA(MapInfo->VendingMachineClass) || CollectorActor->StartingGoalLevel != 0)
        return;

    if (VendingRarityWeightSum <= VendingRarityZeroWeight)
    {
        CollectorActor->K2_DestroyActor();
        return;
    }

    int Rarity = 0;
    bool bFilled = false;

    while (!bFilled)
    {
        bool bFound = false;

        while (!bFound)
        {
            auto RandomNum = (float)rand() / (RAND_MAX / VendingTotalWeight);

            for (auto& [ElementRarity, ElementWeight] : VendingRarityWeights)
            {
                if (ElementWeight == 0)
                    continue;

                if (RandomNum <= ElementWeight)
                {
                    Rarity = ElementRarity;
                    bFound = true;
                    break;
                }

                RandomNum -= ElementWeight;
            }
        }

        if (Rarity == 0)
            break;

        bFilled = true;
        int AttemptsToGetItem = 0;

        for (int i = 0; i < CollectorActor->ItemCollections.Num(); i++)
        {
            if (AttemptsToGetItem > 10)
            {
                bFilled = false;
                break;
            }

            auto& Collection = CollectorActor->ItemCollections[i];

            if (Collection.bUseDefinedOutputItem)
                continue;

            TArray<FFortItemEntry*> LootDrops{};

            ChooseLootForContainer(LootDrops, CollectorActor->DefaultItemLootTierGroupName, Rarity);

            if (Collection.OutputItemEntry.Num() > 0)
            {
                Collection.OutputItemEntry.Clear();
                Collection.OutputItem = nullptr;
            }

            for (auto& LootDrop : LootDrops)
            {
                if (!Collection.OutputItem && IsPrimaryQuickbar((UFortItemDefinition*)LootDrop->ItemDefinition))
                    Collection.OutputItem = (UFortWorldItemDefinition*)LootDrop->ItemDefinition;

                Collection.OutputItemEntry.Add(*LootDrop);
                free(LootDrop);
            }

            if (!Collection.OutputItem)
            {
                i--;
                AttemptsToGetItem++;

                continue;
            }

            AttemptsToGetItem = 0;
        }
    }

    if (Rarity == 0)
    {
        CollectorActor->K2_DestroyActor();
        return;
    }

    CollectorActor->StartingGoalLevel = Rarity;
}

void ABuildingItemCollectorActor__PostLoad()
{
    Hook<ABuildingItemCollectorActor>(uint32(0x195), FillVendingMachine);
}
