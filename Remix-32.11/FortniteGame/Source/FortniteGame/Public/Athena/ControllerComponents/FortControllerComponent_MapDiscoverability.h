#pragma once
#include "pch.h"

FPoiDiscoverabilityDataEntry* FindEntry(UFortControllerComponent_MapDiscoverability* Component, const FGameplayTag& POITag);
void AddDiscoveredData(AFortPoiManager* PoiManager, AFortPlayerControllerAthena* PlayerController, UFortControllerComponent_MapDiscoverability* Component);
