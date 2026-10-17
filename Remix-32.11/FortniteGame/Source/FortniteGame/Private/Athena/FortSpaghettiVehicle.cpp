#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortOctopusVehicle.h"

INIT_MODULE(AFortSpaghettiVehicle);

void AFortSpaghettiVehicle__Init()
{
    ExecHook(AFortSpaghettiVehicle::StaticClass()->FindFunction("ServerUpdateTowhook"), ServerUpdateTowhook);
}
