#include "pch.h"
#include "Shared.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationInstance.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationRegistry.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationTaskNode.h"

INIT_MODULE(UConversationLinkNode);

static void GatherChoices(UConversationLinkNode* Node, TArray<FConversationBranchPoint>* BranchBuilder, const FConversationContext* Context)
{
    auto Conversation = Context->ActiveConversation;
    auto Registry = Context->ConversationRegistry;

    if (!Conversation || !Registry)
        return;

    TArray<FGuid> PotentialStartingPoints;
    GetOutputLinkGUIDs(Registry, PotentialStartingPoints, nullptr, Node->RemoteEntryTag);

    if (PotentialStartingPoints.Num() > 0)
    {
        TArray<FGuid> LegalStartingPoints;
        DetermineBranches(Conversation, LegalStartingPoints, PotentialStartingPoints, EConversationRequirementResult::FailedButVisible);

        auto ReturnScopeContext = Context->CreateReturnScopeContext(Node->Compiled_NodeGUID);
        GenerateChoicesForDestinations(*BranchBuilder, ReturnScopeContext, LegalStartingPoints);
        ReturnScopeContext->Free();
        LegalStartingPoints.Free();
    }

    PotentialStartingPoints.Free();
}

void UConversationLinkNode__Init()
{
    VirtualSwap(UConversationLinkNode::GetDefaultObj()->VTable, 0x63, GatherChoices);
}
