#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"

INIT_POSTLOAD(AFortAthenaMutator_GiveItemsAtGamePhaseStep);

static void OnGamePhaseStepChanged(UObject* Context, FFrame& Stack)
{
    FFortGamePhaseStepUpdatedEvent Event{};
    Stack.StepCompiledIn(&Event);
    Stack.IncrementCode();

    auto Mutator = (AFortAthenaMutator_GiveItemsAtGamePhaseStep*)Context;

    if (Event.GamePhaseStep != Mutator->PhaseToGiveItems)
        return;

    for (auto& PlayerController : Mutator->CachedGameMode->AlivePlayers)
    {
        for (auto& Item : Mutator->ItemsToGive)
            GiveItem((AFortInventory*)PlayerController->WorldInventory.ObjectPointer, Item.ItemToDrop, (int)Item.NumberToGive.Evaluate());
    }
}

void AFortAthenaMutator_GiveItemsAtGamePhaseStep__PostLoad()
{
    ExecHook(AFortAthenaMutator_GiveItemsAtGamePhaseStep::StaticClass()->FindFunction("OnGamePhaseStepChanged"), OnGamePhaseStepChanged);
}
