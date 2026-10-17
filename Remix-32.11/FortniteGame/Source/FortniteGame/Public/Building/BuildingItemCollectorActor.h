#pragma once
#include "pch.h"

inline void (*VendWobble__FinishedFuncOG)(UObject*, FFrame&, void*);

extern std::unordered_map<int, float> VendingRarityWeights;
extern float VendingRarityWeightSum;
extern float VendingRarityZeroWeight;
extern float VendingTotalWeight;

void VendWobble__FinishedFunc(UObject* Context, FFrame& Stack, void* Ret);
void FillVendingMachine(ABuildingItemCollectorActor* CollectorActor);
