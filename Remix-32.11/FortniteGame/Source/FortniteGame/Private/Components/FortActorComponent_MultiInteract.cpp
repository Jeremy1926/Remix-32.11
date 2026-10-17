#include "pch.h"
#include "Shared.h"

INIT_POSTLOAD(UFortActorComponent_MultiInteract);

static AActor* GetInteractingPlayerState(AActor* InteractingActor)
{
    if (InteractingActor->IsA(AFortPlayerStateAthena::StaticClass()))
        return InteractingActor;

    if (InteractingActor->IsA(AFortPlayerPawnAthena::StaticClass()))
    {
        auto PlayerState = ((AFortPlayerPawnAthena*)InteractingActor)->PlayerState;

        if (PlayerState && PlayerState->IsA(AFortPlayerStateAthena::StaticClass()))
            return PlayerState;
    }

    return nullptr;
}

static void OnInteractingActorsChanged(UFortActorComponent_MultiInteract* Component, AActor* PlayerState, TArray<AActor*>& InteractingActors)
{
    if (PlayerState)
        Component->OnRep_InteractingPlayers();
    else
    {
        Component->InteractingNonPlayerActorCount = InteractingActors.Num();
        Component->OnRep_InteractingNonPlayerActorCount();
    }
}

static void AddInteractingActor(UObject* Context, FFrame& Stack, void* Ret)
{
    AActor* InteractingActorToAdd;
    Stack.StepCompiledIn(&InteractingActorToAdd);
    Stack.IncrementCode();

    auto Component = (UFortActorComponent_MultiInteract*)Context;
    auto Owner = Component->GetOwner();

    if (!InteractingActorToAdd || !Owner || Owner->Role != ENetRole::ROLE_Authority)
        return;

    auto PlayerState = GetInteractingPlayerState(InteractingActorToAdd);
    auto& InteractingActors = PlayerState ? *(TArray<AActor*>*)&Component->InteractingPlayers : Component->InteractingNonPlayerActors;
    auto InteractingActor = PlayerState ? PlayerState : InteractingActorToAdd;

    if (InteractingActors.Contains(InteractingActor))
        return;

    if (Component->GetInteractingActorCount(true, true) == 0)
    {
        auto GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;

        Component->ServerInteractionData.ID = Component->ServerInteractionData.ID == 0xFF ? 1 : Component->ServerInteractionData.ID + 1;
        Component->ServerInteractionData.ServerWorldTime = GameState ? (float)GameState->GetServerWorldTimeSeconds() : -1.f;
        Component->SetComponentTickEnabled(true);
    }

    InteractingActors.Add(InteractingActor);
    OnInteractingActorsChanged(Component, PlayerState, InteractingActors);
}

static void RemoveInteractingActor(UObject* Context, FFrame& Stack, void* Ret)
{
    AActor* InteractingActorToRemove;
    Stack.StepCompiledIn(&InteractingActorToRemove);
    Stack.IncrementCode();

    auto Component = (UFortActorComponent_MultiInteract*)Context;
    auto Owner = Component->GetOwner();

    if (!InteractingActorToRemove || !Owner || Owner->Role != ENetRole::ROLE_Authority)
        return;

    auto PlayerState = GetInteractingPlayerState(InteractingActorToRemove);
    auto& InteractingActors = PlayerState ? *(TArray<AActor*>*)&Component->InteractingPlayers : Component->InteractingNonPlayerActors;
    auto InteractingActor = PlayerState ? PlayerState : InteractingActorToRemove;

    for (int i = 0; i < InteractingActors.Num(); i++)
    {
        if (InteractingActors[i] != InteractingActor)
            continue;

        InteractingActors.Remove(i);
        OnInteractingActorsChanged(Component, PlayerState, InteractingActors);

        if (Component->GetInteractingActorCount(true, true) == 0 && !Component->bAllowGradualReset)
            Component->ResetGradualInteractionTimer();

        return;
    }
}

void UFortActorComponent_MultiInteract__PostLoad()
{
    ExecHook(UFortActorComponent_MultiInteract::StaticClass()->FindFunction("AddInteractingActor"), AddInteractingActor);
    ExecHook(UFortActorComponent_MultiInteract::StaticClass()->FindFunction("RemoveInteractingActor"), RemoveInteractingActor);
}
