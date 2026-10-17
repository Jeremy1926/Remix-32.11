#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Building/BuildingGameplayActorConversationCompatible.h"

INIT_POSTLOAD(ABuildingGameplayActorConversationCompatible);

void ABuildingGameplayActorConversationCompatible__PostLoad()
{
    HookServerOnAttemptInteract<ABuildingGameplayActorConversationCompatible>();
}
