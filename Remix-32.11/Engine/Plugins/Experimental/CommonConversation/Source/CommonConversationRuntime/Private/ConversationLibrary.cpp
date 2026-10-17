#include "pch.h"
#include "Shared.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationLibrary.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationInstance.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortNonPlayerConversationParticipantComponent.h"

INIT_MODULE(UConversationLibrary);

UConversationInstance* StartConversation(const FGameplayTag& ConversationEntryTag, AActor* Instigator, const FGameplayTag& InstigatorTag, AActor* Target, const FGameplayTag& TargetTag, UClass* ConversationInstanceClass, UConversationDatabase* Graph)
{
    if (!Instigator || !Target)
        return nullptr;

    auto World = GetWorld(Instigator);

    if (!World)
        return nullptr;

    auto InstanceClass = ConversationInstanceClass;

    if (!InstanceClass)
    {
        InstanceClass = UConversationSettings::GetDefaultObj()->ConversationInstanceClass.Get();

        if (!InstanceClass)
            InstanceClass = UConversationInstance::StaticClass();
    }

    auto ConversationInstance = (UConversationInstance*)UGameplayStatics::SpawnObject(InstanceClass, World);

    if (!ConversationInstance)
        return nullptr;

    if (TargetTag.TagName.ComparisonIndex != 0)
        ServerAssignParticipant(ConversationInstance, TargetTag, Target);

    if (InstigatorTag.TagName.ComparisonIndex != 0)
        ServerAssignParticipant(ConversationInstance, InstigatorTag, Instigator);

    ServerStartConversation(ConversationInstance, ConversationEntryTag, Graph);

    return ConversationInstance;
}

void StartConversation(UObject* Context, FFrame& Stack, UConversationInstance** Ret)
{
    FGameplayTag ConversationEntryTag{};
    AActor* Instigator = nullptr;
    FGameplayTag InstigatorTag{};
    AActor* Target = nullptr;
    FGameplayTag TargetTag{};
    UClass* ConversationInstanceClass = nullptr;

    Stack.StepCompiledIn(&ConversationEntryTag);
    Stack.StepCompiledIn(&Instigator);
    Stack.StepCompiledIn(&InstigatorTag);
    Stack.StepCompiledIn(&Target);
    Stack.StepCompiledIn(&TargetTag);
    Stack.StepCompiledIn(&ConversationInstanceClass);
    Stack.IncrementCode();

    *Ret = StartConversation(ConversationEntryTag, Instigator, InstigatorTag, Target, TargetTag, ConversationInstanceClass);
}

void StartConversationFromGraph(UObject* Context, FFrame& Stack, UConversationInstance** Ret)
{
    FGameplayTag ConversationEntryTag{};
    AActor* Instigator = nullptr;
    FGameplayTag InstigatorTag{};
    AActor* Target = nullptr;
    FGameplayTag TargetTag{};
    UConversationDatabase* Graph = nullptr;

    Stack.StepCompiledIn(&ConversationEntryTag);
    Stack.StepCompiledIn(&Instigator);
    Stack.StepCompiledIn(&InstigatorTag);
    Stack.StepCompiledIn(&Target);
    Stack.StepCompiledIn(&TargetTag);
    Stack.StepCompiledIn(&Graph);
    Stack.IncrementCode();

    *Ret = Graph ? StartConversation(ConversationEntryTag, Instigator, InstigatorTag, Target, TargetTag, nullptr, Graph) : nullptr;
}

void UConversationLibrary__Init()
{
    ExecHook(UConversationLibrary::StaticClass()->FindFunction("StartConversation"), (void (*)(UObject*, FFrame&, UConversationInstance**))StartConversation);
    ExecHook(UConversationLibrary::StaticClass()->FindFunction("StartConversationFromGraph"), (void (*)(UObject*, FFrame&, UConversationInstance**))StartConversationFromGraph);
}
