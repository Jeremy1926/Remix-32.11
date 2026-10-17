#pragma once
#include "pch.h"

TArray<FClientConversationOptionEntry>& GetCurrentUserChoices(UConversationInstance* Conversation);
FConversationNodeHandle& GetCurrentNodeHandle(UConversationInstance* Conversation);
UObject* GetWorld(UObject* Object);
std::vector<AActor*> GetParticipantActors(UConversationInstance* Conversation);
void ServerAssignParticipant(UConversationInstance* Conversation, const FGameplayTag& ParticipantID, AActor* ParticipantActor);
void DetermineBranches(UConversationInstance* Conversation, TArray<FGuid>& EnabledPaths, const TArray<FGuid>& SourceList, EConversationRequirementResult MaximumRequirementResult);
void ServerAbortConversation(UConversationInstance* Conversation);
void ServerStartConversation(UConversationInstance* Conversation, const FGameplayTag& EntryPoint, UConversationDatabase* Graph);
void ServerAdvanceConversation(UConversationInstance* Conversation, const FAdvanceConversationRequest& InChoicePicked);
