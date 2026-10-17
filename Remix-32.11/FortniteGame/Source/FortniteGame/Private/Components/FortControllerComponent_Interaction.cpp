#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"

INIT_POSTLOAD(UFortControllerComponent_Interaction);

static void (*ServerAttemptInteractOG)(UObject*, FFrame&, void*);
static void ServerAttemptInteract(UObject* Context, FFrame& Stack, void* Result)
{
    auto ReceivingActor = *(AActor**)Stack.Locals;
    auto PlayerController = (AFortPlayerControllerAthena*)((UActorComponent*)Context)->GetOwner();

    if (!PlayerController || !PlayerController->IsA(AFortPlayerControllerAthena::StaticClass()) || !ReceivingActor)
        return ServerAttemptInteractOG(Context, Stack, Result);

    auto Pawn = PlayerController->MyFortPawn;
    auto WorldInventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;

    if (ReceivingActor->IsA(ABuildingItemCollectorActor::StaticClass()))
    {
        auto CollectorActor = (ABuildingItemCollectorActor*)ReceivingActor;

        if (CollectorActor->IsA(ABuildingItemWeaponUpgradeActor::StaticClass()))
            return ServerAttemptInteractOG(Context, Stack, Result);

        CollectorActor->ControllingPlayer = PlayerController;

        FCollectorUnitInfo* Collection = nullptr;

        for (auto& ItemCollection : CollectorActor->ItemCollections)
        {
            if (ItemCollection.InputItem == CollectorActor->ActiveInputItem)
            {
                Collection = &ItemCollection;
                break;
            }
        }

        auto Cost = Collection ? Collection->InputCount.Evaluate((float)CollectorActor->StartingGoalLevel) : 0.f;
        auto Item = Collection && Cost > 0 ? FindItemInstance(WorldInventory, Collection->InputItem) : nullptr;

        if (!Collection || (Cost > 0 && (!Item || Item->ItemEntry.Count < (int)Cost)))
        {
            CollectorActor->bCurrentInteractionSuccess = false;
            CollectorActor->ControllingPlayer = nullptr;
            CollectorActor->BlueprintOnInteract(Pawn, EInteractionBeingAttempted::FirstInteraction);
            return ServerAttemptInteractOG(Context, Stack, Result);
        }

        if (auto Entry = Cost > 0 ? FindReplicatedEntry(WorldInventory, Collection->InputItem) : nullptr)
        {
            Entry->Count -= (int)Cost;
            UpdateOrRemoveEntry(WorldInventory, Item, *Entry);
        }

        CollectorActor->ClientPausedActiveInputItem = CollectorActor->ActiveInputItem;
        CollectorActor->bCurrentInteractionSuccess = true;
        CollectorActor->BlueprintOnInteract(Pawn, EInteractionBeingAttempted::FirstInteraction);
    }
    else if (ReceivingActor->IsA(ABuildingProp_LockDevice::StaticClass()))
    {
        auto LockDevice = (ABuildingProp_LockDevice*)ReceivingActor;
        LockDevice->CurrentLockState = ELockState::UNLOCKED;
        LockDevice->OnRep_CurrentLockState();
    }

    ServerAttemptInteractOG(Context, Stack, Result);
}

static void ServerNotifyStartLongUse(UObject* Context, FFrame& Stack)
{
    AActor* ReceivingActor;

    Stack.StepCompiledIn(&ReceivingActor);
    Stack.IncrementCode();

    auto Component = (UFortControllerComponent_Interaction*)Context;
    auto PlayerController = (AFortPlayerControllerAthena*)Component->GetOwner();
    auto InteractingPawn = PlayerController ? PlayerController->MyFortPawn : nullptr;

    if (!ReceivingActor || !InteractingPawn)
        return;

    auto& LongInteractValidateInfo = Component->LongInteractValidateInfo;
    LongInteractValidateInfo.Target = ReceivingActor;
    *(float*)(uint64(&LongInteractValidateInfo) + 0x8) = (float)UGameplayStatics::GetTimeSeconds(ReceivingActor);

    auto MultiInteractComponent = (UFortActorComponent_MultiInteract*)ReceivingActor->GetComponentByClass(UFortActorComponent_MultiInteract::StaticClass());

    if (MultiInteractComponent && MultiInteractComponent->IsMultipleInteractionEnabled())
        MultiInteractComponent->AddInteractingActor(InteractingPawn);

    if (auto InteractInterface = ReceivingActor->GetInterface(IFortInteractInterface::StaticClass()))
        ((void (*)(void*, AActor*))(*(void***)InteractInterface)[0x11])(InteractInterface, InteractingPawn);
}

static void ServerNotifyEndLongUse(UObject* Context, FFrame& Stack)
{
    AActor* ReceivingActor;
    bool bUseCompleted;

    Stack.StepCompiledIn(&ReceivingActor);
    Stack.StepCompiledIn(&bUseCompleted);
    Stack.IncrementCode();

    auto Component = (UFortControllerComponent_Interaction*)Context;
    auto PlayerController = (AFortPlayerControllerAthena*)Component->GetOwner();
    auto InteractingPawn = PlayerController ? PlayerController->MyFortPawn : nullptr;

    auto& LongInteractValidateInfo = Component->LongInteractValidateInfo;

    if (LongInteractValidateInfo.Target == ReceivingActor)
        LongInteractValidateInfo.Target = nullptr;

    if (!ReceivingActor || !InteractingPawn)
        return;

    auto MultiInteractComponent = (UFortActorComponent_MultiInteract*)ReceivingActor->GetComponentByClass(UFortActorComponent_MultiInteract::StaticClass());

    if (MultiInteractComponent && MultiInteractComponent->IsMultipleInteractionEnabled())
        MultiInteractComponent->RemoveInteractingActor(InteractingPawn);

    if (auto InteractInterface = ReceivingActor->GetInterface(IFortInteractInterface::StaticClass()))
        ((void (*)(void*, AActor*, bool))(*(void***)InteractInterface)[0x10])(InteractInterface, InteractingPawn, bUseCompleted);
}

void UFortControllerComponent_Interaction__PostLoad()
{
    ExecHook(UFortControllerComponent_Interaction::StaticClass()->FindFunction("ServerAttemptInteract"), ServerAttemptInteract, ServerAttemptInteractOG);
    ExecHook(UFortControllerComponent_Interaction::StaticClass()->FindFunction("ServerNotifyStartLongUse"), ServerNotifyStartLongUse);
    ExecHook(UFortControllerComponent_Interaction::StaticClass()->FindFunction("ServerNotifyEndLongUse"), ServerNotifyEndLongUse);
}
