#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Building/BuildingItemWeaponUpgradeActor.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"

INIT_MODULE(ABuildingItemWeaponUpgradeActor);

void ConsumeResource(AFortPlayerControllerAthena* PlayerController, const UFortItemDefinition* ResourceDefinition, int32 Amount)
{
    static auto RemoveInventoryItem = (bool (*)(void*, FGuid&, int32, bool, bool))(ImageBase + 0xA251E20);

    auto InventoryOwner = PlayerController->GetInterface(IFortInventoryOwnerInterface::StaticClass());

    if (!InventoryOwner || !ResourceDefinition || Amount <= 0)
        return;

    auto& ReplicatedEntries = ((AFortInventory*)PlayerController->WorldInventory.ObjectPointer)->Inventory.ReplicatedEntries;
    std::vector<std::pair<FGuid, int32>> Stacks;

    for (auto& Entry : ReplicatedEntries)
    {
        if (Entry.ItemDefinition == (UItemDefinitionBase*)ResourceDefinition && Entry.Count > 0)
            Stacks.push_back({ Entry.ItemGuid, Entry.Count });
    }

    for (auto& [ItemGuid, Count] : Stacks)
    {
        if (Amount <= 0)
            break;

        auto Taken = Count < Amount ? Count : Amount;
        RemoveInventoryItem(InventoryOwner, ItemGuid, Taken, false, false);
        Amount -= Taken;
    }
}

static void GrantOutput(ABuildingItemWeaponUpgradeActor* Bench, AFortPlayerControllerAthena* PlayerController, const UFortItemDefinition* UpgradedWeaponDefinition)
{
    static auto IsWeaponInDataTable = (FWeaponUpgradeItemRow* (*)(const UItemDefinitionBase*, EFortWeaponUpgradeDirection))(ImageBase + 0x9511C24);
    static auto GetUpgradeCost = (int32 (*)(const ABuildingItemWeaponUpgradeActor*, const EFortWeaponUpgradeCosts*))(ImageBase + 0x950FA64);

    if (!Bench || !PlayerController || !UpgradedWeaponDefinition || !PlayerController->WorldInventory.ObjectPointer)
        return;

    auto& PendingEntry = *(FFortItemEntry*)(__int64(Bench) + 0xD50);

    if (!*(bool*)(__int64(Bench) + 0xD50 + sizeof(FFortItemEntry)) || !PendingEntry.ItemDefinition)
        return;

    auto Row = IsWeaponInDataTable(PendingEntry.ItemDefinition, EFortWeaponUpgradeDirection::Vertical);
    if (!Row || Row->UpgradedWeaponDef != UpgradedWeaponDefinition)
        Row = IsWeaponInDataTable(PendingEntry.ItemDefinition, EFortWeaponUpgradeDirection::Horizontal);
    if (!Row || Row->UpgradedWeaponDef != UpgradedWeaponDefinition)
        return;

    auto WoodCost = GetUpgradeCost(Bench, &Row->WoodCost);
    auto MetalCost = GetUpgradeCost(Bench, &Row->MetalCost);
    auto BrickCost = GetUpgradeCost(Bench, &Row->BrickCost);

    if (WoodCost == -1 || MetalCost == -1 || BrickCost == -1)
        return;

    ConsumeResource(PlayerController, Bench->WoodItem, WoodCost);
    ConsumeResource(PlayerController, Bench->MetalItem, MetalCost);
    ConsumeResource(PlayerController, Bench->BrickItem, BrickCost);

    auto Inventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;
    auto ItemGuid = PendingEntry.ItemGuid;
    auto Pawn = PlayerController->MyFortPawn;
    auto CurrentWeapon = Pawn ? Pawn->CurrentWeapon : nullptr;
    auto bWasEquipped = CurrentWeapon && CurrentWeapon->ItemEntryGuid == ItemGuid;

    for (auto& Entry : Inventory->Inventory.ReplicatedEntries)
    {
        if (Entry.ItemGuid == ItemGuid)
        {
            Remove(Inventory, ItemGuid);
            break;
        }
    }

    auto Item = GiveItem(Inventory, UpgradedWeaponDefinition, 1, PendingEntry.LoadedAmmo, PendingEntry.Level, true, true, PendingEntry.PhantomReserveAmmo, PendingEntry.StateValues);

    if (Item && bWasEquipped)
    {
        PlayerController->ServerExecuteInventoryItem(Item->ItemEntry.ItemGuid);
        PlayerController->ClientEquipItem(Item->ItemEntry.ItemGuid, true);
    }
}

void ABuildingItemWeaponUpgradeActor__Init()
{
    Hook<ABuildingItemWeaponUpgradeActor>(uint32(0x194), GrantOutput);
}
