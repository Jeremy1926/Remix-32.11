#pragma once
#include "pch.h"

inline std::unordered_map<int32, UClass*> BuildingClassMap;

extern bool bSkipAircraft;
extern uint8 PauseWarmupMask;

void PauseWarmup(EGameModePauseWarmupMaskId MaskId);
void UnPauseWarmup(EGameModePauseWarmupMaskId MaskId);
void SpawnFloorLootForContainer(const UClass* ContainerType);
int GetLateGamePhase(AFortGameStateAthena* GameState);
