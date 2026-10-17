#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Building/BuildingGameplayActorConversationCompatible.h"

INIT_POSTLOAD(ABuildingProp_ConversationCompatible);

void ABuildingProp_ConversationCompatible__PostLoad()
{
    HookServerOnAttemptInteract<ABuildingProp_ConversationCompatible>();
}
