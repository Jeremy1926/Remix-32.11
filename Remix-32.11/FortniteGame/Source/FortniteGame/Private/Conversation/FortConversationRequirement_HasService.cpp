#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Private/Conversation/FortConversationRequirement_HasService.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortConversationRequirement_ControllerRequirement.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortConversationTaskNode_SellItem.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortNonPlayerConversationParticipantComponent.h"

INIT_MODULE(UFortConversationRequirement_HasService);

EConversationRequirementResult IsRequirementSatisfied(UFortConversationRequirement_HasService* Node, const FConversationContext* Context)
{
    auto NPCComponent = GetNPCComponent(Context);

    if (!NPCComponent || !FindSupportedService(NPCComponent, Node->ServiceTag, Node->bRequireExactMatch))
        return EConversationRequirementResult::FailedAndHidden;

    return EConversationRequirementResult::Passed;
}

void UFortConversationRequirement_HasService__Init()
{
    Hook<UFortConversationRequirement_HasService>(uint32(0x5C), (EConversationRequirementResult (*)(UFortConversationRequirement_HasService*, const FConversationContext*))IsRequirementSatisfied);
}
