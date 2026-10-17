#pragma once
#include "pch.h"

UFortWeaponModSetData* GetRandomWeaponModSetData(const UObject* WorldContext, const UFortWeaponItemDefinition* WeaponItemDefinition);
void ApplyModSetToPickup(const UFortWeaponModSetData* ModSet, AFortPickup* WeaponPickup);
