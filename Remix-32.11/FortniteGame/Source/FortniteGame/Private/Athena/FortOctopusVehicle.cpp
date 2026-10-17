#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortOctopusVehicle.h"

INIT_MODULE(AFortOctopusVehicle);

template <class T>
static void SetNetTowhookAimDir(T* Vehicle, const FVector_NetQuantizeNormal& InNetTowhookAimDir)
{
    Vehicle->NetTowhookAimDir = InNetTowhookAimDir;
    Vehicle->OnRep_NetTowhookAimDir();
}

void ServerUpdateTowhook(UObject* Context, FFrame& Stack, void* Ret)
{
    FVector_NetQuantizeNormal InNetTowhookAimDir;
    Stack.StepCompiledIn(&InNetTowhookAimDir);
    Stack.IncrementCode();

    if (Context->IsA(AFortOctopusVehicle::StaticClass()))
        SetNetTowhookAimDir((AFortOctopusVehicle*)Context, InNetTowhookAimDir);
    else
        SetNetTowhookAimDir((AFortSpaghettiVehicle*)Context, InNetTowhookAimDir);
}

void AFortOctopusVehicle__Init()
{
    ExecHook(AFortOctopusVehicle::StaticClass()->FindFunction("ServerUpdateTowhook"), ServerUpdateTowhook);
}
