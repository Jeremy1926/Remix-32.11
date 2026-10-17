#pragma once
#include "pch.h"

UFortWorldItem* GiveItem(AFortInventory* Inventory, const UFortItemDefinition* ItemDefinition, int Count = 1, int LoadedAmmo = 0, int Level = 0, bool bNotifyPlayer = true, bool bUpdateInventory = true, int PhantomReserveAmmo = 0,
                         TArray<FFortItemEntryStateValue> StateValues = {}, const FFortItemEntry* SourceEntry = nullptr);
UFortWorldItem* GiveItem(AFortInventory* Inventory, FFortItemEntry& Entry, int Count = -1, bool bNotifyPlayer = true, bool bUpdateInventory = true);
void Update(AFortInventory* Inventory, FFortItemEntry* Entry);
void Remove(AFortInventory* Inventory, FGuid Guid);
UFortWorldItem* FindItemInstance(AFortInventory* Inventory, const FGuid& ItemGuid);
UFortWorldItem* FindItemInstance(AFortInventory* Inventory, const UFortItemDefinition* ItemDefinition);
FFortItemEntry* FindReplicatedEntry(AFortInventory* Inventory, const FGuid& ItemGuid);
FFortItemEntry* FindReplicatedEntry(AFortInventory* Inventory, const UFortItemDefinition* ItemDefinition);
void UpdateOrRemoveEntry(AFortInventory* Inventory, UFortWorldItem* Item, FFortItemEntry& Entry);
bool RemoveInventoryItem(AFortInventory* Inventory, FGuid& ItemGuid, int32 Count, bool bForceRemoval, bool bForcePersistWhenEmpty);
FFortItemEntry* MakeItemEntry(const UFortItemDefinition* ItemDefinition, int32 Count, int32 Level);
FFortRangedWeaponStats* GetStats(const UFortWeaponItemDefinition* WeaponDefinition);
bool IsPrimaryQuickbar(const UFortItemDefinition* ItemDefinition);
bool TakesQuickbarSlot(const UFortItemDefinition* ItemDefinition);
void UpdateEntry(AFortInventory* Inventory, FFortItemEntry& Entry);
int32 AddItemCount(AFortInventory* Inventory, const UFortItemDefinition* ItemDefinition, int32 Count);
void RemoveWeaponAbilities(AFortWeapon* Weapon);
void SetStateValueConstObject(FFortItemEntry& Entry, const FGameplayTag& StateType, const UObject* Object);
void CopyWeaponModsFrom(FFortItemEntry& Entry, const FFortItemEntry& Source);
bool CanBeDropped(const UFortItemDefinition* ItemDefinition);
void UpdateItemCount(AFortInventory* Inventory, UFortWorldItem* Item, FFortItemEntry& ItemEntry);
