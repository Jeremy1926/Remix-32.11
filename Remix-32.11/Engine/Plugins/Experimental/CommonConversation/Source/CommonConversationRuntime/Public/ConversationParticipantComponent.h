#pragma once
#include "pch.h"

UConversationParticipantComponent* FindFor(AActor* Actor);
bool HasAuthority(UConversationParticipantComponent* Component);
void ServerNotifyConversationStarted(UConversationParticipantComponent* Component, UConversationInstance* Conversation, const FGameplayTag& AsParticipant);
void ServerNotifyConversationEnded(UConversationParticipantComponent* Component, UConversationInstance* Conversation, const FConversationParticipants& PreservedParticipants);
void ServerNotifyExecuteTaskAndSideEffects(UConversationParticipantComponent* Component, const FConversationNodeHandle& Handle, UConversationDatabase* Graph);
void ServerAbortAllConversations(UConversationParticipantComponent* Component);
void SendClientConversationMessage(UConversationParticipantComponent* Component, const FConversationContext* Context, const FClientConversationMessagePayload& Payload);
void SendClientUpdatedChoices(UConversationParticipantComponent* Component, const FConversationContext* Context, bool bForcedRefresh);
void ServerAdvanceConversation(UObject* Context, FFrame& Stack);
