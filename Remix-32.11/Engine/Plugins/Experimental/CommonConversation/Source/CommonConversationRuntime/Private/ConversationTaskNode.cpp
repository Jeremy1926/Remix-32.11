#include "pch.h"
#include "Shared.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationTaskNode.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationInstance.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationParticipantComponent.h"

EConversationRequirementResult CheckRequirements(UConversationTaskNode* Node, const FConversationContext* Context)
{
    static auto CheckRequirements = (EConversationRequirementResult (*)(UConversationTaskNode*, const FConversationContext*))(ImageBase + 0x89FD158);

    return CheckRequirements(Node, Context);
}

void GenerateNextChoices(UConversationTaskNode* Node, TArray<FConversationBranchPoint>& BranchBuilder, const FConversationContext* Context)
{
    ((void (*)(UConversationTaskNode*, TArray<FConversationBranchPoint>*, const FConversationContext*))Node->VTable[0x62])(Node, &BranchBuilder, Context);
}

void GenerateChoicesForDestinations(TArray<FConversationBranchPoint>& BranchBuilder, const FConversationContext* Context, const TArray<FGuid>& CandidateDestinations)
{
    static auto GenerateChoicesForDestinations = (void (*)(TArray<FConversationBranchPoint>*, const FConversationContext*, const TArray<FGuid>*))(ImageBase + 0x89FEB04);

    GenerateChoicesForDestinations(&BranchBuilder, Context, &CandidateDestinations);
}

void ExecuteTaskNodeWithSideEffects(UConversationTaskNode* Node, const FConversationContext* Context, FConversationTaskResult* OutResult)
{
    auto Conversation = Context->ActiveConversation;
    auto PreviousEvalWorldContextObj = Node->EvalWorldContextObj;

    Node->EvalWorldContextObj = Conversation ? GetWorld(Conversation) : nullptr;
    memset(OutResult, 0, sizeof(FConversationTaskResult));

    if (Context->bServer_PRIVATE)
    {
        *OutResult = Node->ExecuteTaskNode(*Context);

        if (OutResult->CanConversationContinue())
        {
            for (int32 i = 0; i < Node->SubNodes.Num(); i++)
            {
                auto SideEffectNode = (UConversationSideEffectNode*)Node->SubNodes.Data[i];

                if (!SideEffectNode || !SideEffectNode->IsA(UConversationSideEffectNode::StaticClass()))
                    continue;

                auto PreviousSideEffectWorldContextObj = SideEffectNode->EvalWorldContextObj;

                SideEffectNode->EvalWorldContextObj = Conversation ? GetWorld(Conversation) : nullptr;
                SideEffectNode->ServerCauseSideEffect(*Context);
                SideEffectNode->EvalWorldContextObj = PreviousSideEffectWorldContextObj;
            }

            if (Conversation)
                for (auto Actor : GetParticipantActors(Conversation))
                    if (auto Component = FindFor(Actor))
                        ServerNotifyExecuteTaskAndSideEffects(Component, GetCurrentNodeHandle(Conversation), Conversation->ActiveConversationGraph);
        }
    }

    Node->EvalWorldContextObj = PreviousEvalWorldContextObj;
}
