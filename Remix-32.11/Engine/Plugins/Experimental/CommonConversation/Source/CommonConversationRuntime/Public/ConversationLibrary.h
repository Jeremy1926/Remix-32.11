#pragma once
#include "pch.h"

UConversationInstance* StartConversation(const FGameplayTag& ConversationEntryTag, AActor* Instigator, const FGameplayTag& InstigatorTag, AActor* Target, const FGameplayTag& TargetTag, UClass* ConversationInstanceClass, UConversationDatabase* Graph = nullptr);
void StartConversation(UObject* Context, FFrame& Stack, UConversationInstance** Ret);
void StartConversationFromGraph(UObject* Context, FFrame& Stack, UConversationInstance** Ret);
