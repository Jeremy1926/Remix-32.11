#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"
#include <objbase.h>

static bool IsValidGuid(const FGuid& Guid)
{
    return Guid.A != 0 || Guid.B != 0 || Guid.C != 0 || Guid.D != 0;
}

UFortWorldItem* FindItemInstance(AFortInventory* Inventory, const FGuid& ItemGuid)
{
    for (auto Instance : Inventory->Inventory.ItemInstances)
    {
        if (Instance->ItemEntry.ItemGuid == ItemGuid)
            return Instance;
    }

    return nullptr;
}

UFortWorldItem* FindItemInstance(AFortInventory* Inventory, const UFortItemDefinition* ItemDefinition)
{
    for (auto Instance : Inventory->Inventory.ItemInstances)
    {
        if (Instance->ItemEntry.ItemDefinition == ItemDefinition)
            return Instance;
    }

    return nullptr;
}

FFortItemEntry* FindReplicatedEntry(AFortInventory* Inventory, const FGuid& ItemGuid)
{
    for (auto& ReplicatedEntry : Inventory->Inventory.ReplicatedEntries)
    {
        if (ReplicatedEntry.ItemGuid == ItemGuid)
            return &ReplicatedEntry;
    }

    return nullptr;
}

FFortItemEntry* FindReplicatedEntry(AFortInventory* Inventory, const UFortItemDefinition* ItemDefinition)
{
    for (auto& ReplicatedEntry : Inventory->Inventory.ReplicatedEntries)
    {
        if (ReplicatedEntry.ItemDefinition == ItemDefinition)
            return &ReplicatedEntry;
    }

    return nullptr;
}

static void AssignItemEntry(FFortItemEntry* Entry, const FFortItemEntry* Source)
{
    static auto AssignItemEntry = (FFortItemEntry* (*)(FFortItemEntry*, const FFortItemEntry*))(ImageBase + 0x1BC5450);

    AssignItemEntry(Entry, Source);
}

template <typename T>
static void CopyRawArray(TArray<T>& Array, const TArray<T>& Source)
{
    if (Source.Num() <= 0)
        return;

    if (Array.Data)
        FMemory::Free(Array.Data);

    Array.Data = nullptr;

    if (Source.Data)
    {
        Array.Data = (T*)FMemory::Malloc(uint64(Source.Num()) * sizeof(T));
        memcpy(Array.Data, Source.Data, uint64(Source.Num()) * sizeof(T));
    }

    Array.NumElements = Array.MaxElements = Source.Num();
}

void SetStateValueConstObject(FFortItemEntry& Entry, const FGameplayTag& StateType, const UObject* Object)
{
    auto& ConstObjects = Entry.StateValuesConstObject;

    for (auto& ConstObject : ConstObjects)
    {
        if (ConstObject.StateType.TagName.ComparisonIndex == StateType.TagName.ComparisonIndex)
        {
            ConstObject.ObjectData = TWeakObjectPtr<UObject>((UObject*)Object);
            return;
        }
    }

    FFortItemEntryStateValueConstObject NewConstObject{};

    NewConstObject.StateType.TagName.ComparisonIndex = StateType.TagName.ComparisonIndex;
    NewConstObject.ObjectData = TWeakObjectPtr<UObject>((UObject*)Object);
    ConstObjects.Add(NewConstObject);
}

void CopyWeaponModsFrom(FFortItemEntry& Entry, const FFortItemEntry& Source)
{
    if (&Source == &Entry)
        return;

    CopyRawArray(Entry.WeaponModSlots, Source.WeaponModSlots);
    CopyRawArray(Entry.StateValuesConstObject, Source.StateValuesConstObject);
}

static void InitializeWeaponModSlots(FFortItemEntry& Entry, const UFortWeaponItemDefinition* WeaponDefinition)
{
    auto& WeaponModSlots = Entry.WeaponModSlots;

    if (!WeaponDefinition || WeaponModSlots.Num() > 0)
        return;

    auto DefaultWeaponModSlots = WeaponDefinition->GetWeaponModSlots();

    if (DefaultWeaponModSlots.Num() <= 0)
    {
        DefaultWeaponModSlots.Free();
        return;
    }

    if (WeaponModSlots.Data)
        FMemory::Free(WeaponModSlots.Data);

    WeaponModSlots.Data = DefaultWeaponModSlots.Data;
    WeaponModSlots.NumElements = DefaultWeaponModSlots.Num();
    WeaponModSlots.MaxElements = DefaultWeaponModSlots.Max();
}

bool CanBeDropped(const UFortItemDefinition* ItemDefinition)
{
    static auto PickupDataStruct = FindObject<UScriptStruct>(L"/Script/FortniteGame.FortItemComponentData_Pickup");

    for (auto& ItemData : ItemDefinition->DataList)
    {
        if (ItemData.ScriptStruct == PickupDataStruct)
            return ((FFortItemComponentData_Pickup*)ItemData.StructMemory)->bCanBeDroppedFromInventory;
    }

    return false;
}

UFortWorldItem* GiveItem(AFortInventory* Inventory, const UFortItemDefinition* ItemDefinition, int Count, int LoadedAmmo, int Level, bool bNotifyPlayer, bool bUpdateInventory, int PhantomReserveAmmo,
                         TArray<FFortItemEntryStateValue> StateValues, const FFortItemEntry* SourceEntry)
{
    if (!Inventory || !ItemDefinition || !Count)
        return nullptr;

    auto MaxStack = ItemDefinition->GetMaxStackSize(nullptr);

    for (int i = 0; MaxStack > 1 && Count > 0 && i < Inventory->Inventory.ReplicatedEntries.Num(); i++)
    {
        auto& ReplicatedEntry = Inventory->Inventory.ReplicatedEntries[i];

        if (ReplicatedEntry.ItemDefinition != ItemDefinition || ReplicatedEntry.Count >= MaxStack)
            continue;

        auto Instance = FindItemInstance(Inventory, ReplicatedEntry.ItemGuid);

        if (!Instance)
            continue;

        auto Added = (std::min)(Count, MaxStack - ReplicatedEntry.Count);

        ReplicatedEntry.Count += Added;
        Instance->ItemEntry.Count = ReplicatedEntry.Count;
        Count -= Added;

        if (bUpdateInventory)
        {
            UpdateEntry(Inventory, ReplicatedEntry);
            Instance->ItemEntry.bIsDirty = true;
        }

        if (Count <= 0)
            return Instance;
    }

    auto Item = (UFortWorldItem*)ItemDefinition->CreateTemporaryItemInstanceBP(Count, Level);

    if (Inventory->Owner && Inventory->Owner->IsA(AFortPlayerController::StaticClass()))
        Item->SetOwningControllerForTemporaryItem((AFortPlayerController*)Inventory->Owner);

    Item->ItemEntry.ParentInventory = Inventory;
    Item->ItemEntry.LoadedAmmo = LoadedAmmo;
    Item->ItemEntry.PhantomReserveAmmo = PhantomReserveAmmo;

    if (SourceEntry)
        CopyWeaponModsFrom(Item->ItemEntry, *SourceEntry);

    if (ItemDefinition->IsA(UFortWeaponItemDefinition::StaticClass()))
        InitializeWeaponModSlots(Item->ItemEntry, (UFortWeaponItemDefinition*)ItemDefinition);

    if (StateValues.Num() > 0)
    {
        auto NewData = FMemory::Malloc(sizeof(FFortItemEntryStateValue) * StateValues.Num());
        memcpy(NewData, StateValues.Data, sizeof(FFortItemEntryStateValue) * StateValues.Num());
        Item->ItemEntry.StateValues.NumElements = StateValues.Num();
        Item->ItemEntry.StateValues.MaxElements = StateValues.Num();
        Item->ItemEntry.StateValues.Data = (FFortItemEntryStateValue*)NewData;
    }

    if (!IsValidGuid(Item->ItemEntry.ItemGuid))
    {
        CoCreateGuid((GUID*)&Item->ItemEntry.ItemGuid);

        if (!IsValidGuid(Item->ItemEntry.TrackerGuid))
            CoCreateGuid((GUID*)&Item->ItemEntry.TrackerGuid);
    }

    static auto ConstructItemEntry = (FFortItemEntry* (*)(FFortItemEntry*))(ImageBase + 0x1BC4DB4);

    auto& ReplicatedEntries = Inventory->Inventory.ReplicatedEntries;
    ReplicatedEntries.Reserve(1);

    auto& ReplicatedEntry = *ConstructItemEntry(&ReplicatedEntries.Data[ReplicatedEntries.NumElements++]);
    AssignItemEntry(&ReplicatedEntry, &Item->ItemEntry);
    ReplicatedEntry.ParentInventory = Item->ItemEntry.ParentInventory;
    ReplicatedEntry.bIsReplicatedCopy = true;
    Inventory->Inventory.ItemInstances.Add(Item);

    if (bUpdateInventory)
    {
        Inventory->bRequiresLocalUpdate = true;
        Inventory->bRequiresSaving = true;

        Inventory->HandleInventoryLocalUpdate();

        ReplicatedEntry.bIsDirty = false;
        Inventory->Inventory.MarkItemDirty(ReplicatedEntry);
        Inventory->ForceNetUpdate();
        Item->ItemEntry.bIsDirty = true;
    }

    ((bool (*)(const UFortWorldItem*, const void*))Item->VTable[0xB2])(Item, Inventory->Owner->GetInterface(IFortInventoryOwnerInterface::StaticClass()));

    return Item;
}

UFortWorldItem* GiveItem(AFortInventory* Inventory, FFortItemEntry& Entry, int Count, bool bNotifyPlayer, bool bUpdateInventory)
{
    if (Count == -1)
        Count = Entry.Count;

    return GiveItem(Inventory, (UFortItemDefinition*)Entry.ItemDefinition, Count, Entry.LoadedAmmo, Entry.Level, bNotifyPlayer, bUpdateInventory, Entry.PhantomReserveAmmo, Entry.StateValues, &Entry);
}

static void SetRequiresUpdate(AFortInventory* Inventory)
{
    Inventory->Inventory.MarkArrayDirty();
    Inventory->bRequiresLocalUpdate = true;
    Inventory->bRequiresSaving = true;
    Inventory->HandleInventoryLocalUpdate();

    Inventory->ForceNetUpdate();
}

static void MarkReplicatedEntryDirty(AFortInventory* Inventory, FFortItemEntry& ReplicatedEntry)
{
    ReplicatedEntry.bIsDirty = false;
    Inventory->Inventory.MarkItemDirty(ReplicatedEntry);
    SetRequiresUpdate(Inventory);
}

void Update(AFortInventory* Inventory, FFortItemEntry* Entry)
{
    if (!Entry)
        return SetRequiresUpdate(Inventory);

    if (Entry->bIsReplicatedCopy)
        return MarkReplicatedEntryDirty(Inventory, *Entry);

    if (!IsValidGuid(Entry->ItemGuid))
        return;

    if (auto ReplicatedEntry = FindReplicatedEntry(Inventory, Entry->ItemGuid))
    {
        AssignItemEntry(ReplicatedEntry, Entry);
        MarkReplicatedEntryDirty(Inventory, *ReplicatedEntry);
    }
}

static void ClearAbility(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAbilitySpecHandle& Handle)
{
    static auto ClearAbility = (void (*)(UAbilitySystemComponent*, FGameplayAbilitySpecHandle&))(ImageBase + 0x7CA8DBC);

    if (Handle.Handle != -1)
        ClearAbility(AbilitySystemComponent, Handle);
}

static void ClearAbilityHandle(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAbilitySpecHandle& Handle)
{
    ClearAbility(AbilitySystemComponent, Handle);
    Handle.Handle = -1;
}

void RemoveWeaponAbilities(AFortWeapon* Weapon)
{
    auto PlayerController = Weapon->Instigator ? (AFortPlayerControllerAthena*)Weapon->Instigator->Controller : nullptr;

    if (!PlayerController || !PlayerController->PlayerState || !PlayerController->PlayerState->IsA(AFortPlayerStateAthena::StaticClass()))
        return;

    auto AbilitySystemComponent = ((AFortPlayerStateAthena*)PlayerController->PlayerState)->AbilitySystemComponent;

    ClearAbilityHandle(AbilitySystemComponent, Weapon->PrimaryAbilitySpecHandle);
    ClearAbilityHandle(AbilitySystemComponent, Weapon->SecondaryAbilitySpecHandle);
    ClearAbilityHandle(AbilitySystemComponent, Weapon->ReloadAbilitySpecHandle);
    ClearAbilityHandle(AbilitySystemComponent, Weapon->ImpactAbilitySpecHandle);

    for (int i = 0; i < Weapon->EquippedAbilityHandles.Num(); i++)
        ClearAbility(AbilitySystemComponent, Weapon->EquippedAbilityHandles[i]);

    Weapon->EquippedAbilityHandles.Clear();

    if (Weapon->EquippedAbilitySetHandles.Num())
    {
        for (int i = 0; i < Weapon->EquippedAbilitySetHandles.Num(); i++)
            UFortKismetLibrary::UnequipFortAbilitySet(Weapon->EquippedAbilitySetHandles[i]);

        if (PlayerController->IsA(AFortPlayerControllerAthena::StaticClass()))
            PlayerController->AppliedInGameModifierAbilitySetHandles.Reset();

        Weapon->EquippedAbilitySetHandles.Clear();
    }
}

void Remove(AFortInventory* Inventory, FGuid Guid)
{
    int32 ItemEntryIndex = -1;
    int32 ItemInstanceIndex = -1;

    for (int i = 0; i < Inventory->Inventory.ReplicatedEntries.Num(); i++)
    {
        if (Inventory->Inventory.ReplicatedEntries[i].ItemGuid == Guid)
        {
            ItemEntryIndex = i;
            break;
        }
    }

    for (int i = 0; i < Inventory->Inventory.ItemInstances.Num(); i++)
    {
        if (Inventory->Inventory.ItemInstances[i]->ItemEntry.ItemGuid == Guid)
        {
            ItemInstanceIndex = i;
            break;
        }
    }

    auto Instance = ItemInstanceIndex != -1 ? Inventory->Inventory.ItemInstances.Data[ItemInstanceIndex] : nullptr;

    if (ItemEntryIndex != -1)
    {
        static auto DestroyItemEntry = (void (*)(FFortItemEntry*))(ImageBase + 0x1BC5354);

        auto& ReplicatedEntries = Inventory->Inventory.ReplicatedEntries;

        DestroyItemEntry(&ReplicatedEntries.Data[ItemEntryIndex]);
        ReplicatedEntries.Data[ItemEntryIndex] = ReplicatedEntries.Data[ReplicatedEntries.Num() - 1];
        ReplicatedEntries.NumElements--;
    }

    if (ItemInstanceIndex != -1)
        Inventory->Inventory.ItemInstances.Remove(ItemInstanceIndex);

    Inventory->bRequiresLocalUpdate = true;
    Inventory->bRequiresSaving = true;

    Inventory->HandleInventoryLocalUpdate();

    Inventory->Inventory.MarkArrayDirty();
    Inventory->ForceNetUpdate();

    if (Instance && Instance->ItemEntry.ItemDefinition)
        ((bool (*)(const UFortWorldItem*, const void*, uint32_t))Instance->VTable[0xB3])(Instance, Inventory->Owner->GetInterface(IFortInventoryOwnerInterface::StaticClass()), Instance->ItemEntry.Count);
}

void UpdateItemCount(AFortInventory* Inventory, UFortWorldItem* Item, FFortItemEntry& ItemEntry)
{
    Item->ItemEntry.Count = ItemEntry.Count;
    UpdateEntry(Inventory, ItemEntry);
    Item->ItemEntry.bIsDirty = true;
}

void UpdateOrRemoveEntry(AFortInventory* Inventory, UFortWorldItem* Item, FFortItemEntry& Entry)
{
    if (Entry.Count <= 0)
    {
        Remove(Inventory, Entry.ItemGuid);
        return;
    }

    UpdateItemCount(Inventory, Item, Entry);
}

bool RemoveInventoryItem(AFortInventory* Inventory, FGuid& ItemGuid, int32 Count, bool bForceRemoval, bool bForcePersistWhenEmpty)
{
    auto Item = FindItemInstance(Inventory, ItemGuid);

    if (!Item)
        return false;

    auto ItemEntry = FindReplicatedEntry(Inventory, ItemGuid);
    ItemEntry->Count -= (std::max)(Count, 0);

    if (bForceRemoval || Count < 0 || (ItemEntry->Count <= 0 && !bForcePersistWhenEmpty))
        Remove(Inventory, ItemGuid);
    else
        UpdateItemCount(Inventory, Item, *ItemEntry);

    return true;
}

FFortRangedWeaponStats* GetStats(const UFortWeaponItemDefinition* WeaponDefinition)
{
    if (!WeaponDefinition || !WeaponDefinition->WeaponStatHandle.DataTable)
        return nullptr;

    for (auto& Pair : WeaponDefinition->WeaponStatHandle.DataTable->RowMap)
    {
        if (WeaponDefinition->WeaponStatHandle.RowName == Pair.Key() && Pair.Value())
            return (FFortRangedWeaponStats*)Pair.Value();
    }

    return nullptr;
}

static const UFortWeaponItemDefinition* GetWeaponDefinition(const UFortItemDefinition* ItemDefinition)
{
    if (ItemDefinition->IsA(UFortGadgetItemDefinition::StaticClass()))
        return ((UFortGadgetItemDefinition*)ItemDefinition)->GetWeaponItemDefinition();

    return ItemDefinition->IsA(UFortWeaponItemDefinition::StaticClass()) ? (UFortWeaponItemDefinition*)ItemDefinition : nullptr;
}

FFortItemEntry* MakeItemEntry(const UFortItemDefinition* ItemDefinition, int32 Count, int32 Level)
{
    auto ItemEntry = (FFortItemEntry*)malloc(sizeof(FFortItemEntry));
    memset(ItemEntry, 0, sizeof(FFortItemEntry));

    ItemEntry->MostRecentArrayReplicationKey = -1;
    ItemEntry->ReplicationID = -1;
    ItemEntry->ReplicationKey = -1;

    if (ItemDefinition->IsA(UFortWorldMultiItemDefinition::StaticClass()))
    {
        auto MultiItemDefinition = (UFortWorldMultiItemDefinition*)ItemDefinition;

        ItemDefinition = MultiItemDefinition->ItemInfos[rand() % MultiItemDefinition->ItemInfos.Num()].ItemDefinition.Get();
    }

    ItemEntry->ItemDefinition = (UItemDefinitionBase*)ItemDefinition;
    ItemEntry->Count = Count;
    ItemEntry->Durability = 1.f;
    ItemEntry->GameplayAbilitySpecHandle.Handle = -1;
    ItemEntry->ParentInventory.ObjectIndex = -1;
    ItemEntry->Level = Level;

    if (auto WeaponDefinition = GetWeaponDefinition(ItemDefinition))
    {
        if (auto Stats = GetStats(WeaponDefinition))
        {
            ItemEntry->LoadedAmmo = Stats->ClipSize;

            if (WeaponDefinition->bUsesPhantomReserveAmmo)
                ItemEntry->PhantomReserveAmmo = (Stats->InitialClips - 1) * Stats->ClipSize;
        }
    }

    ItemEntry->PickupVariantIndex = -1;
    ItemEntry->ItemVariantDataMappingIndex = -1;
    ItemEntry->OrderIndex = -1;

    return ItemEntry;
}

bool IsPrimaryQuickbar(const UFortItemDefinition* ItemDefinition)
{
    switch (ItemDefinition->ItemType)
    {
    case EFortItemType::WeaponHarvest:
    case EFortItemType::WorldResource:
    case EFortItemType::Ammo:
    case EFortItemType::Trap:
    case EFortItemType::BuildingPiece:
    case EFortItemType::EditTool:
    case EFortItemType::Ingredient:
        return false;
    default:
        return true;
    }
}

bool TakesQuickbarSlot(const UFortItemDefinition* ItemDefinition)
{
    return IsPrimaryQuickbar(ItemDefinition) && UFortItemFunctionLibrary::IsInventorySizeLimited(ItemDefinition);
}

void UpdateEntry(AFortInventory* Inventory, FFortItemEntry& Entry)
{
    if (!Inventory)
        return;

    Update(Inventory, &Entry);
}

int32 AddItemCount(AFortInventory* Inventory, const UFortItemDefinition* ItemDefinition, int32 Count)
{
    if (!Inventory || !ItemDefinition || Count <= 0)
        return 0;

    auto MaxStack = ItemDefinition->GetMaxStackSize(nullptr);
    auto ItemEntry = FindReplicatedEntry(Inventory, ItemDefinition);

    if (!ItemEntry)
    {
        auto Given = MaxStack > 0 && Count > MaxStack ? MaxStack : Count;
        GiveItem(Inventory, ItemDefinition, Given);
        return Given;
    }

    auto NewCount = MaxStack > 0 && ItemEntry->Count + Count > MaxStack ? MaxStack : ItemEntry->Count + Count;
    auto Added = NewCount - ItemEntry->Count;

    if (Added <= 0)
        return 0;

    ItemEntry->Count = NewCount;

    if (auto Item = FindItemInstance(Inventory, ItemDefinition))
        Item->ItemEntry.Count = NewCount;

    UpdateEntry(Inventory, *ItemEntry);
    return Added;
}
