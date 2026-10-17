#pragma once
#include "pch.h"

void DropItemsAsPickups(TArray<UFortWorldItem*>& ItemsToDropViaPickup, AFortPawn* DestructionPawn, AFortInventory* Inventory, AActor* PickupPostSpawnObject, void (*PickupPostSpawn)(AActor*, AFortPickup*, int32));
const UClass* ApplyOutsideSafeZoneEffect(UAbilitySystemComponent* AbilitySystemComponent);
void ReportDeath(AController* Controller, AFortPlayerStateAthena* PlayerState, AFortPlayerPawnAthena* Pawn, AFortPlayerStateAthena* KillerPlayerState, AFortPlayerPawnAthena* KillerPawn, const FGameplayTagContainer& Tags);
UFortWeaponItemDefinition* GetKillerWeapon(AActor* DamageCauser);
void GiveLateGameLoadout(AFortInventory* WorldInventory, AFortPawn* Pawn);
void PlaceLateGamePawn(AController* Controller, AFortPawn* Pawn);
void ReportElimination(AFortGameModeAthena* GameMode, AFortPlayerStateAthena* PlayerState, AFortPlayerStateAthena* KillerPlayerState, AFortPlayerPawnAthena* KillerPawn, UFortWeaponItemDefinition* KillerWeapon, EDeathCause DeathCause);
void RemoveDroppableEntries(AFortInventory* WorldInventory);
