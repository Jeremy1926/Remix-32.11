#include "pch.h"
#include "Shared.h"
#include "SDK/SDK/FortniteAIServer_classes.hpp"

INIT_MODULE(UFortAthenaBTService_PickUpLoot);

static void (*OnBecomeRelevantOG)(UFortAthenaBTService_PickUpLoot*, UBehaviorTreeComponent*, uint8*);

static uint8 OnPickupInteractionExecutionStatusChanged(UFortAthenaBTService_PickUpLoot* Service, UBlackboardComponent* Blackboard, uint16, UBehaviorTreeComponent* OwnerComp)
{
    static auto GetExecutionStatus = (uint8 (*)(UBlackboardComponent*, uint16))(ImageBase + 0xA87DEC4);
    static auto GetLootActor = (AActor* (*)(UFortAthenaBTService_PickUpLoot*, UBehaviorTreeComponent*))(ImageBase + 0xA9919C0);
    static auto ConstructInteractionType = (FInteractionType* (*)(FInteractionType*, AFortPlayerPawn*, ETInteractionType))(ImageBase + 0x9A90180);
    static auto FlyToPlayer = (void (*)(AFortPickup*, AFortPlayerPawn*))(ImageBase + 0x9CD3254);

    auto Controller = OwnerComp ? (AFortAthenaAIBotController*)OwnerComp->AIOwner : nullptr;

    if (!Controller || !Controller->IsA(AFortAthenaAIBotController::StaticClass()))
        return 0;

    auto Pawn = Controller->PlayerBotPawn;

    if (GetExecutionStatus(Blackboard, *(uint16*)(uint64(Service) + 0x88)) != 2 || !Pawn)
        return 1;

    auto Loot = GetLootActor(Service, OwnerComp);

    if (!Loot)
        return 1;

    if (Loot->IsA(ABuildingActor::StaticClass()))
    {
        auto InteractInterface = Loot->GetInterface(IFortInteractInterface::StaticClass());

        if (!InteractInterface)
            return 1;

        FInteractionType InteractionType;
        ConstructInteractionType(&InteractionType, Pawn, ETInteractionType::IT_LongPress);
        ((bool (*)(void*, FInteractionType*))(*(void***)InteractInterface)[0x15])(InteractInterface, &InteractionType);
    }
    else if (Loot->IsA(AFortPickup::StaticClass()))
        FlyToPlayer((AFortPickup*)Loot, Pawn);

    return 1;
}

static void OnBecomeRelevant(UFortAthenaBTService_PickUpLoot* Service, UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory)
{
    static auto CreateObserverDelegate = (void* (*)(void*, UFortAthenaBTService_PickUpLoot*, void*, UBehaviorTreeComponent**))(ImageBase + 0xA988A14);
    static auto RegisterObserver = (void* (*)(UBlackboardComponent*, uint64*, uint16, UObject*, void*))(ImageBase + 0x773699C);

    OnBecomeRelevantOG(Service, OwnerComp, NodeMemory);

    auto Blackboard = OwnerComp->BlackboardComp;

    if (!Blackboard)
        return;

    struct alignas(16) { void* Function; uint64 Adjustment; } ObserverMethod{ (void*)OnPickupInteractionExecutionStatusChanged, 0 };
    uint8 ObserverDelegate[0x10];
    uint64 ObserverHandle = 0;

    RegisterObserver(Blackboard, &ObserverHandle, *(uint16*)(uint64(Service) + 0x88), Service, CreateObserverDelegate(ObserverDelegate, Service, &ObserverMethod, &OwnerComp));
}

void UFortAthenaBTService_PickUpLoot__Init()
{
    Hook<UFortAthenaBTService_PickUpLoot>(uint32(0x63), OnBecomeRelevant, OnBecomeRelevantOG);
}
