#pragma once
#include "pch.h"

void FreeData(FIndicatedActorData& Data);
void FreeData(FStenciledActorData& Data);
UFortControllerComponent_IndicatedActorManagement* GetIndicatedActorManagement(AActor* Controller);
void AddIndicatedActors(UFortControllerComponent_IndicatedActorManagement* Component, TArray<AActor*>& IndicatedActors, FIndicatedActorData& Data, bool bAddAsUnique, bool bReplaceExistingEntry, bool bRefreshExistingEntry);
void AddStenciledActors(UFortControllerComponent_IndicatedActorManagement* Component, TArray<AActor*>& StenciledActors, FStenciledActorData& Data, bool bAddAsUnique, bool bReplaceExistingEntry, bool bRefreshExistingEntry);
void AddActorsInRadius(FFrame& Stack, bool bStenciled);
