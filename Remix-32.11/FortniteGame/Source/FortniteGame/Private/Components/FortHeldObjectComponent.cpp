#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"

INIT_POSTLOAD(UFortHeldObjectComponent);

static void SetupOwningPawn(UFortHeldObjectComponent* HeldObjectComponent, AFortPlayerPawn* Pawn)
{
    auto HeldObject = HeldObjectComponent->GetOwner();

    if (!HeldObject)
        return;

    HeldObject->FlushNetDormancy();

    if (Pawn)
        HeldObjectComponent->HeldObjectState = EHeldObjectState::Held;

    auto PreviousPawn = HeldObjectComponent->OwningPawn;

    HeldObjectComponent->PreviousOwningPawn = !Pawn ? HeldObjectComponent->OwningPawn : nullptr;
    HeldObjectComponent->OwningPawn = Pawn;

    if (Pawn)
    {
        Pawn->HeldObject = HeldObject;
        ((AFortPlayerControllerAthena*)Pawn->Controller)->bHoldingObject = true;
    }

    HeldObject->ForceNetUpdate();
    HeldObjectComponent->OnHeldObjectOwningPawnChanged.Process();

    if (Pawn)
        Pawn->OnHeldObjectPickedUp.Process(HeldObject);
    else
        PreviousPawn->OnHeldObjectDropped.Process(HeldObject);
}

static void PickupHeldObject(UObject* Context, FFrame& Stack)
{
    AFortPlayerPawn* PlayerPawn;

    Stack.StepCompiledIn(&PlayerPawn);
    Stack.IncrementCode();

    auto HeldObjectComponent = (UFortHeldObjectComponent*)Context;
    auto PlayerController = (AFortPlayerControllerAthena*)PlayerPawn->Controller;

    SetupOwningPawn(HeldObjectComponent, PlayerPawn);

    if (!HeldObjectComponent->GrantedWeaponItem.Get())
    {
        auto ItemDefinition = HeldObjectComponent->EquippedWeaponItemDefinition.Get();
        auto Item = GiveItem((AFortInventory*)PlayerController->WorldInventory.ObjectPointer, ItemDefinition, 1, 99999);

        if (!Item)
            return;

        auto Weapon = PlayerPawn->EquipWeaponDefinition(ItemDefinition, Item->ItemEntry.ItemGuid, Item->ItemEntry.TrackerGuid, false);

        if (!Weapon)
            return;

        PlayerController->ClientEquipItem(Item->ItemEntry.ItemGuid, true);

        HeldObjectComponent->GrantedWeapon = Weapon;
        HeldObjectComponent->GrantedWeaponItem = Item;
    }

    HeldObjectComponent->OnHeldObjectPickedUp.Process();
}

static void DropHeldObject(UObject* Context, FFrame& Stack)
{
    Stack.IncrementCode();

    auto HeldObjectComponent = (UFortHeldObjectComponent*)Context;
    auto OwningPawn = HeldObjectComponent->OwningPawn;

    if (!OwningPawn)
        return;

    auto PlayerController = (AFortPlayerControllerAthena*)OwningPawn->Controller;

    HeldObjectComponent->HeldObjectState = EHeldObjectState::Dropped;

    for (auto& Entry : ((AFortInventory*)PlayerController->WorldInventory.ObjectPointer)->Inventory.ReplicatedEntries)
    {
        if (Entry.ItemGuid != OwningPawn->PreviousWeapon->ItemEntryGuid)
            continue;

        auto Weapon = OwningPawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)Entry.ItemDefinition, Entry.ItemGuid, Entry.TrackerGuid, false);
        PlayerController->ClientEquipItem(Weapon->ItemEntryGuid, true);
        break;
    }

    SetupOwningPawn(HeldObjectComponent, nullptr);
    HeldObjectComponent->OnHeldObjectDropped.Process();
}

void UFortHeldObjectComponent__PostLoad()
{
    auto HeldObjectComponentClass = UFortHeldObjectComponent::StaticClass();

    ExecHook(HeldObjectComponentClass->FindFunction("PickupHeldObject"), PickupHeldObject);
    ExecHook(HeldObjectComponentClass->FindFunction("DropHeldObject"), DropHeldObject);
}
