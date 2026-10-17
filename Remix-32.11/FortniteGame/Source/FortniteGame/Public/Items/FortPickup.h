#pragma once
#include "pch.h"

AFortPickupAthena* SpawnPickup(FVector Loc, FFortItemEntry& Entry, EFortPickupSourceTypeFlag SourceTypeFlag = EFortPickupSourceTypeFlag::Other, EFortPickupSpawnSource SpawnSource = EFortPickupSpawnSource::Unset,
                               AFortPawn* Pawn = nullptr, int OverrideCount = -1, bool bToss = true, bool bRandomRotation = true, bool bCombine = true, const UClass* OverrideClass = nullptr, FVector FinalLoc = FVector());
AFortPickupAthena* SpawnPickup(FVector Loc, const UFortItemDefinition* ItemDefinition, int Count, int LoadedAmmo, EFortPickupSourceTypeFlag SourceTypeFlag = EFortPickupSourceTypeFlag::Other,
                               EFortPickupSpawnSource SpawnSource = EFortPickupSpawnSource::Unset, AFortPawn* Pawn = nullptr, bool bToss = true, bool bRandomRotation = true, const UClass* OverrideClass = nullptr);
AFortPickupAthena* SpawnPickup(ABuildingContainer* Container, FFortItemEntry& Entry, AFortPawn* Pawn = nullptr, int OverrideCount = -1);
