#pragma once
#include "pch.h"

UConversationRegistry* GetFromWorld(UObject* World);
UConversationNode* GetRuntimeNodeFromGUID(UConversationRegistry* Registry, const FGuid& NodeGUID, UConversationDatabase* Graph);
void GetOutputLinkGUIDs(UConversationRegistry* Registry, TArray<FGuid>& Result, UConversationDatabase* Graph, const FGameplayTag& EntryPoint);
