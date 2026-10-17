#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"

INIT_POSTLOAD(UFortVehicleSeatWeaponComponent);

static void GiveAndEquipSeatWeapon(UFortVehicleSeatWeaponComponent* Component, AFortPlayerPawn* FortPawn, FWeaponSeatDefinition* WeaponSeatDefinition)
{
    if (!FortPawn || !WeaponSeatDefinition)
        return;

    auto PlayerController = FortPawn->Controller && FortPawn->Controller->IsA(AFortPlayerControllerAthena::StaticClass()) ? (AFortPlayerControllerAthena*)FortPawn->Controller : nullptr;
    auto VehicleWeapon = WeaponSeatDefinition->VehicleWeaponOverride ? WeaponSeatDefinition->VehicleWeaponOverride : WeaponSeatDefinition->VehicleWeapon;
    auto WorldInventory = PlayerController ? (AFortInventory*)PlayerController->WorldInventory.ObjectPointer : nullptr;

    if (!WorldInventory || !PlayerController->MyFortPawn || !VehicleWeapon)
        return;

    auto Stats = GetStats(VehicleWeapon);
    auto VehicleItem = GiveItem(WorldInventory, VehicleWeapon, 1, Stats ? Stats->ClipSize : 0);
    auto Weapon = VehicleItem ? PlayerController->MyFortPawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)VehicleItem->ItemEntry.ItemDefinition, VehicleItem->ItemEntry.ItemGuid, VehicleItem->ItemEntry.TrackerGuid, false) : nullptr;

    if (!Weapon)
        return;

    Weapon->ForceNetUpdate();

    *(TWeakObjectPtr<UFortWorldItem>*)(__int64(WeaponSeatDefinition) + 0x34) = TWeakObjectPtr<UFortWorldItem>(VehicleItem);
    *(TWeakObjectPtr<AFortWeapon>*)(__int64(WeaponSeatDefinition) + 0x3C) = TWeakObjectPtr<AFortWeapon>(Weapon);

    Component->CachedWeapon = TWeakObjectPtr<AFortWeaponRanged>((AFortWeaponRanged*)Weapon);
    Component->CachedWeaponDef = (UFortWeaponRangedItemDefinition*)Weapon->WeaponData;
    Component->bWeaponEquipped = true;

    auto Vehicle = Component->GetVehicle();
    FMountedWeaponInfoRepped RepWeaponInfo{};

    RepWeaponInfo.HostVehicleCached.ObjectPointer = Vehicle;
    RepWeaponInfo.HostVehicleCached.InterfacePointer = Vehicle->GetInterface(IFortVehicleInterface::StaticClass());
    RepWeaponInfo.HostVehicleSeatIndexCached = WeaponSeatDefinition->SeatIndex;

    ((AFortWeaponRangedForVehicle*)Weapon)->MountedWeaponInfoRepped = RepWeaponInfo;
    ((AFortWeaponRangedForVehicle*)Weapon)->OnRep_MountedWeaponInfoRepped();

    WeaponSeatDefinition->LastEquippedVehicleWeapon = VehicleWeapon;
}

static void (*EquipVehicleWeaponOG)(UFortVehicleSeatWeaponComponent*, AFortPlayerPawn*, FWeaponSeatDefinition*, int32);
static void EquipVehicleWeapon(UFortVehicleSeatWeaponComponent* Component, AFortPlayerPawn* FortPawn, FWeaponSeatDefinition* WeaponSeatDefinition, int32 ItemLevel)
{
    GiveAndEquipSeatWeapon(Component, FortPawn, WeaponSeatDefinition);

    return EquipVehicleWeaponOG(Component, FortPawn, WeaponSeatDefinition, ItemLevel);
}

void UFortVehicleSeatWeaponComponent__PostLoad()
{
    Hook(ImageBase + 0xA610714, EquipVehicleWeapon, EquipVehicleWeaponOG);
}
