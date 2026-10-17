#pragma once
#include "pch.h"

void AddExtraData(TArray<FConversationNodeParameterPair>& ExtraData, const wchar_t* Key, const FString& Value);
void AddExtraData(TArray<FConversationNodeParameterPair>& ExtraData, const wchar_t* Key, const wchar_t* Value);
void CopyTags(FGameplayTagContainer& Tags, const FGameplayTagContainer& Source);
void GatherStaticExtraData(UFortConversationTaskNode* Node, const FConversationContext* Context, TArray<FConversationNodeParameterPair>* InOutExtraData);
