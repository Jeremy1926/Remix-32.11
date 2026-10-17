#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortPlayerPawnAthena.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortNonPlayerConversationParticipantComponent.h"
#include "FortniteGame/Source/FortniteGame/Public/Pawns/FortPlayerPawn.h"

INIT_POSTLOAD(AFortPlayerPawnAthena);

static void (*TriggerAbilityFromAnimationOG)(AFortPlayerPawnAthena*, UObject*, uint32, uint32, bool, uint32);
static bool (*ServerOnAttemptInteractOG)(void*, uint8*);

static void (*OnPlayerStateChangedOG)(AFortPlayerPawnAthena*, APlayerState*, APlayerState*);

static int64 InteractInterfaceOffset = 0;

static AFortPlayerPawnAthena* GetInteractInterfacePawn(void* InteractInterface)
{
    return (AFortPlayerPawnAthena*)(__int64(InteractInterface) - InteractInterfaceOffset);
}

static void ForceReviveFromDBNO(UObject* Context, FFrame& Stack)
{
    AActor* EventInstigator;

    Stack.StepCompiledIn(&EventInstigator);
    Stack.IncrementCode();

    ReviveFromDBNO((AFortPlayerPawnAthena*)Context, EventInstigator, true);
}

static void ServerNotifyStartLongUse(void* InteractInterface, AFortPlayerPawnAthena* InteractingPawn)
{
    auto Pawn = GetInteractInterfacePawn(InteractInterface);

    if (Pawn->IsDBNO())
        UpdateDBNORevivingActors(Pawn);
}

static void ServerNotifyEndLongUse(void* InteractInterface, AFortPlayerPawnAthena* InteractingPawn, bool bUseCompleted)
{
    auto Pawn = GetInteractInterfacePawn(InteractInterface);

    if (Pawn->DBNORevivingActorsCount > 0)
        UpdateDBNORevivingActors(Pawn);
}

static void TriggerAbilityFromAnimation(AFortPlayerPawnAthena* Pawn, UObject* Animation, uint32 ApplicationTag, uint32 MontageSectionIfBlocked, bool bSetNextSectionIfBlocked, uint32 MontageSectionIfTargetInvalid)
{
    TriggerAbilityFromAnimationOG(Pawn, Animation, ApplicationTag, MontageSectionIfBlocked, bSetNextSectionIfBlocked, MontageSectionIfTargetInvalid);

    if (!Animation || Pawn->Role != ENetRole::ROLE_Authority || Pawn->IsLocallyControlled() || Pawn->IsBotControlled())
        return;

    auto AbilitySystemComponent = Pawn->AbilitySystemComponent;
    UFortGameplayAbility* Ability = nullptr;

    for (int i = 0; AbilitySystemComponent && !Ability && i < AbilitySystemComponent->ActivatableAbilities.Items.Num(); i++)
    {
        auto& Spec = AbilitySystemComponent->ActivatableAbilities.Items[i];

        for (auto Instances : { &Spec.ReplicatedInstances, &Spec.NonReplicatedInstances })
        {
            for (auto Instance : *Instances)
            {
                if (!Ability && Instance && Instance->IsA(UFortGameplayAbility::StaticClass()) && Instance->CurrentMontage == Animation)
                    Ability = (UFortGameplayAbility*)Instance;
            }
        }
    }

    if (!Ability || Ability->NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted)
        return;

    for (auto Task : Ability->ActiveTasks)
    {
        if (Task && Task->IsA(UFortAbilityTask_PlayMontageWaitTarget::StaticClass()) && *(EFortAbilityTargetDataPolicy*)(uint64(Task) + 0x100) == EFortAbilityTargetDataPolicy::SimulateOnServer)
        {
            ((void (*)(UFortGameplayAbility*, UObject*, uint32))Ability->VTable[0xAA])(Ability, AbilitySystemComponent, ApplicationTag);
            return;
        }
    }
}

static void SetIsInsideSafeZone(AFortPlayerPawnAthena* Pawn, bool bNewValue)
{
    if (Pawn->bIsInsideSafeZone == bNewValue)
        return;

    Pawn->bIsInsideSafeZone = bNewValue;
    Pawn->OnRep_IsInsideSafeZone();
}

static void UpdateOutsideSafeZone(AFortPlayerPawnAthena* Pawn)
{
    SetIsInsideSafeZone(Pawn, !Pawn->bIsInAnyStorm);
}

static bool ServerOnAttemptInteract(void* InteractInterface, uint8* Request)
{
    auto Result = ServerOnAttemptInteractOG(InteractInterface, Request);

    if (Request[1] != 0 || Request[0] != 1 || !((bool (*)(void*, uint8*))(*(void***)InteractInterface)[0x6])(InteractInterface, Request))
        return Result;

    static auto GetValidInteractionRequestController = (AActor* (*)(uint8*))(ImageBase + 0x9A9E29C);
    static auto GetNPCConversationComponentForInteraction = (bool (*)(AFortPlayerPawnAthena*, AActor*, UObject**))(ImageBase + 0xA17C574);
    static auto GetInteractionRequestController = (AActor* (*)(uint8*))(ImageBase + 0x9A9C3AC);

    auto Pawn = GetInteractInterfacePawn(InteractInterface);
    auto Controller = GetValidInteractionRequestController(Request);
    UObject* ConversationComponent = nullptr;

    if (!Controller || !GetNPCConversationComponentForInteraction(Pawn, Controller, &ConversationComponent) || !ConversationComponent)
        return Result;

    if (auto Instigator = GetInteractionRequestController(Request))
    {
        auto NPCConversationComponent = (UFortNonPlayerConversationParticipantComponent*)ConversationComponent;
        StartConversation(NPCConversationComponent, NPCConversationComponent->ConversationEntryTag, Instigator, Pawn);
    }

    return Result;
}

static uint64 GetTeamGroupHandle(APlayerState* PlayerState)
{
    auto TeamPrivateInfo = PlayerState && PlayerState->IsA(AFortPlayerState::StaticClass()) ? ((AFortPlayerState*)PlayerState)->PlayerTeamPrivate.Get() : nullptr;

    return TeamPrivateInfo ? *(uint64*)(uint64(TeamPrivateInfo) + 0x568) : 0;
}

void UpdateTeamGroup(APawn* Pawn, APlayerState* OldPlayerState, APlayerState* NewPlayerState)
{
    static auto GetReplicationSystem = (UReplicationSystem* (*)(AActor*))(ImageBase + 0x1BC1D9C);
    static auto GetReplicationBridge = (void* (*)(UReplicationSystem*))(ImageBase + 0x637B0E8);
    static auto GetReplicatedRefHandle = (uint64* (*)(void*, uint64*, UObject*, uint8))(ImageBase + 0x63A31E0);
    static auto AddToGroup = (bool (*)(UReplicationSystem*, uint64, uint64))(ImageBase + 0x63B8BEC);
    static auto RemoveFromGroup = (bool (*)(UReplicationSystem*, uint64, uint64))(ImageBase + 0x63C3760);

    auto OldTeamGroupHandle = GetTeamGroupHandle(OldPlayerState);
    auto TeamGroupHandle = GetTeamGroupHandle(NewPlayerState);

    if (!Pawn || OldTeamGroupHandle == TeamGroupHandle)
        return;

    auto ReplicationSystem = GetReplicationSystem(Pawn);
    auto ReplicationBridge = ReplicationSystem ? GetReplicationBridge(ReplicationSystem) : nullptr;
    uint64 Handle = 0;

    if (ReplicationBridge)
        GetReplicatedRefHandle(ReplicationBridge, &Handle, Pawn, 0);

    if (!Handle)
        return;

    if (OldTeamGroupHandle)
        RemoveFromGroup(ReplicationSystem, OldTeamGroupHandle, Handle);

    if (TeamGroupHandle)
        AddToGroup(ReplicationSystem, TeamGroupHandle, Handle);
}

static void OnPlayerStateChanged(AFortPlayerPawnAthena* Pawn, APlayerState* NewPlayerState, APlayerState* OldPlayerState)
{
    OnPlayerStateChangedOG(Pawn, NewPlayerState, OldPlayerState);

    UpdateTeamGroup(Pawn, OldPlayerState, NewPlayerState);
}

void AFortPlayerPawnAthena__PostLoad()
{
    auto PawnClass = AFortPlayerPawnAthena::StaticClass();
    auto DefaultPawn = AFortPlayerPawnAthena::GetDefaultObj();
    auto InteractInterface = DefaultPawn->GetInterface(IFortInteractInterface::StaticClass());
    auto InteractVft = *(void***)InteractInterface;

    InteractInterfaceOffset = __int64(InteractInterface) - __int64(DefaultPawn);

    ExecHook(PawnClass->FindFunction("ForceReviveFromDBNO"), ForceReviveFromDBNO);
    VirtualSwap(InteractVft, 0x10, ServerNotifyEndLongUse);
    VirtualSwap(InteractVft, 0x11, ServerNotifyStartLongUse);

    ServerOnAttemptInteractOG = decltype(ServerOnAttemptInteractOG)(InteractVft[0x15]);
    VirtualSwap(InteractVft, 0x15, ServerOnAttemptInteract);

    Hook<AFortPlayerPawnAthena>(uint32(0x1F9), TriggerAbilityFromAnimation, TriggerAbilityFromAnimationOG);
    Hook<AFortPlayerPawnAthena>(uint32(0x271), SetIsInsideSafeZone);
    Hook<AFortPlayerPawnAthena>(uint32(0x272), UpdateOutsideSafeZone);
    Hook<AFortPlayerPawnAthena>(uint32(0xE8), OnPlayerStateChanged, OnPlayerStateChangedOG);
}
