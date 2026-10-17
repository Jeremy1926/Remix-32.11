#include "pch.h"
#include "Shared.h"

INIT_POSTLOAD(UFortMissionLibrary);

static void TeleportPlayerPawn(UObject* Context, FFrame& Stack, bool* Ret)
{
    UObject* WorldContextObject;
    AFortPlayerPawn* PlayerPawn;
    FVector DestLocation;
    FRotator DestRotation;
    bool bIgnoreCollision;
    bool bIgnoreSupplementalKillVolumeSweep;

    Stack.StepCompiledIn(&WorldContextObject);
    Stack.StepCompiledIn(&PlayerPawn);
    Stack.StepCompiledIn(&DestLocation);
    Stack.StepCompiledIn(&DestRotation);
    Stack.StepCompiledIn(&bIgnoreCollision);
    Stack.StepCompiledIn(&bIgnoreSupplementalKillVolumeSweep);
    Stack.IncrementCode();

    if (auto Vehicle = PlayerPawn->GetVehicleActor())
        Vehicle->K2_TeleportTo(DestLocation, DestRotation);

    PlayerPawn->K2_TeleportTo(DestLocation, DestRotation);
    *Ret = true;
}

void UFortMissionLibrary__PostLoad()
{
    ExecHook(UFortMissionLibrary::StaticClass()->FindFunction("TeleportPlayerPawn"), TeleportPlayerPawn);
}
