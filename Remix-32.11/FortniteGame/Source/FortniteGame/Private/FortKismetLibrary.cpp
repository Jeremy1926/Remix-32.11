#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/FortKismetLibrary.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortAthenaSupplyDrop.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortPickup.h"
#include "FortniteGame/Source/FortniteGame/Public/Player/FortPlayerController.h"

INIT_MODULE(UFortKismetLibrary);
INIT_POSTLOAD(UFortKismetLibrary);

static void GiveItemToInventoryOwner(UObject* Context, FFrame& Stack, UFortWorldItem** Ret)
{
    TScriptInterface<IFortInventoryOwnerInterface> InventoryOwner;
    UFortItemDefinition* ItemDefinition;
    FGuid ItemVariantGuid;
    int32 NumberToGive;
    bool bNotifyPlayer;
    int32 ItemLevel;
    int32 PickupInstigatorHandle;
    bool bUseItemPickupAnalyticEvent;
    int32 WeaponAmmoOverride;

    Stack.StepCompiledIn(&InventoryOwner);
    Stack.StepCompiledIn(&ItemDefinition);
    Stack.StepCompiledIn(&ItemVariantGuid);
    Stack.StepCompiledIn(&NumberToGive);
    Stack.StepCompiledIn(&bNotifyPlayer);
    Stack.StepCompiledIn(&ItemLevel);
    Stack.StepCompiledIn(&PickupInstigatorHandle);
    Stack.StepCompiledIn(&bUseItemPickupAnalyticEvent);
    Stack.StepCompiledIn(&WeaponAmmoOverride);
    Stack.IncrementCode();

    *Ret = nullptr;

    auto Owner = InventoryOwner.ObjectPointer;

    if (!Owner || !ItemDefinition)
        return;

    if (Owner->IsA(AFortAthenaAIBotController::StaticClass()))
    {
        auto BotInventory = (AFortInventory*)((AFortAthenaAIBotController*)Owner)->Inventory.ObjectPointer;

        if (!BotInventory)
            return;

        auto Stats = ItemDefinition->IsA(UFortWeaponRangedItemDefinition::StaticClass()) ? GetStats((UFortWeaponItemDefinition*)ItemDefinition) : nullptr;
        auto LoadedAmmo = WeaponAmmoOverride != -1 ? WeaponAmmoOverride : (Stats ? Stats->ClipSize : 0);

        *Ret = GiveItem(BotInventory, ItemDefinition, NumberToGive, LoadedAmmo, ItemLevel != -1 ? ItemLevel : 0);
        return;
    }

    auto PlayerController = Owner->IsA(AFortPlayerControllerAthena::StaticClass()) ? (AFortPlayerControllerAthena*)Owner : nullptr;
    auto WorldInventory = PlayerController ? (AFortInventory*)PlayerController->WorldInventory.ObjectPointer : nullptr;

    if (!WorldInventory)
        return;

    auto ItemEntry = MakeItemEntry(ItemDefinition, NumberToGive, ItemLevel);

    if (WeaponAmmoOverride != -1)
        ItemEntry->LoadedAmmo = WeaponAmmoOverride;

    InternalPickup(PlayerController, ItemEntry);
    free(ItemEntry);

    *Ret = FindItemInstance(WorldInventory, ItemDefinition);
}

static int32 RemoveItemFromPlayer(AFortInventory* WorldInventory, UFortWorldItem* Item, FFortItemEntry* ItemEntry, int32 AmountToRemove)
{
    auto RemoveCount = (std::max)(AmountToRemove, 0);
    ItemEntry->Count -= RemoveCount;

    if (AmountToRemove < 0 || ItemEntry->Count <= 0)
    {
        RemoveCount += ItemEntry->Count;
        Remove(WorldInventory, ItemEntry->ItemGuid);
    }
    else
        UpdateItemCount(WorldInventory, Item, *ItemEntry);

    return RemoveCount;
}

static void K2_RemoveItemFromPlayer(UObject* Context, FFrame& Stack, int32* Ret)
{
    AFortPlayerControllerAthena* PlayerController;
    UFortItemDefinition* ItemDefinition;
    FGuid ItemVariantGuid;
    int32 AmountToRemove;
    bool bForceRemoval;

    Stack.StepCompiledIn(&PlayerController);
    Stack.StepCompiledIn(&ItemDefinition);
    Stack.StepCompiledIn(&ItemVariantGuid);
    Stack.StepCompiledIn(&AmountToRemove);
    Stack.StepCompiledIn(&bForceRemoval);
    Stack.IncrementCode();

    *Ret = 0;

    if (!PlayerController || !ItemDefinition)
        return;

    auto WorldInventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;

    if (auto Item = FindItemInstance(WorldInventory, ItemDefinition))
        *Ret = RemoveItemFromPlayer(WorldInventory, Item, FindReplicatedEntry(WorldInventory, ItemDefinition), AmountToRemove);
}

static void K2_RemoveItemFromPlayerByGuid(UObject* Context, FFrame& Stack, int32* Ret)
{
    AFortPlayerControllerAthena* PlayerController;
    FGuid ItemGuid;
    int32 AmountToRemove;
    bool bForceRemoval;

    Stack.StepCompiledIn(&PlayerController);
    Stack.StepCompiledIn(&ItemGuid);
    Stack.StepCompiledIn(&AmountToRemove);
    Stack.StepCompiledIn(&bForceRemoval);
    Stack.IncrementCode();

    *Ret = 0;

    if (!PlayerController)
        return;

    auto WorldInventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;

    if (auto Item = FindItemInstance(WorldInventory, ItemGuid))
        *Ret = RemoveItemFromPlayer(WorldInventory, Item, FindReplicatedEntry(WorldInventory, ItemGuid), AmountToRemove);
}

template <typename T>
static T* PickWeighted(TArray<T*>& Elements)
{
    float TotalWeight = 0.0f;

    for (auto& Element : Elements)
        TotalWeight += Element->Weight;

    float RandomNumber = ((float)rand() / 32767.f) * TotalWeight;

    for (auto& Element : Elements)
    {
        float Weight = Element->Weight;

        if (Weight == 0)
            continue;

        if (RandomNumber <= Weight)
            return Element;

        RandomNumber -= Weight;
    }

    return nullptr;
}

static FFortItemEntry* MakeWeaponAmmoEntry(UFortItemDefinition* ItemDefinition, int WorldLevel)
{
    if (!ItemDefinition->IsA(UFortWorldItemDefinition::StaticClass()))
        return nullptr;

    auto AmmoDefinition = ((UFortWorldItemDefinition*)ItemDefinition)->GetAmmoWorldItemDefinition_BP();

    if (!AmmoDefinition || !AmmoDefinition->IsA(UFortAmmoItemDefinition::StaticClass()))
        return nullptr;

    auto SpawnAmmoData = FindObject<UFortWeaponPickupSpawnAmmoData>(L"/Game/Athena/Balance/Pickups/FortWeaponPickupSpawnAmmoData.FortWeaponPickupSpawnAmmoData");

    FGameplayTagContainer AmmoTags{};
    GetOwnedGameplayTags(AmmoDefinition, AmmoTags);

    float AmmoCount = 0.f;

    for (auto& AmmoCountData : SpawnAmmoData->WeaponPickupAmmoCountArray)
    {
        if (AmmoTags.HasTag(AmmoCountData.AmmoItemDefinitionTag))
        {
            AmmoCount = AmmoCountData.SpawnCount.Evaluate((float)WorldLevel);
            break;
        }
    }

    auto Multiplier = SpawnAmmoData->DefaultWeaponAmmoMultiplier.Evaluate((float)WorldLevel);

    FGameplayTagContainer WeaponTags{};
    GetOwnedGameplayTags(ItemDefinition, WeaponTags);

    for (auto& MultiplierData : SpawnAmmoData->WeaponPickupAmmoMultiplierOverrideArray)
    {
        if (WeaponTags.HasTag(MultiplierData.Tag))
        {
            Multiplier = MultiplierData.CountMultiplier.Evaluate((float)WorldLevel);
            break;
        }
    }

    auto FinalCount = AmmoCount * Multiplier;
    auto AmmoEntry = FinalCount > 0.f ? MakeItemEntry(AmmoDefinition, (int)FinalCount, 0) : nullptr;

    FreeGameplayTags(AmmoTags);
    FreeGameplayTags(WeaponTags);

    return AmmoEntry;
}

static void AddToLootDrop(TArray<FFortItemEntry*>& LootDrops, FFortItemEntry* LootDrop, const UFortItemDefinition* ItemDefinition, int32 Count)
{
    LootDrop->Count += Count;

    if (LootDrop->Count > ItemDefinition->GetMaxStackSize(nullptr))
    {
        auto TotalCount = LootDrop->Count;
        LootDrop->Count = ItemDefinition->GetMaxStackSize(nullptr);

        LootDrops.Add(MakeItemEntry(ItemDefinition, TotalCount - ItemDefinition->GetMaxStackSize(nullptr), 0));
    }
}

static void SetupLDSForPackage(TArray<FFortItemEntry*>& LootDrops, FName Package, int LootPackageCategory, int WorldLevel)
{
    TArray<FFortLootPackageData*> Candidates{};

    for (auto const& Candidate : LootPackageMap[Package.ComparisonIndex])
    {
        if (!Candidate)
            continue;

        if (LootPackageCategory != -1 && Candidate->LootPackageCategory != LootPackageCategory)
            continue;

        if (WorldLevel >= 0)
        {
            if (Candidate->MaxWorldLevel >= 0 && WorldLevel > Candidate->MaxWorldLevel)
                continue;

            if (Candidate->MinWorldLevel >= 0 && WorldLevel < Candidate->MinWorldLevel)
                continue;
        }

        Candidates.Add(Candidate);
    }

    if (Candidates.Num() == 0)
        return;

    auto LootPackage = PickWeighted(Candidates);

    if (!LootPackage)
        return;

    if (LootPackage->LootPackageCall.Num() > 1)
    {
        for (int i = 0; i < LootPackage->Count; i++)
            SetupLDSForPackage(LootDrops, UKismetStringLibrary::Conv_StringToName(LootPackage->LootPackageCall), -1, WorldLevel);

        return;
    }

    auto ItemDefinition = LootPackage->ItemDefinition.Get();

    if (!ItemDefinition)
        return;

    auto AmmoEntry = MakeWeaponAmmoEntry(ItemDefinition, WorldLevel);
    bool bFound = false;

    for (auto& LootDrop : LootDrops)
    {
        if (LootDrop->ItemDefinition == (UItemDefinitionBase*)ItemDefinition && LootDrop->Count < ItemDefinition->GetMaxStackSize(nullptr))
        {
            AddToLootDrop(LootDrops, LootDrop, ItemDefinition, LootPackage->Count);
            bFound = true;
            break;
        }

        if (AmmoEntry && AmmoEntry->ItemDefinition && LootDrop->ItemDefinition == AmmoEntry->ItemDefinition)
        {
            AddToLootDrop(LootDrops, LootDrop, (UFortItemDefinition*)AmmoEntry->ItemDefinition, AmmoEntry->Count);
            free(AmmoEntry);
            AmmoEntry = nullptr;
        }
    }

    if (!bFound && LootPackage->Count > 0)
        LootDrops.Add(MakeItemEntry(ItemDefinition, LootPackage->Count, 0));

    if (AmmoEntry && LootPackage->Count > 0)
        LootDrops.Add(AmmoEntry);

    Candidates.Free();
}

void ChooseLootForContainer(TArray<FFortItemEntry*>& LootDrops, FName TierGroup, int LootTier, int WorldLevel)
{
    TArray<FFortLootTierData*> TierDataGroups{};

    for (auto const& TierData : TierDataMap[TierGroup.ComparisonIndex])
        if (LootTier == -1 || LootTier == TierData->LootTier)
            TierDataGroups.Add(TierData);

    auto LootTierData = PickWeighted(TierDataGroups);

    if (!LootTierData || LootTierData->NumLootPackageDrops <= 0)
        return;

    int DropCount;

    if (LootTierData->NumLootPackageDrops < 1.f)
        DropCount = 1;
    else
    {
        DropCount = (int)((LootTierData->NumLootPackageDrops * 2.f) - .5f) >> 1;

        float Remainder = LootTierData->NumLootPackageDrops - (float)DropCount;

        if (Remainder > 0.0000099999997f)
            DropCount += Remainder >= ((float)rand() / 32767.f);
    }

    auto& Weights = LootTierData->LootPackageCategoryWeightArray;
    auto& Maxes = LootTierData->LootPackageCategoryMaxArray;
    auto NumCategories = (std::min)(LootTierData->LootPackageCategoryMinArray.Num(), Weights.Num());

    TArray<int32> CategoryDrops{};
    int32 NumDrops = 0;

    for (int32 i = 0; i < NumCategories; i++)
    {
        CategoryDrops.Add(LootTierData->LootPackageCategoryMinArray[i]);
        NumDrops += CategoryDrops[i];
    }

    auto CanRollCategory = [&](int32 i) { return Weights[i] > 0 && (Maxes[i] < 0 || CategoryDrops[i] < Maxes[i]); };

    for (; NumDrops < DropCount; NumDrops++)
    {
        int32 SumWeights = 0;

        for (int32 i = 0; i < NumCategories; i++)
            if (CanRollCategory(i))
                SumWeights += Weights[i];

        if (SumWeights <= 0)
            break;

        auto RandomWeight = (int32)(((float)rand() / 32768.f) * SumWeights);

        for (int32 i = 0; i < NumCategories; i++)
        {
            if (!CanRollCategory(i))
                continue;

            if (RandomWeight < Weights[i])
            {
                CategoryDrops[i]++;
                break;
            }

            RandomWeight -= Weights[i];
        }
    }

    for (int32 i = 0; i < NumCategories; i++)
        for (int32 j = 0; j < CategoryDrops[i]; j++)
            SetupLDSForPackage(LootDrops, LootTierData->LootPackage, i, WorldLevel);

    CategoryDrops.Free();
    TierDataGroups.Free();
}

static void K2_SpawnPickupInWorld(UObject* Context, FFrame& Stack, AFortPickupAthena** Ret)
{
    UObject* WorldContextObject;
    UFortItemDefinition* ItemDefinition;
    int32 NumberToSpawn;
    FVector Position;
    FVector Direction;
    int32 OverrideMaxStackCount;
    bool bToss;
    bool bRandomRotation;
    bool bBlockedFromAutoPickup;
    int32 PickupInstigatorHandle;
    EFortPickupSourceTypeFlag SourceType;
    EFortPickupSpawnSource Source;
    AFortPlayerController* OptionalOwnerPC;
    bool bPickupOnlyRelevantToOwner;

    Stack.StepCompiledIn(&WorldContextObject);
    Stack.StepCompiledIn(&ItemDefinition);
    Stack.StepCompiledIn(&NumberToSpawn);
    Stack.StepCompiledIn(&Position);
    Stack.StepCompiledIn(&Direction);
    Stack.StepCompiledIn(&OverrideMaxStackCount);
    Stack.StepCompiledIn(&bToss);
    Stack.StepCompiledIn(&bRandomRotation);
    Stack.StepCompiledIn(&bBlockedFromAutoPickup);
    Stack.StepCompiledIn(&PickupInstigatorHandle);
    Stack.StepCompiledIn(&SourceType);
    Stack.StepCompiledIn(&Source);
    Stack.StepCompiledIn(&OptionalOwnerPC);
    Stack.StepCompiledIn(&bPickupOnlyRelevantToOwner);
    Stack.IncrementCode();

    *Ret = SpawnPickup(Position, ItemDefinition, NumberToSpawn, -1, SourceType, Source, OptionalOwnerPC ? OptionalOwnerPC->MyFortPawn : nullptr, bToss, bRandomRotation);
}

static void K2_SpawnPickupInWorldWithClassAndItemEntry(UObject* Context, FFrame& Stack, AFortPickupAthena** Ret)
{
    UObject* WorldContextObject;
    FFortItemEntry Entry{};
    TSubclassOf<AFortPickupAthena> PickupClass;
    FVector Position;
    FVector Direction;
    int32 OverrideMaxStackCount;
    bool bToss;
    bool bRandomRotation;
    bool bBlockedFromAutoPickup;
    EFortPickupSourceTypeFlag SourceType;
    EFortPickupSpawnSource Source;
    AFortPlayerController* OptionalOwnerPC;
    bool bPickupOnlyRelevantToOwner;

    Stack.StepCompiledIn(&WorldContextObject);
    Stack.StepCompiledIn(&Entry);
    Stack.StepCompiledIn(&PickupClass);
    Stack.StepCompiledIn(&Position);
    Stack.StepCompiledIn(&Direction);
    Stack.StepCompiledIn(&OverrideMaxStackCount);
    Stack.StepCompiledIn(&bToss);
    Stack.StepCompiledIn(&bRandomRotation);
    Stack.StepCompiledIn(&bBlockedFromAutoPickup);
    Stack.StepCompiledIn(&SourceType);
    Stack.StepCompiledIn(&Source);
    Stack.StepCompiledIn(&OptionalOwnerPC);
    Stack.StepCompiledIn(&bPickupOnlyRelevantToOwner);
    Stack.IncrementCode();

    *Ret = SpawnPickup(Position, (UFortItemDefinition*)Entry.ItemDefinition, Entry.Count, Entry.LoadedAmmo, SourceType, Source, OptionalOwnerPC ? OptionalOwnerPC->MyFortPawn : nullptr, bToss, bRandomRotation);

    static auto DestroyItemEntry = (void (*)(FFortItemEntry*))(ImageBase + 0x1BC5354);
    DestroyItemEntry(&Entry);
}

static void SpawnItemVariantPickupInWorld(UObject* Context, FFrame& Stack, AFortPickupAthena** Ret)
{
    UObject* WorldContextObject;
    FSpawnItemVariantParams Params;

    Stack.StepCompiledIn(&WorldContextObject);
    Stack.StepCompiledIn(&Params);
    Stack.IncrementCode();

    *Ret = SpawnPickup(Params.Position, Params.WorldItemDefinition, Params.NumberToSpawn, -1, Params.SourceType, Params.Source, nullptr, Params.bToss, Params.bRandomRotation);
}

static void PickLootDrops(UObject* Context, FFrame& Stack, bool* Ret)
{
    UObject* WorldContextObject;
    FName TierGroupName;
    int32 WorldLevel;
    int32 ForcedLootTier;
    FGameplayTagContainer OptionalLootTags{};

    Stack.StepCompiledIn(&WorldContextObject);
    TArray<FFortItemEntry> OutLootToDropTemp{};
    auto& OutLootToDrop = Stack.StepCompiledInRef(&OutLootToDropTemp);
    Stack.StepCompiledIn(&TierGroupName);
    Stack.StepCompiledIn(&WorldLevel);
    Stack.StepCompiledIn(&ForcedLootTier);
    Stack.StepCompiledIn(&OptionalLootTags);
    Stack.IncrementCode();

    TArray<FFortItemEntry*> LootDrops{};
    ChooseLootForContainer(LootDrops, TierGroupName, ForcedLootTier, WorldLevel);

    for (auto LootDrop : LootDrops)
    {
        OutLootToDrop.Add(*LootDrop);
        free(LootDrop);
    }

    *Ret = LootDrops.Num() > 0;
}

void UFortKismetLibrary__Init()
{
    ExecHook(UFortKismetLibrary::StaticClass()->FindFunction("K2_SpawnPickupInWorld"), K2_SpawnPickupInWorld);
    ExecHook(UFortKismetLibrary::StaticClass()->FindFunction("K2_SpawnPickupInWorldWithClassAndItemEntry"), K2_SpawnPickupInWorldWithClassAndItemEntry);
    ExecHook(UFortKismetLibrary::StaticClass()->FindFunction("SpawnItemVariantPickupInWorld"), SpawnItemVariantPickupInWorld);
    ExecHook(UFortKismetLibrary::StaticClass()->FindFunction("PickLootDrops"), PickLootDrops);
}

void UFortKismetLibrary__PostLoad()
{
    ExecHook(UFortKismetLibrary::StaticClass()->FindFunction("GiveItemToInventoryOwner"), GiveItemToInventoryOwner);
    ExecHook(UFortKismetLibrary::StaticClass()->FindFunction("K2_RemoveItemFromPlayer"), K2_RemoveItemFromPlayer);
    ExecHook(UFortKismetLibrary::StaticClass()->FindFunction("K2_RemoveItemFromPlayerByGuid"), K2_RemoveItemFromPlayerByGuid);
}
