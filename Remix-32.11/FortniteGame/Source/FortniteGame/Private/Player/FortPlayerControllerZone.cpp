#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortPlayerControllerAthena.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"

INIT_POSTLOAD(AFortPlayerControllerZone);

static void (*ClientOnPawnDiedOG)(AFortPlayerControllerAthena*, FFortPlayerDeathReport&);

static void (*ServerRequestSeatChangeOG)(UObject*, FFrame&, void*);

static void ClientOnPawnDied(AFortPlayerControllerAthena* PlayerController, FFortPlayerDeathReport& DeathReport)
{
    if (!PlayerController)
        return ClientOnPawnDiedOG(PlayerController, DeathReport);

    auto WorldInventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;
    auto Pawn = (AFortPlayerPawnAthena*)PlayerController->Pawn;

    if (WorldInventory && Pawn && Pawn->bShouldDropItemsOnDeath)
    {
        TArray<UFortWorldItem*> ItemsToDrop;

        for (auto Item : WorldInventory->Inventory.ItemInstances)
        {
            if (CanBeDropped((UFortItemDefinition*)Item->ItemEntry.ItemDefinition))
                ItemsToDrop.Add(Item);
        }

        DropItemsAsPickups(ItemsToDrop, Pawn, WorldInventory, nullptr, nullptr);
        ItemsToDrop.Free();
    }

    ReportDeath(PlayerController, (AFortPlayerStateAthena*)PlayerController->PlayerState, Pawn, (AFortPlayerStateAthena*)DeathReport.KillerPlayerState, (AFortPlayerPawnAthena*)DeathReport.KillerPawn.Get(), DeathReport.Tags);

    ClientOnPawnDiedOG(PlayerController, DeathReport);
}

static void ServerSpectateActor(AFortPlayerControllerAthena* PlayerController, AActor* NewViewTarget, bool bAllowStateChange)
{
    static auto SpectatingName = FName(L"Spectating");

    if (!PlayerController || (!bAllowStateChange && PlayerController->StateName != SpectatingName))
        return;

    auto PlayerState = (AFortPlayerStateZone*)PlayerController->PlayerState;

    if (!PlayerState || !PlayerState->IsA(AFortPlayerStateZone::StaticClass()) || !NewViewTarget)
        return;

    auto TargetPawnPlayerState = NewViewTarget->IsA(APawn::StaticClass()) ? ((APawn*)NewViewTarget)->PlayerState : nullptr;
    auto TargetPlayerState = TargetPawnPlayerState && TargetPawnPlayerState->IsA(AFortPlayerStateZone::StaticClass()) ? (AFortPlayerStateZone*)TargetPawnPlayerState : nullptr;

    if (TargetPlayerState && UFortRuntimeOptions::GetDefaultObj()->bEnableSpectatorUpdates)
    {
        static auto BeginSpectating = (void (*)(AFortPlayerStateZone*, AFortPlayerStateZone*))(ImageBase + 0xA2C9730);
        BeginSpectating(PlayerState, TargetPlayerState);
    }

    ((void (*)(AFortPlayerControllerAthena*, FName))PlayerController->VTable[0xED])(PlayerController, SpectatingName);
    PlayerController->ClientGotoState(SpectatingName);

    FViewTargetTransitionParams TransitionParams{};
    TransitionParams.BlendTime = 0.f;
    TransitionParams.BlendFunction = EViewTargetBlendFunction::VTBlend_Cubic;
    TransitionParams.BlendExp = 2.f;
    TransitionParams.bLockOutgoing = false;

    PlayerController->SetViewTargetWithBlend(NewViewTarget, TransitionParams.BlendTime, TransitionParams.BlendFunction, TransitionParams.BlendExp, false);
    PlayerController->ClientSetViewTarget(NewViewTarget, TransitionParams);
}

static void ServerRequestSeatChange(UObject* Context, FFrame& Stack)
{
    int32 TargetSeatIndex;

    Stack.StepCompiledIn(&TargetSeatIndex);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;
    auto Pawn = (AFortPlayerPawnAthena*)PlayerController->Pawn;
    auto Vehicle = Pawn ? Pawn->GetVehicleActor() : nullptr;

    if (!Vehicle && Pawn && Pawn->IsA(AFortCharacterVehicle::StaticClass()))
        Vehicle = Pawn;

    auto SeatWeaponComponent = Vehicle ? (UFortVehicleSeatWeaponComponent*)Vehicle->GetComponentByClass(UFortVehicleSeatWeaponComponent::StaticClass()) : nullptr;

    if (!SeatWeaponComponent)
        return callOG(PlayerController, Stack.CurrentNativeFunction, ServerRequestSeatChange, TargetSeatIndex);

    auto SeatComponent = (UFortVehicleSeatComponent*)Vehicle->GetComponentByClass(UFortVehicleSeatComponent::StaticClass());
    int32 SeatIndex = -1;
    UFortWeaponItemDefinition* OldWeapon = nullptr;

    for (int i = 0; i < SeatComponent->PlayerSlots.Num(); i++)
    {
        if (SeatComponent->PlayerSlots[i].Player == PlayerController->MyFortPawn)
        {
            SeatIndex = i;
            break;
        }
    }

    for (auto& WeaponDefinition : SeatWeaponComponent->WeaponSeatDefinitions)
    {
        if (WeaponDefinition.SeatIndex == SeatIndex)
        {
            OldWeapon = WeaponDefinition.VehicleWeapon;
            break;
        }
    }

    callOG(PlayerController, Stack.CurrentNativeFunction, ServerRequestSeatChange, TargetSeatIndex);

    if (!OldWeapon)
        return;

    if (auto LastItem = Pawn->PreviousWeapon)
    {
        PlayerController->ServerExecuteInventoryItem(LastItem->ItemEntryGuid);
        PlayerController->ClientEquipItem(LastItem->ItemEntryGuid, true);
        return;
    }

    for (auto& Entry : ((AFortInventory*)PlayerController->WorldInventory.ObjectPointer)->Inventory.ReplicatedEntries)
    {
        if (Entry.ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass()))
        {
            PlayerController->ServerExecuteInventoryItem(Entry.ItemGuid);
            PlayerController->ClientEquipItem(Entry.ItemGuid, true);
            break;
        }
    }
}

void AFortPlayerControllerZone__PostLoad()
{
    auto PlayerControllerClass = AFortPlayerControllerAthena::StaticClass();

    Hook(ImageBase + 0xA2936BC, ClientOnPawnDied, ClientOnPawnDiedOG);
    Hook<AFortPlayerControllerAthena>(uint32(0x48C), ServerSpectateActor);
    ExecHook(PlayerControllerClass->FindFunction("ServerRequestSeatChange"), ServerRequestSeatChange, ServerRequestSeatChangeOG);
}
