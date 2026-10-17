#include "pch.h"
#include "Shared.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationInstance.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationParticipantComponent.h"

INIT_MODULE(UFortPlayerConversationComponent);

static void RequestServerAbortConversation(UObject* Context, FFrame& Stack)
{
    Stack.IncrementCode();

    auto Component = (UFortPlayerConversationComponent*)Context;

    if (HasAuthority(Component))
        ServerAbortAllConversations(Component);
}

static void RequestServerAbortConversationWithParticipant(UObject* Context, FFrame& Stack)
{
    UConversationParticipantComponent* Participant = nullptr;

    Stack.StepCompiledIn(&Participant);
    Stack.IncrementCode();

    auto Component = (UFortPlayerConversationComponent*)Context;
    auto ParticipantActor = Participant ? Participant->GetOwner() : nullptr;

    if (!HasAuthority(Component) || !ParticipantActor)
        return;

    std::vector<UConversationInstance*> ConversationsToAbort;

    for (int32 i = 0; i < Component->Auth_Conversations.Num(); i++)
    {
        auto Conversation = Component->Auth_Conversations.Data[i];
        auto Actors = GetParticipantActors(Conversation);

        if (std::find(Actors.begin(), Actors.end(), ParticipantActor) != Actors.end())
            ConversationsToAbort.push_back(Conversation);
    }

    for (auto Conversation : ConversationsToAbort)
        ServerAbortConversation(Conversation);
}

void UFortPlayerConversationComponent__Init()
{
    ExecHook(UFortPlayerConversationComponent::StaticClass()->FindFunction("RequestServerAbortConversation"), RequestServerAbortConversation);
    ExecHook(UFortPlayerConversationComponent::StaticClass()->FindFunction("RequestServerAbortConversationWithParticipant"), RequestServerAbortConversationWithParticipant);
}
