#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Plugins/GameFeatures/Accolades/Source/AccoladesRuntime/Public/Components/FortGameStateComponent_Accolades.h"
#include "FortniteGame/Source/FortniteGame/Public/Quests/FortQuestManager.h"

INIT_POSTLOAD(UFortAbilitySystemComponentAthena);

static int32 (*HandleGameplayEventOG)(UAbilitySystemComponent*, FGameplayTag, const FGameplayEventData*);
static int32 HandleGameplayEvent(UAbilitySystemComponent* AbilitySystemComponent, FGameplayTag EventTag, const FGameplayEventData* Payload)
{
    auto Result = HandleGameplayEventOG(AbilitySystemComponent, EventTag, Payload);
    auto PlayerController = AbilitySystemComponent && Payload ? ResolvePlayerController(AbilitySystemComponent->OwnerActor) : nullptr;

    if (!PlayerController)
        return Result;

    auto QuestManager = PlayerController->GetQuestManager(ESubGame::Athena);

    if (QuestManager && (HasGameplayEventObjective(QuestManager, EventTag) || HasAccoladeGameplayEvent(EventTag)))
        SendPlayerReceivedGameplayEvent(PlayerController, EventTag, *Payload);

    return Result;
}

void UFortAbilitySystemComponentAthena__PostLoad()
{
    auto HandleGameplayEventFunction = UFortAbilitySystemComponentAthena::GetDefaultObj()->VTable[0xE3];

    Hook((uint64)HandleGameplayEventFunction, HandleGameplayEvent, HandleGameplayEventOG);
}
