#pragma once
#include "pch.h"
#include <functional>

void ProcessAccoladeEvent(AFortPlayerControllerAthena* PlayerController, FGameplayTagContainer* ContextTags, const std::function<int32(const FInstancedStruct&, FName)>& EvaluateFilter);
bool HasAccoladeGameplayEvent(FGameplayTag EventTag);
