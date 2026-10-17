#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Player/FortPlayerController.h"
#include "FortniteGame/Source/FortniteAI/Public/AI/FortAthenaAIBotController.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortGameModeAthena.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortPickup.h"
#include "FortniteGame/Source/FortniteGame/Public/Quests/FortQuestManager.h"

INIT_POSTLOAD(AFortPlayerController);

static std::optional<UFortWorldItemDefinition*> CachedWID_Launcher_Petrol;
static std::optional<UClass*> CachedBGA_Petrol_PickupClass;
static std::optional<UClass*> CachedSprayAbilityClass;
static std::optional<UClass*> CachedEmoteAbilityClass;

static FFortItemEntry* FindEditToolEntry(AFortInventory* WorldInventory)
{
    for (auto& Entry : WorldInventory->Inventory.ReplicatedEntries)
    {
        if (Entry.ItemDefinition->Class == UFortEditToolItemDefinition::StaticClass())
            return &Entry;
    }

    return nullptr;
}

static void ServerExecuteInventoryItem(UObject* Context, FFrame& Stack)
{
    FGuid ItemGuid;

    Stack.StepCompiledIn(&ItemGuid);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;

    if (!PlayerController || !PlayerController->MyFortPawn)
        return;

    auto Entry = FindReplicatedEntry((AFortInventory*)PlayerController->WorldInventory.ObjectPointer, ItemGuid);

    if (!Entry)
        return;

    auto RealDef = (UFortItemDefinition*)Entry->ItemDefinition;
    auto Gadget = RealDef->IsA(UFortGadgetItemDefinition::StaticClass()) ? (UFortGadgetItemDefinition*)RealDef : nullptr;
    auto UncastedDef = Gadget ? Gadget->GetWeaponItemDefinition() : RealDef;

    if (!UncastedDef || !UncastedDef->IsA(UFortWeaponItemDefinition::StaticClass()))
        return;

    auto ItemDefinition = (UFortWeaponItemDefinition*)UncastedDef;
    auto Weapon = PlayerController->MyFortPawn->EquipWeaponDefinition(ItemDefinition, ItemGuid, Entry->TrackerGuid, false);

    if (!Weapon)
        return;

    if (Weapon->IsA(AFortDecoTool::StaticClass()))
    {
        ((AFortDecoTool*)Weapon)->SetDecoObjectPreview(ItemDefinition, true);

        if (Weapon->IsA(AFortDecoTool_ContextTrap::StaticClass()))
            ((AFortDecoTool_ContextTrap*)Weapon)->ContextTrapItemDefinition = (UFortContextTrapItemDefinition*)ItemDefinition;
    }

    Weapon->ForceNetUpdate();

    if (Gadget ? !Gadget->bValidForLastEquipped : !ItemDefinition->bValidForLastEquipped)
        return;

    *(FGuid*)(uint64(&PlayerController->MyFortPawn->VehicleInputComponent) - 0x20) = ItemGuid;
}

static void SetEditingPlayer(ABuildingSMActor* Building, AFortPlayerStateAthena* NewEditingPlayer)
{
    if (Building->Role != ENetRole::ROLE_Authority || (Building->EditingPlayer && NewEditingPlayer))
        return;

    Building->SetNetDormancy(NewEditingPlayer ? ENetDormancy::DORM_Awake : ENetDormancy::DORM_DormantAll);
    Building->ForceNetUpdate();

    auto EditingPlayer = Building->EditingPlayer ? Building->EditingPlayer : NewEditingPlayer;

    if (EditingPlayer && (!EditingPlayer->Owner || !EditingPlayer->Owner->IsA(AFortPlayerControllerAthena::StaticClass())))
        return;

    Building->EditingPlayer = NewEditingPlayer;
    Building->OnRep_EditingPlayer();
}

static void SetEditActor(AFortWeap_EditingTool* EditTool, ABuildingSMActor* EditActor)
{
    EditTool->EditActor = EditActor;
    EditTool->ForceNetUpdate();
    EditTool->OnRep_EditActor();
}

static void ServerCreateBuildingActor(UObject* Context, FFrame& Stack)
{
    FCreateBuildingActorData CreateBuildingData;

    Stack.StepCompiledIn(&CreateBuildingData);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;
    auto WorldInventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;
    auto BuildingClassIt = BuildingClassMap.find(CreateBuildingData.BuildingClassHandle);
    auto BuildingClass = BuildingClassIt != BuildingClassMap.end() ? BuildingClassIt->second : nullptr;
    FVector BuildLoc = CreateBuildingData.BuildLoc;
    FRotator BuildRot = CreateBuildingData.BuildRot;
    auto bMirrored = CreateBuildingData.bMirrored;
    auto BuildingClassData = CreateBuildingData.BuildingClassData;

    if (!BuildingClass)
        return;

    BuildingClassData.BuildingClass = BuildingClass;

    static auto CanAffordToPlaceBuildableClass = (bool (*)(AFortPlayerControllerAthena*, FBuildingClassData, bool))(ImageBase + 0xA21CED8);
    static auto CantBuild = (int64 (*)(UWorld*, TSubclassOf<AActor>&, FVector, FRotator, bool, TArray<ABuildingSMActor*>*, char*))(ImageBase + 0x9A95ED4);
    static auto PayBuildableClassPlacementCost = (int32 (*)(AFortPlayerControllerAthena*, FBuildingClassData))(ImageBase + 0xA24BD50);

    if (!CanAffordToPlaceBuildableClass(PlayerController, BuildingClassData, false))
        return;

    TSubclassOf<AActor> BuildingActorClass = BuildingClass;
    TArray<ABuildingSMActor*> BuildingsToRemove;
    char bCanBuild;

    if (CantBuild(UWorld::GetWorld(), BuildingActorClass, BuildLoc, BuildRot, bMirrored, &BuildingsToRemove, &bCanBuild))
        return;

    for (auto BuildingToRemove : BuildingsToRemove)
        BuildingToRemove->K2_DestroyActor();

    BuildingsToRemove.Free();

    auto Building = SpawnActorUnfinished<ABuildingSMActor>(BuildingClass, BuildLoc, BuildRot, PlayerController);

    if (!Building)
        return;

    Building->InitializeKismetSpawnedBuildingActor(Building, PlayerController, true, nullptr, false);
    FinishSpawnActor(Building, BuildLoc, BuildRot);

    auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;

    Building->CurrentBuildingLevel = BuildingClassData.UpgradeLevel;
    Building->OnRep_CurrentBuildingLevel();
    Building->SetMirrored(bMirrored);
    Building->bPlayerPlaced = true;
    Building->Team = (EFortTeam)PlayerState->TeamIndex;
    Building->TeamIndex = (uint8)Building->Team;

    if (!PlayerController->bBuildFree)
        PayBuildableClassPlacementCost(PlayerController, BuildingClassData);

    SendStatEvent(PlayerController->GetQuestManager(ESubGame::Athena), PlayerController, EFortQuestObjectiveStatEvent::Build, 1, true);
}

static void ServerBeginEditingBuildingActor(UObject* Context, FFrame& Stack)
{
    ABuildingSMActor* BuildingActorToEdit;

    Stack.StepCompiledIn(&BuildingActorToEdit);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;

    if (!PlayerController || !PlayerController->MyFortPawn || !BuildingActorToEdit || !BuildingActorToEdit->IsA(ABuildingSMActor::StaticClass()))
        return;

    auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;

    if (!PlayerState)
        return;

    SetEditingPlayer(BuildingActorToEdit, PlayerState);

    auto Pawn = PlayerController->MyFortPawn;

    if (!Pawn->CurrentWeapon || !Pawn->CurrentWeapon->IsA(AFortWeap_EditingTool::StaticClass()))
    {
        if (auto EditToolEntry = FindEditToolEntry((AFortInventory*)PlayerController->WorldInventory.ObjectPointer))
            Pawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)EditToolEntry->ItemDefinition, EditToolEntry->ItemGuid, EditToolEntry->TrackerGuid, false);
    }

    if (Pawn->CurrentWeapon && Pawn->CurrentWeapon->IsA(AFortWeap_EditingTool::StaticClass()))
        SetEditActor((AFortWeap_EditingTool*)Pawn->CurrentWeapon, BuildingActorToEdit);
}

static void ServerEditBuildingActor(UObject* Context, FFrame& Stack)
{
    ABuildingSMActor* BuildingActorToEdit;
    TSubclassOf<ABuildingSMActor> NewBuildingClass;
    uint8 RotationIterations;
    bool bMirrored;

    Stack.StepCompiledIn(&BuildingActorToEdit);
    Stack.StepCompiledIn(&NewBuildingClass);
    Stack.StepCompiledIn(&RotationIterations);
    Stack.StepCompiledIn(&bMirrored);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;

    if (!PlayerController || !BuildingActorToEdit || !NewBuildingClass || !BuildingActorToEdit->IsA(ABuildingSMActor::StaticClass()) || !NewBuildingClass->IsSubclassOf(ABuildingSMActor::StaticClass()) ||
        !((ABuildingSMActor*)NewBuildingClass->ClassDefaultObject)->bIsPlayerBuildable ||
        BuildingActorToEdit->EditingPlayer != PlayerController->PlayerState || BuildingActorToEdit->bDestroyed)
        return;

    SetEditingPlayer(BuildingActorToEdit, nullptr);

    static auto ReplaceBuildingActor = (ABuildingSMActor* (*)(ABuildingSMActor*, uint32, TSubclassOf<ABuildingSMActor>&, uint32, int32, bool, AFortPlayerControllerAthena*))(ImageBase + 0x9538A78);

    if (auto NewBuild = ReplaceBuildingActor(BuildingActorToEdit, 1, NewBuildingClass, BuildingActorToEdit->CurrentBuildingLevel, RotationIterations, bMirrored, PlayerController))
        NewBuild->bPlayerPlaced = true;
}

static void ServerEndEditingBuildingActor(UObject* Context, FFrame& Stack)
{
    ABuildingSMActor* BuildingActorToStopEditing;

    Stack.StepCompiledIn(&BuildingActorToStopEditing);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;

    if (!PlayerController || !PlayerController->MyFortPawn || !BuildingActorToStopEditing || !BuildingActorToStopEditing->IsA(ABuildingSMActor::StaticClass()) ||
        BuildingActorToStopEditing->EditingPlayer != PlayerController->PlayerState || BuildingActorToStopEditing->bDestroyed)
        return;

    SetEditingPlayer(BuildingActorToStopEditing, nullptr);

    auto EditToolEntry = FindEditToolEntry((AFortInventory*)PlayerController->WorldInventory.ObjectPointer);

    if (!EditToolEntry)
        return;

    for (auto Weapon : PlayerController->MyFortPawn->CurrentWeaponList)
    {
        if (Weapon->ItemEntryGuid == EditToolEntry->ItemGuid)
        {
            SetEditActor((AFortWeap_EditingTool*)Weapon, nullptr);
            break;
        }
    }
}

static void ServerRepairBuildingActor(UObject* Context, FFrame& Stack)
{
    ABuildingSMActor* BuildingActorToRepair;

    Stack.StepCompiledIn(&BuildingActorToRepair);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;

    if (!PlayerController || !BuildingActorToRepair || !BuildingActorToRepair->IsA(ABuildingSMActor::StaticClass()))
        return;

    auto WorldInventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;
    auto Price = (int32)std::floor((10.f * (1.f - BuildingActorToRepair->GetHealthPercent())) * 0.75f);
    auto ResourceItemDefinition = UFortKismetLibrary::K2_GetResourceItemDefinition(BuildingActorToRepair->ResourceType);
    auto Item = FindItemInstance(WorldInventory, ResourceItemDefinition);
    auto ItemEntry = FindReplicatedEntry(WorldInventory, ResourceItemDefinition);

    if (!Item || ItemEntry->Count - Price < 0)
        return;

    ItemEntry->Count -= Price;
    UpdateOrRemoveEntry(WorldInventory, Item, *ItemEntry);

    BuildingActorToRepair->RepairBuilding(PlayerController, Price);
}

static void ServerAttemptInventoryDrop(UObject* Context, FFrame& Stack)
{
    FGuid ItemGuid;
    int32 Count;
    bool bTrash;

    Stack.StepCompiledIn(&ItemGuid);
    Stack.StepCompiledIn(&Count);
    Stack.StepCompiledIn(&bTrash);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;

    if (!PlayerController || !PlayerController->Pawn)
        return;

    auto WorldInventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;
    auto Pawn = PlayerController->Pawn;
    auto Item = FindItemInstance(WorldInventory, ItemGuid);
    auto ItemEntry = FindReplicatedEntry(WorldInventory, ItemGuid);

    if (!Item || !ItemEntry)
        return;

    if (Count <= 0 || Count > ItemEntry->Count)
        Count = ItemEntry->Count;

    ItemEntry->Count -= Count;

    auto FinalLoc = Pawn->K2_GetActorLocation();

    auto WID_Launcher_Petrol = FindCachedObject(CachedWID_Launcher_Petrol, L"/Game/Athena/Items/Weapons/Prototype/WID_Launcher_Petrol.WID_Launcher_Petrol");

    if (ItemEntry->ItemDefinition == WID_Launcher_Petrol)
    {
        auto BGA_Petrol_PickupClass = FindCachedObject(CachedBGA_Petrol_PickupClass, L"/Game/Athena/Items/Weapons/Prototype/PetrolPump/BGA_Petrol_Pickup.BGA_Petrol_Pickup_C");

        SpawnActor<AActor>(BGA_Petrol_PickupClass, FinalLoc);
        Remove(WorldInventory, ItemGuid);
        return;
    }

    FinalLoc = FinalLoc + Pawn->GetActorForwardVector() * 450.f;
    FinalLoc.Z += 50.f;

    auto RandomAngleVariation = ((float)rand() * 0.00109866634f) - 18.f;
    auto FinalAngle = (RandomAngleVariation + 360.f / ((float)rand() * 0.00015259254737998596f)) * 0.017453292519943295f;

    FinalLoc.X += cos(FinalAngle) * 100.f;
    FinalLoc.Y += sin(FinalAngle) * 100.f;

    SpawnPickup(Pawn->K2_GetActorLocation() + Pawn->GetActorForwardVector() * 70.f + FVector(0, 0, 50), *ItemEntry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, PlayerController->MyFortPawn,
                Count, true, true, true, nullptr, FinalLoc);

    UpdateOrRemoveEntry(WorldInventory, Item, *ItemEntry);
}

static void ServerPlayEmoteItem(UObject* Context, FFrame& Stack)
{
    UFortMontageItemDefinitionBase* EmoteAsset;
    float EmoteRandomNumber;

    Stack.StepCompiledIn(&EmoteAsset);
    Stack.StepCompiledIn(&EmoteRandomNumber);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;

    if (!PlayerController || !PlayerController->MyFortPawn || !EmoteAsset)
        return;

    auto Pawn = (AFortPlayerPawnAthena*)PlayerController->MyFortPawn;
    auto AbilitySystemComponent = ((AFortPlayerStateAthena*)PlayerController->PlayerState)->AbilitySystemComponent;
    auto QuestManager = PlayerController->GetQuestManager(ESubGame::Athena);
    UObject* AbilityToUse = nullptr;

    if (PlayerController->Pawn->IsA(AFortCharacterVehicle::StaticClass()))
        AbilitySystemComponent = ((AFortCharacterVehicle*)PlayerController->Pawn)->OverrideAbilitySystemComponent;

    if (EmoteAsset->IsA(UAthenaSprayItemDefinition::StaticClass()))
    {
        auto SprayAbilityClass = FindCachedObject(CachedSprayAbilityClass, L"/Game/Abilities/Sprays/GAB_Spray_Generic.GAB_Spray_Generic_C");
        AbilityToUse = SprayAbilityClass->ClassDefaultObject;

        SendStatEvent(QuestManager, PlayerController, EFortQuestObjectiveStatEvent::Spray, 1, true);
    }
    else if (EmoteAsset->IsA(UAthenaToyItemDefinition::StaticClass()))
    {
        auto ToySpawnAbility = ((UAthenaToyItemDefinition*)EmoteAsset)->ToySpawnAbility.Get();
        AbilityToUse = ToySpawnAbility ? ToySpawnAbility->ClassDefaultObject : nullptr;

        SendStatEvent(QuestManager, PlayerController, EFortQuestObjectiveStatEvent::Toy, 1, true);
    }
    else if (EmoteAsset->IsA(UAthenaDanceItemDefinition::StaticClass()))
    {
        auto EmoteAbilityClass = FindCachedObject(CachedEmoteAbilityClass, L"/Game/Abilities/Emotes/GAB_Emote_Generic.GAB_Emote_Generic_C");

        auto DanceAsset = (UAthenaDanceItemDefinition*)EmoteAsset;
        auto CustomAbility = DanceAsset->CustomDanceAbility.Get();

        Pawn->bMovingEmote = DanceAsset->bMovingEmote;
        Pawn->EmoteWalkSpeed = DanceAsset->WalkForwardSpeed;
        Pawn->bMovingEmoteForwardOnly = DanceAsset->bMoveForwardOnly;
        Pawn->bMovingEmoteFollowingOnly = DanceAsset->bMoveFollowingOnly;
        AbilityToUse = (CustomAbility ? CustomAbility : EmoteAbilityClass)->ClassDefaultObject;

        SendStatEvent(QuestManager, PlayerController, EFortQuestObjectiveStatEvent::Emote, 1, true);
    }

    if (!AbilityToUse)
        return;

    static auto ConstructAbilitySpec = (void (*)(FGameplayAbilitySpec*, const UObject*, int, int, UObject*))(ImageBase + 0x7CF6DD0);
    static auto GiveAbilityAndActivateOnce = (void (*)(UAbilitySystemComponent*, FGameplayAbilitySpecHandle*, FGameplayAbilitySpec*, void*))(ImageBase + 0x7CBA17C);

    auto Spec = (FGameplayAbilitySpec*)calloc(1, sizeof(FGameplayAbilitySpec));
    FGameplayAbilitySpecHandle Handle;

    ConstructAbilitySpec(Spec, AbilityToUse, 1, -1, EmoteAsset);
    GiveAbilityAndActivateOnce(AbilitySystemComponent, &Handle, Spec, nullptr);

    free(Spec);

    auto OldEmote = Pawn->LastReplicatedEmoteExecuted;
    Pawn->LastReplicatedEmoteExecuted = EmoteAsset;
    Pawn->OnRep_LastReplicatedEmoteExecuted(OldEmote);
}

void InternalPickup(AController* Controller, FFortItemEntry* PickupEntry)
{
    if (!PickupEntry || !PickupEntry->ItemDefinition)
        return;

    auto PlayerController = Controller->IsA(AFortPlayerControllerAthena::StaticClass()) ? (AFortPlayerControllerAthena*)Controller : nullptr;
    auto WorldInventory = (AFortInventory*)(PlayerController ? PlayerController->WorldInventory.ObjectPointer : ((AFortAthenaAIBotController*)Controller)->Inventory.ObjectPointer);
    auto Pawn = Controller->Pawn;
    auto MyFortPawn = PlayerController ? PlayerController->MyFortPawn : (AFortPawn*)Pawn;
    auto PickupDefinition = (UFortItemDefinition*)PickupEntry->ItemDefinition;
    auto MaxStack = PickupDefinition->GetMaxStackSize(nullptr);
    int ItemCount = 0;

    for (auto Instance : WorldInventory->Inventory.ItemInstances)
    {
        if (TakesQuickbarSlot((UFortItemDefinition*)Instance->ItemEntry.ItemDefinition))
            ItemCount += Instance->GetNumberOfSlotsTaken();
    }

    auto FinalLoc = Pawn ? Pawn->K2_GetActorLocation() : FVector();
    auto ForwardVector = Pawn ? Pawn->GetActorForwardVector() : FVector();

    ForwardVector.Z = 0.0f;
    ForwardVector.Normalize();

    FinalLoc = FinalLoc + ForwardVector * 450.f;
    FinalLoc.Z += 50.f;

    auto FinalAngle = (((float)rand() * 0.00109866634f) - 18.f) * 0.017453292519943295f;

    FinalLoc.X += cos(FinalAngle) * 100.f;
    FinalLoc.Y += sin(FinalAngle) * 100.f;

    auto SendCollectStat = [&](int Count)
    {
        if (!PlayerController)
            return;

        FGameplayTagContainer TargetTags{};
        GetOwnedGameplayTags(PickupDefinition, TargetTags);

        SendStatEvent(PlayerController->GetQuestManager(ESubGame::Athena), PlayerController, EFortQuestObjectiveStatEvent::Collect, Count, true, PickupDefinition, TargetTags);

        FreeGameplayTags(TargetTags);
    };

    auto DropPickup = [&](FFortItemEntry& Entry, int Count)
    {
        SpawnPickup(Pawn->K2_GetActorLocation() + Pawn->GetActorForwardVector() * 70.f + FVector(0, 0, 50), Entry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, MyFortPawn, Count, true, true, true, nullptr, FinalLoc);
    };

    auto GiveOrSwap = [&]()
    {
        if (ItemCount >= 5 && TakesQuickbarSlot(PickupDefinition))
        {
            if (!MyFortPawn || !MyFortPawn->CurrentWeapon)
                return;

            if (!IsPrimaryQuickbar(MyFortPawn->CurrentWeapon->WeaponData))
            {
                DropPickup(*PickupEntry, -1);
                return;
            }

            for (auto& Entry : WorldInventory->Inventory.ReplicatedEntries)
            {
                if (Entry.ItemGuid == MyFortPawn->CurrentWeapon->ItemEntryGuid)
                {
                    DropPickup(Entry, -1);
                    break;
                }
            }

            auto Item = GiveItem(WorldInventory, *PickupEntry, PickupEntry->Count, true);

            if (PlayerController)
            {
                auto& SlotItemGuids = PlayerController->PrimaryQuickBarSlotItemGuids;
                auto NumEnabledSlots = (std::min)(SlotItemGuids.NumEnabledSlots, (int32)std::size(SlotItemGuids.EquippedItemGuids));
                auto NumSlots = (int32)Item->GetNumberOfSlotsTaken();
                int32 Slot = 0;

                while (Slot < NumEnabledSlots && !(SlotItemGuids.EquippedItemGuids[Slot] == MyFortPawn->CurrentWeapon->ItemEntryGuid))
                    Slot++;

                auto FirstSlot = Slot < NumEnabledSlots ? (std::min)(Slot, NumEnabledSlots - NumSlots) : -1;

                for (int32 i = FirstSlot; i >= 0 && i < FirstSlot + NumSlots; i++)
                {
                    auto CoveredEntry = FindReplicatedEntry(WorldInventory, SlotItemGuids.EquippedItemGuids[i]);
                    auto CoveredDefinition = CoveredEntry ? (UFortItemDefinition*)CoveredEntry->ItemDefinition : nullptr;

                    if (!CoveredDefinition || CoveredEntry->ItemGuid == MyFortPawn->CurrentWeapon->ItemEntryGuid || !TakesQuickbarSlot(CoveredDefinition) || !CanBeDropped(CoveredDefinition))
                        continue;

                    DropPickup(*CoveredEntry, -1);
                    Remove(WorldInventory, CoveredEntry->ItemGuid);
                }
            }

            Remove(WorldInventory, MyFortPawn->CurrentWeapon->ItemEntryGuid);

            if (PlayerController)
            {
                PlayerController->ServerExecuteInventoryItem(Item->ItemEntry.ItemGuid);
                PlayerController->ClientEquipItem(Item->ItemEntry.ItemGuid, true);
            }
            else
            {
                static auto EquipBestWeapon = (void (*)(AFortAthenaAIBotController*, bool))(ImageBase + 0xA779BA0);

                RefreshSlotItems((AFortAthenaAIBotController*)Controller);
                EquipBestWeapon((AFortAthenaAIBotController*)Controller, true);
            }
        }
        else
            GiveItem(WorldInventory, *PickupEntry, PickupEntry->Count, true);

        SendCollectStat(PickupEntry->Count);
    };

    if (MaxStack <= 1)
        return GiveOrSwap();

    UFortWorldItem* Item = nullptr;
    FFortItemEntry* ItemEntry = nullptr;
    FFortItemEntry* FullItemEntry = nullptr;

    for (auto Instance : WorldInventory->Inventory.ItemInstances)
    {
        if (Instance->ItemEntry.ItemDefinition == PickupEntry->ItemDefinition && Instance->ItemEntry.Count < MaxStack)
        {
            Item = Instance;
            break;
        }
    }

    for (auto& Entry : WorldInventory->Inventory.ReplicatedEntries)
    {
        if (Entry.ItemDefinition != PickupEntry->ItemDefinition)
            continue;

        if (!ItemEntry && Entry.Count < MaxStack)
            ItemEntry = &Entry;
        else if (!FullItemEntry && Entry.Count >= MaxStack)
            FullItemEntry = &Entry;
    }

    if (!Item)
    {
        if (!ItemEntry && FullItemEntry)
        {
            DropPickup(*PickupEntry, PickupEntry->Count);
            return;
        }

        if (PickupEntry->Count > MaxStack)
        {
            auto OriginalCount = PickupEntry->Count;
            PickupEntry->Count = MaxStack;

            DropPickup(*PickupEntry, OriginalCount - MaxStack);
        }

        return GiveOrSwap();
    }

    auto OriginalItemCount = ItemEntry->Count;

    if ((ItemEntry->Count += PickupEntry->Count) > MaxStack)
    {
        auto OriginalCount = ItemEntry->Count;
        ItemEntry->Count = MaxStack;

        DropPickup(*PickupEntry, OriginalCount - MaxStack);
    }

    bool bFound = false;

    for (auto& StateValue : ItemEntry->StateValues)
    {
        if (StateValue.StateType == EFortItemEntryState::ShouldShowItemToast)
        {
            StateValue.IntValue = 1;
            bFound = true;
            break;
        }
    }

    if (!bFound)
    {
        FFortItemEntryStateValue Value{};
        Value.IntValue = 1;
        Value.StateType = EFortItemEntryState::ShouldShowItemToast;
        ItemEntry->StateValues.Add(Value);
    }

    SendCollectStat(ItemEntry->Count - OriginalItemCount);
    Item->ItemEntry.Count = ItemEntry->Count;
    UpdateEntry(WorldInventory, *ItemEntry);
}

static void ServerDropAllItems(UObject* Context, FFrame& Stack)
{
    UFortItemDefinition* IgnoreItemDef;

    Stack.StepCompiledIn(&IgnoreItemDef);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;
    auto WorldInventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;

    if (!WorldInventory || !PlayerController->MyFortPawn)
        return;

    auto Loc = PlayerController->MyFortPawn->K2_GetActorLocation();
    std::vector<FGuid> GuidsToRemove;

    for (auto& Entry : WorldInventory->Inventory.ReplicatedEntries)
    {
        if (Entry.ItemDefinition == IgnoreItemDef || !CanBeDropped((UFortItemDefinition*)Entry.ItemDefinition))
            continue;

        SpawnPickup(Loc, Entry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, PlayerController->MyFortPawn);
        GuidsToRemove.push_back(Entry.ItemGuid);
    }

    for (auto& Guid : GuidsToRemove)
        Remove(WorldInventory, Guid);
}

static void SpawnToyInstance(UObject* Context, FFrame& Stack, AActor** Ret)
{
    TSubclassOf<AActor> ToyClass;
    FTransform SpawnPosition;

    Stack.StepCompiledIn(&ToyClass);
    Stack.StepCompiledIn(&SpawnPosition);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;
    auto Toy = SpawnActor<AActor>(ToyClass, SpawnPosition, PlayerController);

    PlayerController->ActiveToyInstances.Add(Toy);

    *Ret = Toy;
}

static void ServerCraftSchematic(UObject* Context, FFrame& Stack)
{
    FString ItemId;
    int32 PostCraftSlot;
    int32 CraftAmount;
    EFortItemTier RequestedTier;
    bool bIsQuickCrafted;

    Stack.StepCompiledIn(&ItemId);
    Stack.StepCompiledIn(&PostCraftSlot);
    Stack.StepCompiledIn(&CraftAmount);
    Stack.StepCompiledIn(&RequestedTier);
    Stack.StepCompiledIn(&bIsQuickCrafted);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;
    auto SchematicName = ItemId.ToString();
    ItemId.Free();

    if (SchematicName.rfind("Schematic:", 0) == 0)
        SchematicName.erase(0, 10);

    auto Schematic = UObject::FindObjectFast<UFortSchematicItemDefinition>(SchematicName);
    auto ResultItemDef = Schematic ? Schematic->GetResultWorldItemDefinition() : nullptr;

    if (!ResultItemDef)
        return;

    auto NewEntry = MakeItemEntry(ResultItemDef, Schematic->GetQuantityProduced(), 0);

    InternalPickup(PlayerController, NewEntry);
    free(NewEntry);
}

static void ServerOnMaterialSelection(UObject* Context, FFrame& Stack)
{
    EFortResourceType NewResourceType;
    EFortResourceLevel NewResourceLevel;

    Stack.StepCompiledIn(&NewResourceType);
    Stack.StepCompiledIn(&NewResourceLevel);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;

    PlayerController->CurrentResourceType = NewResourceType;
    PlayerController->CurrentResourceLevel = NewResourceLevel;
}

static bool RemoveInventoryItem(void* Interface, FGuid& ItemGuid, int32 Count, bool bForceRemoval, bool bForcePersistWhenEmpty)
{
    auto PlayerController = (AFortPlayerControllerAthena*)(__int64(Interface) - 0x878);

    return RemoveInventoryItem((AFortInventory*)PlayerController->WorldInventory.ObjectPointer, ItemGuid, Count, bForceRemoval, bForcePersistWhenEmpty);
}

void AFortPlayerController__PostLoad()
{
    auto PlayerControllerClass = AFortPlayerControllerAthena::StaticClass();

    ExecHook(PlayerControllerClass->FindFunction("ServerExecuteInventoryItem"), ServerExecuteInventoryItem);
    ExecHook(PlayerControllerClass->FindFunction("ServerCreateBuildingActor"), ServerCreateBuildingActor);
    ExecHook(PlayerControllerClass->FindFunction("ServerBeginEditingBuildingActor"), ServerBeginEditingBuildingActor);
    ExecHook(PlayerControllerClass->FindFunction("ServerEditBuildingActor"), ServerEditBuildingActor);
    ExecHook(PlayerControllerClass->FindFunction("ServerEndEditingBuildingActor"), ServerEndEditingBuildingActor);
    ExecHook(PlayerControllerClass->FindFunction("ServerRepairBuildingActor"), ServerRepairBuildingActor);
    ExecHook(PlayerControllerClass->FindFunction("ServerAttemptInventoryDrop"), ServerAttemptInventoryDrop);
    ExecHook(PlayerControllerClass->FindFunction("ServerPlayEmoteItem"), ServerPlayEmoteItem);
    ExecHook(PlayerControllerClass->FindFunction("ServerDropAllItems"), ServerDropAllItems);
    ExecHook(PlayerControllerClass->FindFunction("SpawnToyInstance"), SpawnToyInstance);
    ExecHook(PlayerControllerClass->FindFunction("ServerCraftSchematic"), ServerCraftSchematic);
    ExecHook(PlayerControllerClass->FindFunction("ServerOnMaterialSelection"), ServerOnMaterialSelection);
    Hook(ImageBase + 0xA251E20, (bool (*)(void*, FGuid&, int32, bool, bool))RemoveInventoryItem);
}
