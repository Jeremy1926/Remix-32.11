#pragma once
#include "pch.h"

inline std::map<int32, TArray<FFortLootTierData*>> TierDataMap;
inline std::map<int32, TArray<FFortLootPackageData*>> LootPackageMap;

void ChooseLootForContainer(TArray<FFortItemEntry*>& LootDrops, FName TierGroup, int LootTier = -1, int WorldLevel = ((AFortGameStateAthena*)UWorld::GetWorld()->GameState)->WorldLevel);
