#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteAI/Public/AI/FortAthenaAIBotController.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortPickup.h"
#include "FortniteGame/Source/FortniteGame/Public/Player/FortPlayerController.h"

INIT_MODULE(AFortPickupAthena);
INIT_POSTLOAD(AFortPickupAthena);

static void (*GivePickupToOG)(AFortPickupAthena*, void*, bool);
static void GivePickupTo(AFortPickupAthena* Pickup, void* InventoryOwner, bool bDestroyAfterPickup)
{
    auto Pawn = (AFortPlayerPawnAthena*)Pickup->PickupLocationData.PickupTarget.Get();
    auto Controller = Pawn ? (AFortAthenaAIBotController*)Pawn->Controller : nullptr;

    if (Pickup->Role == ENetRole::ROLE_Authority && Controller && Controller->IsA(AFortAthenaAIBotController::StaticClass()) && InventoryOwner == (void*)(uint64(Controller) + 0x3F0) && Controller->Inventory.ObjectPointer)
    {
        InternalPickup(Controller, &Pickup->PrimaryPickupItemEntry);
        RefreshSlotItems(Controller);
    }

    GivePickupToOG(Pickup, InventoryOwner, bDestroyAfterPickup);
}

static bool (*FinishedTargetSplineOG)(AFortPickupAthena*);
static bool FinishedTargetSpline(AFortPickupAthena* Pickup)
{
    auto Pawn = (AFortPlayerPawnAthena*)Pickup->PickupLocationData.PickupTarget.Get();

    if (!Pawn)
        return FinishedTargetSplineOG(Pickup);

    auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;

    if (!PlayerController || !PlayerController->IsA(AFortPlayerControllerAthena::StaticClass()))
        return FinishedTargetSplineOG(Pickup);

    if (!PlayerController->bTryPickupSwap)
    {
        InternalPickup(PlayerController, &Pickup->PrimaryPickupItemEntry);
        return FinishedTargetSplineOG(Pickup);
    }

    PlayerController->bTryPickupSwap = false;

    auto WorldInventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;
    auto FinalLoc = Pawn->K2_GetActorLocation();
    auto ForwardVector = Pawn->GetActorForwardVector();

    ForwardVector.Z = 0.0f;
    ForwardVector.Normalize();

    FinalLoc = FinalLoc + ForwardVector * 450.f;
    FinalLoc.Z += 50.f;

    auto FinalAngle = (((float)rand() * 0.00109866634f) - 18.f) * 0.017453292519943295f;

    FinalLoc.X += cos(FinalAngle) * 100.f;
    FinalLoc.Y += sin(FinalAngle) * 100.f;

    if (!Pawn->CurrentWeapon || !IsPrimaryQuickbar(Pawn->CurrentWeapon->WeaponData) || !IsPrimaryQuickbar((UFortItemDefinition*)Pickup->PrimaryPickupItemEntry.ItemDefinition))
    {
        SpawnPickup(PlayerController->GetViewTarget()->K2_GetActorLocation(), Pickup->PrimaryPickupItemEntry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, PlayerController->MyFortPawn, -1, true,
                    true, true, nullptr, FinalLoc);
        return FinishedTargetSplineOG(Pickup);
    }

    auto CurrentWeapon = ((AFortPlayerPawnAthena*)PlayerController->Pawn)->CurrentWeapon;
    auto Entry = FindReplicatedEntry(WorldInventory, CurrentWeapon->ItemEntryGuid);

    SpawnPickup(PlayerController->GetViewTarget()->K2_GetActorLocation(), *Entry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, PlayerController->MyFortPawn, -1, true, true, true, nullptr,
                FinalLoc);

    auto Item = GiveItem(WorldInventory, Pickup->PrimaryPickupItemEntry);
    auto& SlotItemGuids = PlayerController->PrimaryQuickBarSlotItemGuids;
    auto NumEnabledSlots = (std::min)(SlotItemGuids.NumEnabledSlots, (int32)std::size(SlotItemGuids.EquippedItemGuids));
    auto NumSlots = (int32)Item->GetNumberOfSlotsTaken();
    int32 Slot = 0;

    while (Slot < NumEnabledSlots && !(SlotItemGuids.EquippedItemGuids[Slot] == CurrentWeapon->ItemEntryGuid))
        Slot++;

    auto FirstSlot = Slot < NumEnabledSlots ? (std::min)(Slot, NumEnabledSlots - NumSlots) : -1;

    for (int32 i = FirstSlot; i >= 0 && i < FirstSlot + NumSlots; i++)
    {
        auto CoveredEntry = FindReplicatedEntry(WorldInventory, SlotItemGuids.EquippedItemGuids[i]);
        auto CoveredDefinition = CoveredEntry ? (UFortItemDefinition*)CoveredEntry->ItemDefinition : nullptr;

        if (!CoveredDefinition || CoveredEntry->ItemGuid == CurrentWeapon->ItemEntryGuid || !TakesQuickbarSlot(CoveredDefinition) || !CanBeDropped(CoveredDefinition))
            continue;

        SpawnPickup(PlayerController->GetViewTarget()->K2_GetActorLocation(), *CoveredEntry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, PlayerController->MyFortPawn, -1, true, true, true,
                    nullptr, FinalLoc);
        Remove(WorldInventory, CoveredEntry->ItemGuid);
    }

    Remove(WorldInventory, CurrentWeapon->ItemEntryGuid);
    PlayerController->ClientEquipItem(Item->ItemEntry.ItemGuid, true);

    return FinishedTargetSplineOG(Pickup);
}

void AFortPickupAthena__Init()
{
    Hook(ImageBase + 0x9CD6F20, GivePickupTo, GivePickupToOG);
}

void AFortPickupAthena__PostLoad()
{
    Hook(ImageBase + 0x2878094, FinishedTargetSpline, FinishedTargetSplineOG);
}
