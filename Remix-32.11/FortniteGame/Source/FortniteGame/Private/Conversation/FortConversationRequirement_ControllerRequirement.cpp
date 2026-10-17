#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortConversationRequirement_ControllerRequirement.h"
#include "FortniteGame/Source/FortniteGame/Private/Conversation/FortConversationRequirement_HasService.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortConversationTaskNode_SellItem.h"

INIT_MODULE(UFortConversationRequirement_ControllerRequirement);

static void ResolveParticipant(AActor* Participant, AActor*& OutActor, AController*& OutController)
{
    OutActor = Participant;
    OutController = nullptr;

    if (!Participant)
        return;

    if (Participant->IsA(AController::StaticClass()))
    {
        OutController = (AController*)Participant;
        OutActor = OutController->Pawn;
    }
    else if (Participant->IsA(APawn::StaticClass()))
        OutController = ((APawn*)Participant)->Controller;
}

bool IsControllerRequirementMet(UFortControllerRequirement* Requirement, const FConversationContext* Context, const FGameplayTag& ParticipantID)
{
    auto Conversation = Context ? Context->ActiveConversation : nullptr;

    if (!Requirement || !Conversation)
        return false;

    AActor* Subject = nullptr;
    AActor* Other = nullptr;

    for (auto& Entry : Conversation->Participants.List)
    {
        if (Entry.ParticipantID.TagName == ParticipantID.TagName)
            Subject = Entry.Actor;
        else if (!Other)
            Other = Entry.Actor;
    }

    FControllerRequirementTestContext RequestContext{};

    ResolveParticipant(Subject, RequestContext.TestSubjectActor, RequestContext.TestSubjectController);
    ResolveParticipant(Other, RequestContext.OtherActor, RequestContext.OtherController);

    return Requirement->IsRequirementMet(RequestContext);
}

EConversationRequirementResult IsRequirementSatisfied(UFortConversationRequirement_ControllerRequirement* Node, const FConversationContext* Context)
{
    return IsControllerRequirementMet(Node->Requirement, Context, Node->ParticipantID) ? EConversationRequirementResult::Passed : EConversationRequirementResult::FailedAndHidden;
}

void UFortConversationRequirement_ControllerRequirement__Init()
{
    Hook<UFortConversationRequirement_ControllerRequirement>(uint32(0x5C), (EConversationRequirementResult (*)(UFortConversationRequirement_ControllerRequirement*, const FConversationContext*))IsRequirementSatisfied);
}
