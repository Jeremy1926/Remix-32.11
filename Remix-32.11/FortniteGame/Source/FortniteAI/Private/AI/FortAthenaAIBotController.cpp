#include "pch.h"
#include "Shared.h"
#include "Config.h"
#include "FortniteGame/Source/FortniteAI/Public/AI/FortAthenaAIBotController.h"
#include "Engine/Source/Runtime/Engine/Public/TimerManager.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortPlayerControllerAthena.h"
#include "FortniteGame/Source/FortniteGame/Public/FortKismetLibrary.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortPickup.h"

INIT_MODULE(AFortAthenaAIBotController);
INIT_TICKER(AFortAthenaAIBotController);

static void (*InitializeInventoryOG)(void*, TArray<FItemAndCount>*);
static void (*ApplyCosmeticLoadoutOG)(AFortAthenaAIBotController*, void*);
static void (*OnPossesedPawnDiedOG)(AFortAthenaAIBotController*, AFortPawn*, float, AController*, AActor*);
static void (*OnPawnDiedOG)(AFortAthenaAIBotController*, AFortPawn*, float, const FGameplayTagContainer&, const FGameplayEffectContextHandle&, AController*, AActor*, AActor*);

static bool CreateInventory(void* InventoryOwnerInterface)
{
    auto Controller = (AFortAthenaAIBotController*)(uint64(InventoryOwnerInterface) - 0x3F0);
    auto Inventory = SpawnActor<AFortInventory>(FVector{}, FRotator{}, Controller);

    if (!Inventory)
        return false;

    Inventory->InventoryType = EFortInventoryType::World;

    Controller->Inventory.ObjectPointer = Inventory;
    Controller->Inventory.InterfacePointer = Inventory->GetInterface(IFortInventoryInterface::StaticClass());

    return Controller->Inventory.InterfacePointer != nullptr;
}

static void InitializeInventory(void* InventoryOwnerInterface, TArray<FItemAndCount>* Items)
{
    auto Controller = (AFortAthenaAIBotController*)(uint64(InventoryOwnerInterface) - 0x3F0);
    auto InventoryRuntimeParameters = Controller->CachedInventoryRuntimeParameters;

    if (!InventoryRuntimeParameters || InventoryRuntimeParameters->bSkipInventoryInitialization)
        return InitializeInventoryOG(InventoryOwnerInterface, Items);

    if (!Controller->Inventory.ObjectPointer)
        CreateInventory(InventoryOwnerInterface);

    auto Inventory = (AFortInventory*)Controller->Inventory.ObjectPointer;
    bool bRemoveAllItemsOnInitialization = InventoryRuntimeParameters->bRemoveAllItemsOnInitialization;

    if (Inventory && bRemoveAllItemsOnInitialization)
    {
        std::vector<FGuid> ItemGuids;

        for (int32 i = 0; i < Inventory->Inventory.ItemInstances.Num(); i++)
            if (auto Item = Inventory->Inventory.ItemInstances.Data[i])
                ItemGuids.push_back(Item->ItemEntry.ItemGuid);

        for (auto& ItemGuid : ItemGuids)
            Remove(Inventory, ItemGuid);
    }

    if (Inventory && Items && InventoryRuntimeParameters->bAddStartingItemsToInventory)
    {
        for (int32 i = 0; i < Items->Num(); i++)
        {
            auto& ItemAndCount = Items->Data[i];
            auto ItemDefinition = (UFortItemDefinition*)ItemAndCount.Item;

            if (!ItemDefinition || ItemAndCount.Count <= 0)
                continue;

            auto Stats = ItemDefinition->IsA(UFortWeaponItemDefinition::StaticClass()) ? GetStats((UFortWeaponItemDefinition*)ItemDefinition) : nullptr;

            GiveItem(Inventory, ItemDefinition, ItemAndCount.Count, Stats ? Stats->ClipSize : 0);
        }
    }

    if (Inventory && InventoryRuntimeParameters->bGrantPickAxe)
    {
        auto CosmeticLoadoutProvider = (void*)(uint64(Controller) + 0x3D0);
        auto GetCosmeticLoadout = (FFortAthenaLoadout* (*)(void*, uint8))(*(void***)CosmeticLoadoutProvider)[0x3];
        auto Pickaxe = GetCosmeticLoadout(CosmeticLoadoutProvider, 2)->Pickaxe;

        if (!Pickaxe)
            Pickaxe = GetCosmeticLoadout(CosmeticLoadoutProvider, 0)->Pickaxe;

        if (auto PickaxeWeaponDefinition = Pickaxe ? Pickaxe->WeaponDefinition : nullptr)
        {
            auto& ReplicatedEntries = Inventory->Inventory.ReplicatedEntries;

            for (int32 i = ReplicatedEntries.Num() - 1; i >= 0; i--)
            {
                auto ItemDefinition = (UFortItemDefinition*)ReplicatedEntries[i].ItemDefinition;

                if (ItemDefinition && ItemDefinition->ItemType == EFortItemType::WeaponHarvest)
                    Remove(Inventory, ReplicatedEntries[i].ItemGuid);
            }

            GiveItem(Inventory, PickaxeWeaponDefinition, 1);
        }
    }

    InventoryRuntimeParameters->bRemoveAllItemsOnInitialization = false;
    InitializeInventoryOG(InventoryOwnerInterface, Items);
    InventoryRuntimeParameters->bRemoveAllItemsOnInitialization = bRemoveAllItemsOnInitialization;
}

void RefreshSlotItems(AFortAthenaAIBotController* Controller)
{
    static auto SetItem = (void (*)(FFortBotInventoryInfo*, UFortWorldItem*))(ImageBase + 0xA7980C0);

    auto Inventory = (AFortInventory*)Controller->Inventory.ObjectPointer;

    if (!Inventory)
        return;

    auto& ItemInstances = Inventory->Inventory.ItemInstances;
    auto& SlotItems = Controller->SlotItems;
    auto NumSlots = (std::min)(SlotItems.Num(), Controller->MaxInventorySlots);

    for (int32 i = 0; i < NumSlots; i++)
        if (SlotItems[i].FortItem && !ItemInstances.Contains(SlotItems[i].FortItem))
            SetItem(&SlotItems[i], nullptr);

    for (auto Item : ItemInstances)
    {
        auto ItemDefinition = (UFortItemDefinition*)Item->ItemEntry.ItemDefinition;

        if (!ItemDefinition || (ItemDefinition->ItemType != EFortItemType::WeaponHarvest && !TakesQuickbarSlot(ItemDefinition)))
            continue;

        int32 FreeSlot = -1;
        int32 Slot = 0;

        for (; Slot < NumSlots && SlotItems[Slot].FortItem != Item; Slot++)
            if (FreeSlot < 0 && !SlotItems[Slot].FortItem)
                FreeSlot = Slot;

        if (Slot == NumSlots && FreeSlot >= 0)
            SetItem(&SlotItems[FreeSlot], Item);
    }
}

static bool RemoveInventoryItem(void* InventoryOwnerInterface, FGuid& ItemGuid, int32 Count, bool bForceRemoval, bool bForcePersistWhenEmpty)
{
    auto Controller = (AFortAthenaAIBotController*)(uint64(InventoryOwnerInterface) - 0x3F0);
    auto Inventory = (AFortInventory*)Controller->Inventory.ObjectPointer;

    if (!Inventory || !RemoveInventoryItem(Inventory, ItemGuid, Count, bForceRemoval, bForcePersistWhenEmpty))
        return false;

    RefreshSlotItems(Controller);
    return true;
}

static int32 GetNumInStack(UFortWorldItem* Item)
{
    return ((int32 (*)(UFortWorldItem*))Item->VTable[0x80])(Item);
}

static void DropInventoryItems(AFortAthenaAIBotController* Controller, AFortPawn* FortPawn)
{
    static auto ExternalShouldAlwaysDropItemOnDeathOrLogout = (bool (*)(UFortWorldItem*, bool))(ImageBase + 0x9206ADC);
    static auto ExternalShouldDropOrDestroyByItemType = (void (*)(AController*, UFortWorldItemDefinition*, bool*, bool*))(ImageBase + 0x9206B68);
    static auto GetPickupComponentData = (FFortItemComponentData_Pickup* (*)(UFortWorldItemDefinition*))(ImageBase + 0x1BC59A8);
    static auto HandlePickupPostSpawnOnDeath = (void (*)(AActor*, AFortPickup*, int32))(ImageBase + 0xA785A00);

    auto Inventory = (AFortInventory*)Controller->Inventory.ObjectPointer;

    if (!Inventory || !Controller->Inventory.InterfacePointer || !FortPawn)
        return;

    TArray<UFortWorldItem*> ItemsToDropViaPickup;
    TArray<UFortWorldItem*> ItemsToDropViaDestruction;

    for (auto Item : Inventory->Inventory.ItemInstances)
    {
        auto ItemDefinition = ((UFortWorldItemDefinition* (*)(UFortWorldItem*))Item->VTable[0xB8])(Item);

        if (!ItemDefinition || !ItemDefinition->IsA(UFortWorldItemDefinition::StaticClass()))
            continue;

        bool bShouldDestroy = false;
        bool bShouldDrop = ExternalShouldAlwaysDropItemOnDeathOrLogout(Item, false);

        ExternalShouldDropOrDestroyByItemType(Controller, ItemDefinition, &bShouldDrop, &bShouldDestroy);

        if (!bShouldDrop && !bShouldDestroy)
            continue;

        auto PickupComponentData = GetPickupComponentData(ItemDefinition);
        auto DropBehavior = PickupComponentData ? PickupComponentData->DropBehavior : EWorldItemDropBehavior::DropAsPickup;

        if (bShouldDestroy || (DropBehavior != EWorldItemDropBehavior::DropAsPickup && DropBehavior != EWorldItemDropBehavior::DropAsPickupEvenWhenEmpty &&
                               (DropBehavior != EWorldItemDropBehavior::DropAsPickupDestroyOnEmpty || GetNumInStack(Item) <= 0)))
            ItemsToDropViaDestruction.Add(Item);
        else
            ItemsToDropViaPickup.Add(Item);
    }

    DropItemsAsPickups(ItemsToDropViaPickup, FortPawn, Inventory, Controller, HandlePickupPostSpawnOnDeath);

    auto InventoryOwnerInterface = (void*)(uint64(Controller) + 0x3F0);
    auto OwnerRemoveInventoryItem = (bool (*)(void*, FGuid&, int32, bool, bool))(*(void***)InventoryOwnerInterface)[0x3D];

    for (auto Item : ItemsToDropViaDestruction)
    {
        auto ItemGuid = Item->ItemEntry.ItemGuid;

        OwnerRemoveInventoryItem(InventoryOwnerInterface, ItemGuid, GetNumInStack(Item), true, false);
    }

    ItemsToDropViaPickup.Free();
    ItemsToDropViaDestruction.Free();
}

static void DropLootOnDeath(AFortAthenaAIBotController* Controller, AFortPawn* FortPawn)
{
    static auto NotifyPickupsSpawnedOnDeath = (void (*)(AFortAthenaAIBotController*, TArray<AFortPickup*>*, int32))(ImageBase + 0xA78C2D0);

    if (FortPawn)
    {
        auto LoopStateComponent = (UFortAthenaNPCLoopStateComponent*)FortPawn->GetComponentByClass(UFortAthenaNPCLoopStateComponent::StaticClass());

        if (LoopStateComponent && LoopStateComponent->bSpawnOutsideTheLoop)
            return;
    }

    auto InventoryRuntimeParameters = Controller->CachedInventoryRuntimeParameters;
    auto AbilitySystemInterface = FortPawn ? FortPawn->GetInterface(IAbilitySystemInterface::StaticClass()) : nullptr;
    auto AbilitySystemComponent = AbilitySystemInterface ? ((UAbilitySystemComponent* (*)(void*))(*(void***)AbilitySystemInterface)[0x2])(AbilitySystemInterface) : nullptr;
    TArray<AFortPickup*> SpawnedPickups;

    if (InventoryRuntimeParameters && (InventoryRuntimeParameters->LootInfo.DataTable || InventoryRuntimeParameters->LootInfo.RowName.ComparisonIndex) && AbilitySystemComponent)
    {
        auto& LootInfo = InventoryRuntimeParameters->LootInfo;
        FFortAthenaAILootInfoDataTableRow* LootInfoRow = nullptr;

        if (LootInfo.DataTable)
        {
            for (auto& Row : LootInfo.DataTable->RowMap)
            {
                if (Row.Key() == LootInfo.RowName)
                {
                    LootInfoRow = (FFortAthenaAILootInfoDataTableRow*)Row.Value();
                    break;
                }
            }
        }

        if (LootInfoRow && LootInfoRow->LootDroppingBehaviors.Num() > 0)
        {
            auto Location = FortPawn->K2_GetActorLocation();
            auto GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
            auto PlaylistContextTags = GameState ? GameState->GetAthenaPlaylistContextTags() : FGameplayTagContainer{};
            FGameplayTagContainer OwnedGameplayTags{};
            GetOwnedGameplayTags(FortPawn, OwnedGameplayTags);

            auto& LootDroppingBehaviors = LootInfoRow->LootDroppingBehaviors;

            for (int32 i = 0; i < LootDroppingBehaviors.Num(); i++)
            {
                auto& LootDroppingBehavior = LootDroppingBehaviors.Data[i];

                if (!UBlueprintGameplayTagLibrary::IsTagQueryEmpty(LootDroppingBehavior.OptionalTagQuery) &&
                    !UBlueprintGameplayTagLibrary::DoesContainerMatchTagQuery(OwnedGameplayTags, LootDroppingBehavior.OptionalTagQuery))
                    continue;

                if (!UBlueprintGameplayTagLibrary::IsTagQueryEmpty(LootDroppingBehavior.OptionalPlaylistTagQuery) &&
                    !UBlueprintGameplayTagLibrary::DoesContainerMatchTagQuery(PlaylistContextTags, LootDroppingBehavior.OptionalPlaylistTagQuery))
                    continue;

                if (LootDroppingBehavior.bShouldDropInventoryOnDeath)
                    DropInventoryItems(Controller, FortPawn);

                auto& LootTiers = LootDroppingBehavior.LootTiers;

                if (!LootDroppingBehavior.bShouldDropLootOnDeath || LootTiers.Num() == 0)
                    continue;

                int32 FirstTier = 0;
                int32 LastTier = LootTiers.Num();

                if (LootDroppingBehavior.bShouldBotPickRandomLootTier)
                {
                    FirstTier = rand() % LootTiers.Num();
                    LastTier = FirstTier + 1;
                }

                for (int32 Tier = FirstTier; Tier < LastTier; Tier++)
                {
                    TArray<FFortItemEntry*> LootDrops{};

                    ChooseLootForContainer(LootDrops, LootTiers.Data[Tier]);

                    for (auto& LootDrop : LootDrops)
                    {
                        if (auto Pickup = SpawnPickup(Location, *LootDrop, EFortPickupSourceTypeFlag::Tossed, EFortPickupSpawnSource::Unset, FortPawn))
                            SpawnedPickups.Add(Pickup);

                        free(LootDrop);
                    }

                    LootDrops.Free();
                }
            }

            FreeGameplayTags(OwnedGameplayTags);
            FreeGameplayTags(PlaylistContextTags);
        }
    }
    else if (FortPawn && FortPawn->IsA(AFortPlayerPawnAthena::StaticClass()) && ((AFortPlayerPawnAthena*)FortPawn)->bShouldDropItemsOnDeath)
        DropInventoryItems(Controller, FortPawn);

    NotifyPickupsSpawnedOnDeath(Controller, &SpawnedPickups, -1);
    SpawnedPickups.Free();
}

static void OnPawnDied(AFortAthenaAIBotController* Controller, AFortPawn* DeadPawn, float Damage, const FGameplayTagContainer& DamageTags, const FGameplayEffectContextHandle& EffectContext,
                       AController* InstigatedBy, AActor* DamageCauser, AActor* KillerActor)
{
    auto PlayerState = (AFortPlayerStateAthena*)Controller->PlayerState;

    if (DeadPawn && (AFortPawn*)Controller->PlayerBotPawn == DeadPawn && Controller->bIsAnAthenaGameParticipant && PlayerState && PlayerState->IsA(AFortPlayerStateAthena::StaticClass()))
    {
        auto KillerPlayerState = InstigatedBy && InstigatedBy->PlayerState && InstigatedBy->PlayerState->IsA(AFortPlayerStateAthena::StaticClass()) ? (AFortPlayerStateAthena*)InstigatedBy->PlayerState : nullptr;
        auto KillerPawn = InstigatedBy && InstigatedBy->Pawn && InstigatedBy->Pawn->IsA(AFortPawn::StaticClass()) ? (AFortPlayerPawnAthena*)InstigatedBy->Pawn : nullptr;

        ReportDeath(Controller, PlayerState, (AFortPlayerPawnAthena*)DeadPawn, KillerPlayerState, KillerPawn, DamageTags);
    }

    OnPawnDiedOG(Controller, DeadPawn, Damage, DamageTags, EffectContext, InstigatedBy, DamageCauser, KillerActor);
}

static void OnPossesedPawnDied(AFortAthenaAIBotController* Controller, AFortPawn* Pawn, float Damage, AController* InstigatedBy, AActor* DamageCauser)
{
    auto PlayerState = (AFortPlayerStateAthena*)Controller->PlayerState;

    if (!Pawn || (AFortPawn*)Controller->PlayerBotPawn != Pawn || !Controller->bIsAnAthenaGameParticipant || !PlayerState || !PlayerState->IsA(AFortPlayerStateAthena::StaticClass()))
        return OnPossesedPawnDiedOG(Controller, Pawn, Damage, InstigatedBy, DamageCauser);

    auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
    auto KillerPlayerState = InstigatedBy && InstigatedBy->PlayerState && InstigatedBy->PlayerState->IsA(AFortPlayerStateAthena::StaticClass()) ? (AFortPlayerStateAthena*)InstigatedBy->PlayerState : nullptr;
    auto KillerPawn = InstigatedBy && InstigatedBy->Pawn && InstigatedBy->Pawn->IsA(AFortPawn::StaticClass()) ? (AFortPlayerPawnAthena*)InstigatedBy->Pawn : nullptr;

    auto bWasAlive = GameMode && GameMode->IsA(AFortGameModeAthena::StaticClass()) && GameMode->AliveBots.Contains(Controller);

    OnPossesedPawnDiedOG(Controller, Pawn, Damage, InstigatedBy, DamageCauser);

    if (bWasAlive && !GameMode->AliveBots.Contains(Controller))
        ReportElimination(GameMode, PlayerState, KillerPlayerState, KillerPawn, GetKillerWeapon(DamageCauser), PlayerState->DeathInfo.DeathCause);
}

static void ApplyCosmeticLoadout(AFortAthenaAIBotController* Controller, void* CosmeticLoadout)
{
    static auto ApplyCharacterCustomization = (void (*)(APlayerState*, AFortPawn*, bool))(ImageBase + 0xA2C731C);

    ApplyCosmeticLoadoutOG(Controller, CosmeticLoadout);

    if (Controller->PlayerState && Controller->PlayerBotPawn)
        ApplyCharacterCustomization(Controller->PlayerState, nullptr, false);
}

static bool IsLateGameBot(AFortAthenaAIBotController* Controller)
{
    return bLateGame && Controller->bIsAnAthenaGameParticipant && Controller->PlayerBotPawn && Controller->Inventory.ObjectPointer;
}

static void GiveBotLateGameLoadout(AFortAthenaAIBotController* Controller)
{
    GiveLateGameLoadout((AFortInventory*)Controller->Inventory.ObjectPointer, Controller->PlayerBotPawn);
    RefreshSlotItems(Controller);
}

static bool (*RestartBotOG)(AFortAthenaAIBotController*, AActor*, const FVector*);
static bool RestartBot(AFortAthenaAIBotController* Controller, AActor* StartSpot, const FVector* Location)
{
    auto bRestarted = RestartBotOG(Controller, StartSpot, Location);

    if (bRestarted && IsLateGameBot(Controller))
    {
        PlaceLateGamePawn(Controller, Controller->PlayerBotPawn);
        GiveBotLateGameLoadout(Controller);
    }

    return bRestarted;
}

static void (*RespawnPlayerAfterDeathOG)(AFortAthenaAIBotController*, const FVector*, const FRotator*, float, bool);
static void RespawnPlayerAfterDeath(AFortAthenaAIBotController* Controller, const FVector* RespawnLocation, const FRotator* RespawnRotation, float RespawnCameraDist, bool bBeginSkydiving)
{
    RespawnPlayerAfterDeathOG(Controller, RespawnLocation, RespawnRotation, RespawnCameraDist, bBeginSkydiving);

    auto bAlive = (*(uint8*)(uint64(Controller) + 0xB74) & 0x10) != 0;

    if (bAlive && IsLateGameBot(Controller))
        GiveBotLateGameLoadout(Controller);
}

static float FRand()
{
    return float(rand() & 0x7FFF) * (1.f / 32767.f);
}

static void ThankBusDriver(AFortAthenaAIBotController* Controller)
{
    static auto FindAircraftComponent = (UFortControllerComponent_Aircraft* (*)(AActor*))(ImageBase + 0x22BE02C);

    auto AircraftComponent = FindAircraftComponent(Controller);
    auto PlayerState = (AFortPlayerStateAthena*)Controller->PlayerState;

    if (AircraftComponent && !*(bool*)(uint64(AircraftComponent) + 0x140) && PlayerState && PlayerState->IsA(AFortPlayerStateAthena::StaticClass()) && !PlayerState->bThankedBusDriver)
        PlayerState->bThankedBusDriver = true;
}

void EnterAircraft(AFortAthenaAIBotController* Controller, AFortAthenaAircraft* Aircraft)
{
    static auto FindAircraftComponent = (UFortControllerComponent_Aircraft* (*)(AActor*))(ImageBase + 0x22BE02C);
    static auto EnterAircraftComponent = (void (*)(UFortControllerComponent_Aircraft*, AFortAthenaAircraft*))(ImageBase + 0x965F1E0);
    static auto SetValueAsBool = (void (*)(UBlackboardComponent*, uint16, bool))(ImageBase + 0x77225C4);
    static auto SetHasEverSkydivedFromBusAndLanded = (void (*)(AFortPlayerStateAthena*, bool))(ImageBase + 0x5B8322C);

    auto Base = uint64(Controller);
    auto AircraftComponent = FindAircraftComponent(Controller);

    if (AircraftComponent && !AircraftComponent->CurrentAircraft && !*(bool*)(uint64(AircraftComponent) + 0x140))
        EnterAircraftComponent(AircraftComponent, Aircraft);

    auto Service = Controller->CachedAIServicePlayerBots;
    auto World = UWorld::GetWorld();

    if (Service && World && World->GameState && World->GameState->IsA(AFortGameStateAthena::StaticClass()) && Service->ThankBusDriverProbability.Evaluate() > FRand())
    {
        auto MaxTime = Service->ThankBusDriverMaxTime.Evaluate();
        auto MinTime = Service->ThankBusDriverMinTime.Evaluate();
        auto Delay = MinTime + FRand() * (MaxTime - MinTime);

        if (Delay > 0.f)
            SetActorTimer(Controller, (void*)ThankBusDriver, *(uint64*)(Base + 0xDF0), Delay);
    }

    if (!Controller->Blackboard)
        return;

    SetValueAsBool(Controller->Blackboard, *(uint16*)(Base + 0x146E), true);
    SetValueAsBool(Controller->Blackboard, *(uint16*)(Base + 0x1470), false);

    if (*(uint16*)(Base + 0x1472) != 0xFFFF)
        SetValueAsBool(Controller->Blackboard, *(uint16*)(Base + 0x1472), false);

    if (Controller->PlayerState && Controller->PlayerState->IsA(AFortPlayerStateAthena::StaticClass()))
        SetHasEverSkydivedFromBusAndLanded((AFortPlayerStateAthena*)Controller->PlayerState, false);
}

void ExitAircraft(AFortAthenaAIBotController* Controller)
{
    static auto GetAircraftIndex = (int32 (*)(UFortGameStateComponent_BattleRoyaleGamePhaseLogic*, APlayerState*))(ImageBase + 0x1E35610);
    static auto GetAircraft = (AFortAthenaAircraft* (*)(UFortGameStateComponent_BattleRoyaleGamePhaseLogic*, int32))(ImageBase + 0x1E34810);
    static auto FindAircraftComponent = (UFortControllerComponent_Aircraft* (*)(AActor*))(ImageBase + 0x22BE02C);
    static auto IsAircraftJumpAllowed = (bool (*)(UFortGameStateComponent_BattleRoyaleGamePhaseLogic*, AController*))(ImageBase + 0xA6EABB4);
    static auto KickFromAircraft = (void (*)(UFortControllerComponent_Aircraft*))(ImageBase + 0x9667574);

    auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(Controller);
    auto Aircraft = GamePhaseLogic ? GetAircraft(GamePhaseLogic, GetAircraftIndex(GamePhaseLogic, Controller->PlayerState)) : nullptr;
    auto AircraftComponent = FindAircraftComponent(Controller);

    if (AircraftComponent && AircraftComponent->CurrentAircraft && Controller->Blackboard && Aircraft && IsAircraftJumpAllowed(GamePhaseLogic, Controller))
        KickFromAircraft(AircraftComponent);
}

enum class EBotNavigationStage : uint8
{
    WaitingForRelease,
    WaitingForRegistration,
    Building,
    Done
};

static EBotNavigationStage BotNavigationStage = EBotNavigationStage::WaitingForRelease;
static UNavigationSystemBase* BotNavigationSystem = nullptr;
static int32 NavDataCountBeforeSpawn = 0;
static uint64 StageTime = 0;
static bool bBoundsReannounced = false;

void AFortAthenaAIBotController__Init()
{
    Hook(ImageBase + 0xA7713DC, ApplyCosmeticLoadout, ApplyCosmeticLoadoutOG);
    Hook(ImageBase + 0xA778F28, DropLootOnDeath);
    Hook(ImageBase + 0xA778D28, DropInventoryItems);
    Hook(ImageBase + 0xA78E59C, OnPawnDied, OnPawnDiedOG);
    Hook(ImageBase + 0xA78EDA4, OnPossesedPawnDied, OnPossesedPawnDiedOG);
    Hook(ImageBase + 0xA795F38, RestartBot, RestartBotOG);
    Hook(ImageBase + 0xA794F88, RespawnPlayerAfterDeath, RespawnPlayerAfterDeathOG);

    auto InventoryOwnerInterfaceVft = *(void***)(uint64(AFortAthenaAIBotController::GetDefaultObj()) + 0x3F0);

    InitializeInventoryOG = decltype(InitializeInventoryOG)(InventoryOwnerInterfaceVft[0x4]);
    Patch<void*>(uint64(&InventoryOwnerInterfaceVft[0x4]), (void*)InitializeInventory);
    Patch<void*>(uint64(&InventoryOwnerInterfaceVft[0x3]), (void*)CreateInventory);
    Patch<void*>(uint64(&InventoryOwnerInterfaceVft[0x3D]), (void*)(bool (*)(void*, FGuid&, int32, bool, bool))RemoveInventoryItem);
}

void AFortAthenaAIBotController__Tick()
{
    auto World = UWorld::GetWorld();
    auto NavigationSystem = World ? (UNavigationSystemV1*)World->NavigationSystem : nullptr;

    if (!NavigationSystem)
        return;

    if (NavigationSystem != BotNavigationSystem)
    {
        BotNavigationSystem = NavigationSystem;
        BotNavigationStage = EBotNavigationStage::WaitingForRelease;
        bBoundsReannounced = false;
    }

    if (BotNavigationStage == EBotNavigationStage::Done)
        return;

    auto AISystem = (UFortAISystem*)World->AISystem;

    if (!AISystem || !AISystem->IsA(UFortAISystem::StaticClass()) || !AISystem->AIServiceManager)
        return;

    auto& AIServices = AISystem->AIServiceManager->AIServices;
    bool bPlayerBotsRegistered = false;

    for (int32 i = 0; i < AIServices.Num(); i++)
        if (AIServices.Data[i] && AIServices.Data[i]->Class == UAthenaAIServicePlayerBots::StaticClass())
            bPlayerBotsRegistered = true;

    if (!bPlayerBotsRegistered)
        return;

    auto Now = GetTickCount64();
    auto& NavDataSet = NavigationSystem->NavDataSet;

    if (BotNavigationStage == EBotNavigationStage::WaitingForRelease)
    {
        if (NavDataSet.Num() > 0)
        {
            BotNavigationStage = EBotNavigationStage::Done;
            return;
        }

        if (UAthenaNavSystem::IsInitialNavigationLockActive(World) || (UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World) && !UNavigationSystemV1::IsNavigationBeingBuilt(World)))
            return;

        NavDataCountBeforeSpawn = NavDataSet.Num();

        ((void (*)(UNavigationSystemV1*))NavigationSystem->VTable[0x92])(NavigationSystem);

        BotNavigationStage = EBotNavigationStage::WaitingForRegistration;
        StageTime = Now;
        return;
    }

    if (BotNavigationStage == EBotNavigationStage::WaitingForRegistration)
    {
        if (NavDataSet.Num() > NavDataCountBeforeSpawn)
        {
            BotNavigationStage = EBotNavigationStage::Building;
            StageTime = Now;
        }
        else if (Now - StageTime >= 15000)
            BotNavigationStage = EBotNavigationStage::Done;

        return;
    }

    if (UNavigationSystemV1::IsNavigationBeingBuilt(World))
    {
        BotNavigationStage = EBotNavigationStage::Done;
        return;
    }

    if (!bBoundsReannounced && Now - StageTime >= 5000)
    {
        TArray<ANavMeshBoundsVolume*> Volumes;
        GetAll<ANavMeshBoundsVolume>(Volumes);

        for (int32 i = 0; i < Volumes.Num(); i++)
            if (Volumes.Data[i] && (*(uint32*)&Volumes.Data[i]->SupportedAgents & *(uint32*)&NavigationSystem->SupportedAgentsMask))
                NavigationSystem->OnNavigationBoundsUpdated(Volumes.Data[i]);

        Volumes.Free();

        bBoundsReannounced = true;
        StageTime = Now;
    }
    else if (bBoundsReannounced && Now - StageTime >= 10000)
        BotNavigationStage = EBotNavigationStage::Done;
}
