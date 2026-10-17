#include "pch.h"
#include "Shared.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationParticipantComponent.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationInstance.h"

INIT_MODULE(UConversationParticipantComponent);

UConversationParticipantComponent* FindFor(AActor* Actor)
{
    return Actor ? (UConversationParticipantComponent*)Actor->GetComponentByClass(UConversationParticipantComponent::StaticClass()) : nullptr;
}

bool HasAuthority(UConversationParticipantComponent* Component)
{
    auto Owner = Component->GetOwner();

    return Owner && Owner->Role == ENetRole::ROLE_Authority;
}

static void UpdateConversationsActive(UConversationParticipantComponent* Component, int32 NewConversationsActive)
{
    Component->ConversationsActive = NewConversationsActive;
    UNetPushModelHelpers::MarkPropertyDirty(Component, FName(L"ConversationsActive"));
}

void ServerNotifyConversationStarted(UConversationParticipantComponent* Component, UConversationInstance* Conversation, const FGameplayTag& AsParticipant)
{
    if (!HasAuthority(Component) || !Conversation)
        return;

    Component->Auth_CurrentConversation = Conversation;
    Component->Auth_Conversations.Add(Conversation);
    Component->ClientUpdateParticipants(Conversation->Participants);

    auto OldConversationsActive = Component->ConversationsActive;
    UpdateConversationsActive(Component, OldConversationsActive + 1);

    if (OldConversationsActive == 0)
        Component->OnRep_ConversationsActive(OldConversationsActive);

    Component->ClientStartConversation(Conversation->Participants);
    Component->ClientUpdateConversations(Component->ConversationsActive);
}

void ServerNotifyConversationEnded(UConversationParticipantComponent* Component, UConversationInstance* Conversation, const FConversationParticipants& PreservedParticipants)
{
    if (!HasAuthority(Component) || !Conversation)
        return;

    auto& Conversations = Component->Auth_Conversations;

    for (int32 i = 0; i < Conversations.Num(); i++)
    {
        if (Conversations.Data[i] != Conversation)
            continue;

        if (Component->Auth_CurrentConversation == Conversation)
            Component->Auth_CurrentConversation = nullptr;

        Conversations.Remove(i);

        auto OldConversationsActive = Component->ConversationsActive;
        UpdateConversationsActive(Component, OldConversationsActive - 1);

        Component->ClientExitConversation(PreservedParticipants);
        Component->ClientUpdateConversations(Component->ConversationsActive);

        if (Component->ConversationsActive == 0)
            Component->OnRep_ConversationsActive(OldConversationsActive);

        break;
    }
}

void ServerNotifyExecuteTaskAndSideEffects(UConversationParticipantComponent* Component, const FConversationNodeHandle& Handle, UConversationDatabase* Graph)
{
    if (HasAuthority(Component))
        Component->ClientExecuteTaskAndSideEffects(Handle, Graph);
}

void ServerAbortAllConversations(UConversationParticipantComponent* Component)
{
    std::vector<UConversationInstance*> Conversations;

    for (int32 i = 0; i < Component->Auth_Conversations.Num(); i++)
        Conversations.push_back(Component->Auth_Conversations.Data[i]);

    for (auto Conversation : Conversations)
        ServerAbortConversation(Conversation);
}

void SendClientConversationMessage(UConversationParticipantComponent* Component, const FConversationContext* Context, const FClientConversationMessagePayload& Payload)
{
    auto& LastMessage = Component->LastMessage;

    LastMessage.Destroy();
    memset(&LastMessage, 0, sizeof(FClientConversationMessagePayload));
    LastMessage.CopyFrom(Payload);
    Component->ClientUpdateConversation(LastMessage);
}

void SendClientUpdatedChoices(UConversationParticipantComponent* Component, const FConversationContext* Context, bool bForcedRefresh)
{
    auto Conversation = Context->ActiveConversation;

    if (!Conversation)
        return;

    auto& NewOptions = GetCurrentUserChoices(Conversation);
    auto& LastMessage = Component->LastMessage;

    if (!bForcedRefresh && FClientConversationOptionEntry::AreArraysIdentical(NewOptions, LastMessage.Options))
        return;

    FClientConversationOptionEntry::DestroyArray(LastMessage.Options);
    FClientConversationOptionEntry::CopyArray(LastMessage.Options, NewOptions);
    Component->ClientUpdateConversation(LastMessage);
}

void ServerAdvanceConversation(UObject* Context, FFrame& Stack)
{
    auto InChoicePicked = FAdvanceConversationRequest::Allocate();
    Stack.StepCompiledIn(InChoicePicked);
    Stack.IncrementCode();

    auto Component = (UConversationParticipantComponent*)Context;

    if (HasAuthority(Component) && Component->Auth_CurrentConversation)
        ServerAdvanceConversation(Component->Auth_CurrentConversation, *InChoicePicked);

    InChoicePicked->Free();
}

void UConversationParticipantComponent__Init()
{
    ExecHook(UConversationParticipantComponent::StaticClass()->FindFunction("ServerAdvanceConversation"), (void (*)(UObject*, FFrame&))ServerAdvanceConversation);
}
