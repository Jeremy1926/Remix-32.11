#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"

INIT_POSTLOAD(UFortWorldItem);

static AFortInventory* FindOwningInventory(UFortWorldItem* Item)
{
    if (auto Inventory = (AFortInventory*)Item->ItemEntry.ParentInventory.Get())
        return Inventory;

    auto Controller = Item->GetOwningController();

    return Controller && Controller->IsA(AFortPlayerControllerAthena::StaticClass()) ? (AFortInventory*)Controller->WorldInventory.ObjectPointer : nullptr;
}

static bool SetLoadedAmmo(UFortWorldItem* Item, int LoadedAmmo)
{
    if (!Item || Item->ItemEntry.LoadedAmmo == LoadedAmmo)
        return false;

    Item->ItemEntry.LoadedAmmo = LoadedAmmo;
    Item->ItemEntry.bIsDirty = true;

    auto Inventory = FindOwningInventory(Item);

    if (auto ReplicatedEntry = Inventory ? FindReplicatedEntry(Inventory, Item->ItemEntry.ItemGuid) : nullptr)
    {
        ReplicatedEntry->LoadedAmmo = LoadedAmmo;
        UpdateEntry(Inventory, *ReplicatedEntry);
    }

    return true;
}

static bool SetPhantomReserveAmmo(UFortWorldItem* Item, unsigned int PhantomReserveAmmo)
{
    if (!Item || Item->ItemEntry.PhantomReserveAmmo == (int32)PhantomReserveAmmo)
        return false;

    Item->ItemEntry.PhantomReserveAmmo = PhantomReserveAmmo;
    Item->ItemEntry.bIsDirty = true;

    auto Inventory = FindOwningInventory(Item);

    if (auto ReplicatedEntry = Inventory ? FindReplicatedEntry(Inventory, Item->ItemEntry.ItemGuid) : nullptr)
    {
        ReplicatedEntry->PhantomReserveAmmo = PhantomReserveAmmo;
        UpdateEntry(Inventory, *ReplicatedEntry);
    }

    return true;
}

void UFortWorldItem__PostLoad()
{
    Hook<UFortWorldItem>(uint32(0xC4), SetLoadedAmmo);
    Hook<UFortWorldItem>(uint32(0xC5), SetPhantomReserveAmmo);
}
