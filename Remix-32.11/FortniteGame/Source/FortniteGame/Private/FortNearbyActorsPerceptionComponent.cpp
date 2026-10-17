#include "pch.h"
#include "Shared.h"

INIT_MODULE(UFortNearbyActorsPerceptionComponent);

static void (*TickComponentOG)(UFortNearbyActorsPerceptionComponent*, float, uint8, void*);
static void TickComponent(UFortNearbyActorsPerceptionComponent* Component, float DeltaTime, uint8 TickType, void* ThisTickFunction)
{
    auto& CachedAIDirector = *(FWeakObjectPtr*)(uint64(Component) + 0xB8);

    if (!CachedAIDirector.Get())
    {
        auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;

        if (GameMode && GameMode->AIDirector)
            CachedAIDirector = FWeakObjectPtr(GameMode->AIDirector);
    }

    TickComponentOG(Component, DeltaTime, TickType, ThisTickFunction);
}

void UFortNearbyActorsPerceptionComponent__Init()
{
    auto TickFunction = UFortNearbyActorsPerceptionComponent::GetDefaultObj()->VTable[0x7B];

    TickComponentOG = decltype(TickComponentOG)(TickFunction);

    for (int32 i = 0; i < UObject::GObjects->Num(); i++)
    {
        auto Object = UObject::GObjects->GetByIndex(i);

        if (Object && Object->IsDefaultObject() && Object->IsA(UFortNearbyActorsPerceptionComponent::StaticClass()) && Object->VTable[0x7B] == TickFunction)
            VirtualSwap(Object->VTable, 0x7B, TickComponent);
    }
}
