#pragma once
#include "pch.h"

struct FServiceEntry
{
    int32 ServiceTag;
    int32 AdditionalRequirementTag;
    bool bShowAsLockedIfUnavailable;
};

struct FChosenSaleItem
{
    UFortItemDefinition* ItemDefinition;
    int32 StackSize;
    const FNPCSaleInventoryRow* Row;
};

void SetNPCUniqueIDTag(const UObject* Component, const FGameplayTag& Tag);
bool CanStartConversationWith(const UFortNonPlayerConversationParticipantComponent* Component, AActor* PlayerController);
UConversationInstance* StartConversation(const UFortNonPlayerConversationParticipantComponent* Component, const FGameplayTag& InConversationEntryTag, AActor* Instigator, AActor* Target, const UClass* ConversationInstanceClass = nullptr, const UConversationDatabase* Graph = nullptr);
uint64 GetObjectKey(const UObject* Object);
FGameplayTag GetServiceProviderTag(const UFortNonPlayerConversationParticipantComponent* Component);
UFortNonPlayerConversationParticipantComponent* GetNPCComponent(const FConversationContext* Context);
const FServiceEntry* FindSupportedService(const UFortNonPlayerConversationParticipantComponent* Component, const FGameplayTag& ServiceTag, bool bExactMatch);
const FChosenSaleItem* GetSaleItem(const UFortNonPlayerConversationParticipantComponent* Component, int32 SellSlot, const FGameplayTag& FilterCategoryTag);
float GetDiscountMultiplier(const UFortNonPlayerConversationParticipantComponent* Component);
void StartConversation(UObject* Context, FFrame& Stack);
void StartConversationFromGraph(UObject* Context, FFrame& Stack);
