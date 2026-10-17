#include "pch.h"
#include "Shared.h"
#include "SDK/SDK/FortniteGame_parameters.hpp"

INIT_POSTLOAD(AFortPlayerControllerGameplay);

static void ServerVehicleModSeatRpc(UObject* Context, FFrame& Stack)
{
    FGameplayTag RequestType;
    UFortVehicleModComponent* ModInstance;
    int32 SeatIndex;

    Stack.StepCompiledIn(&RequestType);
    Stack.StepCompiledIn(&ModInstance);
    Stack.StepCompiledIn(&SeatIndex);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerGameplay*)Context;
    auto Vehicle = ModInstance ? ModInstance->GetOwner() : nullptr;

    if (!Vehicle || !PlayerController->Pawn)
        return;

    static auto GetPawnAtSeatFn = IFortVehicleInterface::StaticClass()->FindFunction("GetPawnAtSeat");

    Params::FortVehicleInterface_GetPawnAtSeat GetPawnAtSeatParams{};
    GetPawnAtSeatParams.SeatIndex = SeatIndex;
    Vehicle->ProcessEvent(GetPawnAtSeatFn, &GetPawnAtSeatParams);

    if (GetPawnAtSeatParams.ReturnValue != PlayerController->Pawn)
        return;

    static auto EquipTurret = FName(L"Tag.VehicleMod.ServerRpcType.EquipTurret");
    static auto UnequipTurret = FName(L"Tag.VehicleMod.ServerRpcType.UnequipTurret");

    if (RequestType.TagName == EquipTurret)
        ModInstance->AuthorityTryEquipModSeatWeapon(SeatIndex);
    else if (RequestType.TagName == UnequipTurret)
        ModInstance->AuthorityTryUnequipModSeatWeapon(SeatIndex);
}

void AFortPlayerControllerGameplay__PostLoad()
{
    ExecHook(AFortPlayerControllerGameplay::StaticClass()->FindFunction("ServerVehicleModSeatRpc"), ServerVehicleModSeatRpc);
}
