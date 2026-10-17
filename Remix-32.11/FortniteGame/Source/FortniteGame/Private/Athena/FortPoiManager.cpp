#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/ControllerComponents/FortControllerComponent_MapDiscoverability.h"

INIT_MODULE(AFortPoiManager);

static void (*ProcessTagDeltaForControllerOG)(AFortPoiManager*, AFortPlayerControllerGameplay*, TArray<FGameplayTag>&, TArray<FGameplayTag>&);
static void ProcessTagDeltaForController(AFortPoiManager* PoiManager, AFortPlayerControllerGameplay* PlayerController, TArray<FGameplayTag>& ExitedTags, TArray<FGameplayTag>& EnteredTags)
{
    static auto CanVisitPoi = (bool (*)(AFortPlayerControllerGameplay*))(ImageBase + 0x9273890);

    auto PlayerControllerAthena = PlayerController && PlayerController->IsA(AFortPlayerControllerAthena::StaticClass()) ? (AFortPlayerControllerAthena*)PlayerController : nullptr;
    auto Component = PlayerControllerAthena ? PlayerControllerAthena->DiscoverabilityComponent : nullptr;

    if (Component)
        AddDiscoveredData(PoiManager, PlayerControllerAthena, Component);

    ProcessTagDeltaForControllerOG(PoiManager, PlayerController, ExitedTags, EnteredTags);

    if (!Component || *(bool*)(__int64(Component) + 0x39C) || !CanVisitPoi(PlayerController))
        return;

    for (auto& Tag : EnteredTags)
    {
        auto Entry = FindEntry(Component, Tag);

        if (!Entry || Entry->VisitCounter != 0)
            continue;

        if (Entry->DiscoveredState == EPoiDiscoveredState::Undiscovered)
            Entry->DiscoveredState = EPoiDiscoveredState::Discovered;

        Entry->VisitCounter++;
        Component->DiscoveredData.MarkItemDirty(*Entry);
    }
}

void AFortPoiManager__Init()
{
    Hook(ImageBase + 0x927FBB4, ProcessTagDeltaForController, ProcessTagDeltaForControllerOG);
}
