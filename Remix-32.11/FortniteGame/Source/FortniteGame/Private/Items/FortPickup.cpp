#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortPickup.h"
#include "FortniteGame/Source/FortniteAI/Public/Athena/AI/Systems/AthenaAIServiceLoot.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortAthenaSupplyDrop.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"

static void SetPickupItems(AFortPickupAthena* Pickup)
{
    static auto SetPickupItemsFn = (void (*)(AFortPickupAthena*, FFortItemEntry*, TArray<FFortItemEntry>*, uint8_t, bool, uint8_t))(ImageBase + 0x9CDB314);

    TArray<FFortItemEntry> MultiItemPickupEntries{};
    SetPickupItemsFn(Pickup, &Pickup->PrimaryPickupItemEntry, &MultiItemPickupEntries, (uint8_t)EFortPickupSourceTypeFlag::Container, false, (uint8_t)EFortPickupSpawnSource::Chest);
}

static void InitializePickup(AFortPickupAthena* Pickup, const FFortItemEntry& Entry, AFortPawn* Pawn, int OverrideCount)
{
    auto& PickupEntry = Pickup->PrimaryPickupItemEntry;

    PickupEntry.ItemDefinition = Entry.ItemDefinition;
    PickupEntry.LoadedAmmo = Entry.LoadedAmmo;
    PickupEntry.Count = OverrideCount != -1 ? OverrideCount : Entry.Count;
    PickupEntry.PhantomReserveAmmo = Entry.PhantomReserveAmmo;
    CopyWeaponModsFrom(PickupEntry, Entry);

    SetPickupItems(Pickup);
    Pickup->PawnWhoDroppedPickup = Pawn;
}

AFortPickupAthena* SpawnPickup(FVector Loc, FFortItemEntry& Entry, EFortPickupSourceTypeFlag SourceTypeFlag, EFortPickupSpawnSource SpawnSource, AFortPawn* Pawn, int OverrideCount, bool bToss, bool bRandomRotation,
                               bool bCombine, const UClass* OverrideClass, FVector FinalLoc)
{
    auto NewPickup = SpawnActor<AFortPickupAthena>(OverrideClass ? OverrideClass : AFortPickupAthena::StaticClass(), Loc, {});

    if (!NewPickup)
        return nullptr;

    NewPickup->bRandomRotation = bRandomRotation;

    if (Entry.Level != -1)
        NewPickup->PrimaryPickupItemEntry.Level = Entry.Level;

    InitializePickup(NewPickup, Entry, Pawn, OverrideCount);

    auto FinalLocation = FinalLoc.X || FinalLoc.Y || FinalLoc.Z ? FinalLoc : Loc;
    NewPickup->TossPickup(FinalLocation, Pawn, -1, bToss, true, SourceTypeFlag, SpawnSource);

    NewPickup->bTossedFromContainer = SpawnSource == EFortPickupSpawnSource::Chest || SpawnSource == EFortPickupSpawnSource::AmmoBox;

    if (NewPickup->bTossedFromContainer)
        NewPickup->OnRep_TossedFromContainer();

    RegisterPickupLoot(NewPickup);

    return NewPickup;
}

AFortPickupAthena* SpawnPickup(FVector Loc, const UFortItemDefinition* ItemDefinition, int Count, int LoadedAmmo, EFortPickupSourceTypeFlag SourceTypeFlag, EFortPickupSpawnSource SpawnSource, AFortPawn* Pawn, bool bToss,
                               bool bRandomRotation, const UClass* OverrideClass)
{
    auto ItemEntry = MakeItemEntry(ItemDefinition, Count, -1);

    if (LoadedAmmo != -1)
        ItemEntry->LoadedAmmo = LoadedAmmo;

    auto Pickup = SpawnPickup(Loc, *ItemEntry, SourceTypeFlag, SpawnSource, Pawn, -1, bToss, true, bRandomRotation, OverrideClass);
    free(ItemEntry);

    return Pickup;
}

AFortPickupAthena* SpawnPickup(ABuildingContainer* Container, FFortItemEntry& Entry, AFortPawn* Pawn, int OverrideCount)
{
    auto ContainerLoc = Container->K2_GetActorLocation();
    auto& SpawnLocation = Container->LootSpawnLocation_Athena;
    auto Loc = ContainerLoc + (Container->GetActorForwardVector() * SpawnLocation.X) + (Container->GetActorRightVector() * SpawnLocation.Y) + (Container->GetActorUpVector() * SpawnLocation.Z);
    auto NewPickup = SpawnActor<AFortPickupAthena>(Loc, {});

    if (!NewPickup)
        return nullptr;

    NewPickup->bRandomRotation = true;
    NewPickup->PrimaryPickupItemEntry.Level = Entry.Level;
    InitializePickup(NewPickup, Entry, Pawn, OverrideCount);

    UFortKismetLibrary::TossPickupFromContainer(UWorld::GetWorld(), Container, NewPickup, 10, (int32)std::clamp((float)rand() * 0.0003357036f, 0.f, 10.f), Container->LootTossConeHalfAngle_Athena,
                                                *(FRotator*)&Container->LootTossDirection_Athena, Container->LootTossSpeed_Athena, Container->bForceHidePickupMinimapIndicator);

    NewPickup->bTossedFromContainer = true;
    NewPickup->OnRep_TossedFromContainer();

    RegisterPickupLoot(NewPickup);

    return NewPickup;
}
