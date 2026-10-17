#include "pch.h"
#include "Shared.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationInstance.h"

INIT_MODULE(UConversationContextHelpers);

static void SetResult(FFrame& Stack, FConversationTaskResult* Ret, EConversationTaskResultType Type)
{
    auto ConversationContext = FConversationContext::Allocate();
    Stack.StepCompiledInRefInternal(ConversationContext);
    Stack.IncrementCode();

    Ret->Set(Type);
    ConversationContext->Free();
}

static void AdvanceConversation(UObject* Context, FFrame& Stack, FConversationTaskResult* Ret)
{
    SetResult(Stack, Ret, EConversationTaskResultType::AdvanceConversation);
}

static void ReturnToLastClientChoice(UObject* Context, FFrame& Stack, FConversationTaskResult* Ret)
{
    SetResult(Stack, Ret, EConversationTaskResultType::ReturnToLastClientChoice);
}

static void ReturnToCurrentClientChoice(UObject* Context, FFrame& Stack, FConversationTaskResult* Ret)
{
    SetResult(Stack, Ret, EConversationTaskResultType::ReturnToCurrentClientChoice);
}

static void ReturnToConversationStart(UObject* Context, FFrame& Stack, FConversationTaskResult* Ret)
{
    SetResult(Stack, Ret, EConversationTaskResultType::ReturnToConversationStart);
}

static void AbortConversation(UObject* Context, FFrame& Stack, FConversationTaskResult* Ret)
{
    SetResult(Stack, Ret, EConversationTaskResultType::AbortConversation);
}

static void AdvanceConversationWithChoice(UObject* Context, FFrame& Stack, FConversationTaskResult* Ret)
{
    auto ConversationContext = FConversationContext::Allocate();
    Stack.StepCompiledInRefInternal(ConversationContext);

    auto ChoiceTemp = FAdvanceConversationRequest::Allocate();
    auto Choice = (FAdvanceConversationRequest*)Stack.StepCompiledInRefInternal(ChoiceTemp);
    Stack.IncrementCode();

    Ret->Set(EConversationTaskResultType::AdvanceConversationWithChoice, Choice);
    ChoiceTemp->Free();
    ConversationContext->Free();
}

static void PauseConversationAndSendClientChoices(UObject* Context, FFrame& Stack, FConversationTaskResult* Ret)
{
    auto ConversationContext = FConversationContext::Allocate();
    Stack.StepCompiledInRefInternal(ConversationContext);

    auto MessageTemp = FClientConversationMessage::Allocate();
    auto Message = (FClientConversationMessage*)Stack.StepCompiledInRefInternal(MessageTemp);
    Stack.IncrementCode();

    Ret->Set(EConversationTaskResultType::PauseConversationAndSendClientChoices, nullptr, Message);
    MessageTemp->Free();
    ConversationContext->Free();
}

static void MakeConversationParticipant(UObject* Context, FFrame& Stack)
{
    auto ConversationContextTemp = FConversationContext::Allocate();
    auto ConversationContext = (FConversationContext*)Stack.StepCompiledInRefInternal(ConversationContextTemp);
    AActor* ParticipantActor = nullptr;
    FGameplayTag ParticipantTag{};

    Stack.StepCompiledIn(&ParticipantActor);
    Stack.StepCompiledIn(&ParticipantTag);
    Stack.IncrementCode();

    if (ParticipantActor && ParticipantTag.TagName.ComparisonIndex != 0 && ConversationContext->ActiveConversation)
        ServerAssignParticipant(ConversationContext->ActiveConversation, ParticipantTag, ParticipantActor);

    ConversationContextTemp->Free();
}

void UConversationContextHelpers__Init()
{
    auto Class = UConversationContextHelpers::StaticClass();

    ExecHook(Class->FindFunction("AdvanceConversation"), AdvanceConversation);
    ExecHook(Class->FindFunction("AdvanceConversationWithChoice"), AdvanceConversationWithChoice);
    ExecHook(Class->FindFunction("PauseConversationAndSendClientChoices"), PauseConversationAndSendClientChoices);
    ExecHook(Class->FindFunction("ReturnToLastClientChoice"), ReturnToLastClientChoice);
    ExecHook(Class->FindFunction("ReturnToCurrentClientChoice"), ReturnToCurrentClientChoice);
    ExecHook(Class->FindFunction("ReturnToConversationStart"), ReturnToConversationStart);
    ExecHook(Class->FindFunction("AbortConversation"), AbortConversation);
    ExecHook(Class->FindFunction("MakeConversationParticipant"), MakeConversationParticipant);
}
