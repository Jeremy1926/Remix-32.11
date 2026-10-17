#include "pch.h"
#include "Shared.h"

INIT_MODULE(ABuildingActor);

static void (*BroadcastNotifyBuildingNetCullDistChangeOG)(void*, ABuildingActor*, float);
static void BroadcastNotifyBuildingNetCullDistChange(void* Delegate, ABuildingActor* BuildingActor, float CullDistance)
{
    BroadcastNotifyBuildingNetCullDistChangeOG(Delegate, BuildingActor, CullDistance);

    if (Delegate != (void*)(ImageBase + 0x12C5E1F0) || !BuildingActor)
        return;

    static auto SetCullDistanceOverride = (void (*)(AActor*, float))(ImageBase + 0x7059998);

    auto NetCullDistance = CullDistance + 1000.f;
    SetCullDistanceOverride(BuildingActor, NetCullDistance * NetCullDistance);
}

void ABuildingActor__Init()
{
    Hook(ImageBase + 0x521A444, BroadcastNotifyBuildingNetCullDistChange, BroadcastNotifyBuildingNetCullDistChangeOG);
}
