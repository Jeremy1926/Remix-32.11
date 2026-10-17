#include "pch.h"
#include "Shared.h"

INIT_MODULE(ASpecialEventScript);

static void SetPhaseDataLayersState(UDataLayerManager* DataLayerManager, FPhaseInfo& PhaseInfo, EDataLayerRuntimeState State)
{
    for (auto& DataLayerEntry : PhaseInfo.DataLayers)
        DataLayerManager->SetDataLayerRuntimeState(DataLayerEntry.DataLayerAsset, State, DataLayerEntry.bIsRecursive);
}

static void (*ActivatePhaseOG)(ASpecialEventScript*, int32, float);
static void ActivatePhase(ASpecialEventScript* SpecialEventScript, int32 IndexToActivate, float SequenceTimeOffset)
{
    auto DataLayerManager = UWorld::GetWorld()->GetDataLayerManager();

    if (SpecialEventScript->ReplicatedActivePhaseIndex >= 0)
        SetPhaseDataLayersState(DataLayerManager, SpecialEventScript->PhaseInfoArray[SpecialEventScript->ReplicatedActivePhaseIndex], EDataLayerRuntimeState::Unloaded);

    SetPhaseDataLayersState(DataLayerManager, SpecialEventScript->PhaseInfoArray[IndexToActivate], EDataLayerRuntimeState::Activated);

    SpecialEventScript->ReplicatedActivePhaseIndex = IndexToActivate;

    ActivatePhaseOG(SpecialEventScript, IndexToActivate, SequenceTimeOffset);
}

void ASpecialEventScript__Init()
{
    Hook(ImageBase + 0xC2AA930, ActivatePhase, ActivatePhaseOG);
}
