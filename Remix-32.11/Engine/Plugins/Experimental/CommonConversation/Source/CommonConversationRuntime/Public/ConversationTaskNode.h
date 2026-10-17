#pragma once
#include "pch.h"

EConversationRequirementResult CheckRequirements(UConversationTaskNode* Node, const FConversationContext* Context);
void GenerateNextChoices(UConversationTaskNode* Node, TArray<FConversationBranchPoint>& BranchBuilder, const FConversationContext* Context);
void GenerateChoicesForDestinations(TArray<FConversationBranchPoint>& BranchBuilder, const FConversationContext* Context, const TArray<FGuid>& CandidateDestinations);
void ExecuteTaskNodeWithSideEffects(UConversationTaskNode* Node, const FConversationContext* Context, FConversationTaskResult* OutResult);
