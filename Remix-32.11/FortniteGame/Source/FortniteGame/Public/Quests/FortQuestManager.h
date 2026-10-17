#pragma once
#include "pch.h"

struct FPendingDamageTaken
{
    FWeakObjectPtr Victim;
    FWeakObjectPtr DamageSource;
    FGameplayTagContainer DamageSourceTags;
    FGameplayTagContainer DamageTypeTags;
    int32 Damage = 0;
    double Distance = 0.0;
};

extern FPendingDamageTaken PendingDamageTaken;

void SendStatEvent(UFortQuestManager* QuestManager, AActor* PlayerController, EFortQuestObjectiveStatEvent StatEvent, int32 Count, bool bAllowQueueStatEvent, UObject* TargetObject = nullptr,
                   FGameplayTagContainer TargetTags = FGameplayTagContainer(), FGameplayTagContainer AdditionalSourceTags = FGameplayTagContainer());
void RegisterWithQuestManager(AFortPlayerControllerAthena* PlayerController);
void SendPlacementUpdate(AFortPlayerControllerAthena* Eliminator);
void EndBattleRoyaleGame(AFortPlayerControllerAthena* PlayerController);
AFortPlayerControllerAthena* ResolvePlayerController(UObject* Object);
void SendDamageVerbs(AFortPlayerControllerAthena* PlayerController, AFortPawn* Pawn, FPendingDamageTaken& Pending, float Damage, bool bCriticalHit);
bool HasGameplayEventObjective(UFortQuestManager* QuestManager, FGameplayTag EventTag);
void SendPlayerReceivedGameplayEvent(AFortPlayerControllerAthena* PlayerController, FGameplayTag EventTag, const FGameplayEventData& Payload);
void SendRequestedMatchReports(double Now);
