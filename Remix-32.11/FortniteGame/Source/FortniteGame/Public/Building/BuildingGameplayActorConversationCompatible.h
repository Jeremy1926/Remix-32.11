#pragma once
#include "pch.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortNonPlayerConversationParticipantComponent.h"

template <class T>
static bool (*ServerOnAttemptInteractOG)(void*, uint8*) = nullptr;

template <class T>
static int64 InteractInterfaceOffset = 0;

template <class T>
static bool ServerOnAttemptInteract(void* InteractInterface, uint8* Request)
{
    static auto GetValidInteractionRequestController = (AActor* (*)(uint8*))(ImageBase + 0x9A9E29C);

    auto Result = ServerOnAttemptInteractOG<T>(InteractInterface, Request);

    if (!Result || Request[0] != 1)
        return Result;

    auto Controller = GetValidInteractionRequestController(Request);

    if (!Controller)
        return Result;

    auto Actor = (AActor*)(__int64(InteractInterface) - InteractInterfaceOffset<T>);
    auto Component = (UFortNonPlayerConversationParticipantComponent*)Actor->GetComponentByClass(UFortNonPlayerConversationParticipantComponent::StaticClass());

    if (Component && CanStartConversationWith(Component, Controller))
        StartConversation(Component, Component->ConversationEntryTag, Controller, Actor, std::is_same_v<T, ABuildingProp_ConversationCompatible> ? UFortPropConversationInstance::StaticClass() : nullptr);

    return Result;
}

template <class T>
static void HookServerOnAttemptInteract()
{
    auto DefaultObject = T::GetDefaultObj();
    auto InteractInterface = DefaultObject->GetInterface(IFortInteractInterface::StaticClass());
    auto InteractVft = *(void***)InteractInterface;

    InteractInterfaceOffset<T> = __int64(InteractInterface) - __int64(DefaultObject);
    ServerOnAttemptInteractOG<T> = decltype(ServerOnAttemptInteractOG<T>)(InteractVft[0x15]);
    VirtualSwap(InteractVft, 0x15, ServerOnAttemptInteract<T>);
}
