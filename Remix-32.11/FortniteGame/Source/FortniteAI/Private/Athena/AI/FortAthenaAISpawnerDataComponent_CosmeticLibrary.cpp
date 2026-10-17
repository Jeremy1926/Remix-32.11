#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteAI/Public/Athena/AI/FortAthenaAISpawnerDataComponent_CosmeticLibrary.h"

INIT_MODULE(UFortAthenaAISpawnerDataComponent_CosmeticLibrary);

struct FBotLoadout
{
    UAthenaCharacterItemDefinition* Character;
    UAthenaBackpackItemDefinition* Backpack;
    UAthenaGliderItemDefinition* Glider;
    UAthenaPickaxeItemDefinition* Pickaxe;
    UAthenaSkyDiveContrailItemDefinition* SkyDiveContrail;
};

struct FEmoteRowsInfo
{
    std::vector<std::pair<UAthenaDanceItemDefinition*, float>> Rows;
    float TotalWeight = 0.f;
};

static std::vector<FBotLoadout> Loadouts;
static size_t NextLoadout = 0;
static std::unordered_map<UDataTable*, FEmoteRowsInfo> EmoteRowsInfos;
static const FFortAthenaLoadout* FirstSpawnLoadout = nullptr;

static float RandomFraction()
{
    return float(rand()) / (float(RAND_MAX) + 1.f);
}

static UObject* LoadPrimaryAsset(const FPrimaryAssetId& PrimaryAssetId)
{
    if (auto Object = UKismetSystemLibrary::GetObjectFromPrimaryAssetId(PrimaryAssetId))
        return Object;

    return UKismetSystemLibrary::LoadAsset_Blocking(UKismetSystemLibrary::GetSoftObjectReferenceFromPrimaryAssetId(PrimaryAssetId));
}

static std::vector<FPrimaryAssetId> GetPrimaryAssetIds(const wchar_t* PrimaryAssetType)
{
    TArray<FPrimaryAssetId> List;
    std::vector<FPrimaryAssetId> Ids;

    UKismetSystemLibrary::GetPrimaryAssetIdList(FPrimaryAssetType{ FName(PrimaryAssetType) }, &List);

    for (auto& Id : List)
        Ids.push_back(Id);

    List.Free();

    return Ids;
}

static bool IsExcludedCosmetic(UObject* Object)
{
    static FName ExcludedTags[] = { FName(L"Phoebe.Cosmetic.Minus.Zone1"), FName(L"Cosmetics.UserFacingFlags.BuiltinEmote"), FName(L"Cosmetics.UserFacingFlags.Synced") };

    auto TagInterface = Object->GetInterface(IGameplayTagAssetInterface::StaticClass());

    if (!TagInterface)
        return false;

    FGameplayTagContainer OwnedTags{};
    bool bExcluded = false;

    ((void (*)(void*, FGameplayTagContainer*))(*(void***)TagInterface)[0x2])(TagInterface, &OwnedTags);

    for (auto& Tag : OwnedTags.GameplayTags)
    {
        for (auto& ExcludedTag : ExcludedTags)
            bExcluded |= Tag.TagName == ExcludedTag;
    }

    FreeGameplayTags(OwnedTags);

    return bExcluded;
}

static bool IsPlayerCosmetic(UObject* Object)
{
    static auto HasItemTrait = (bool (*)(UObject*, const FGameplayTag*))(ImageBase + 0x1BC5A54);
    static auto MatchesTag = (bool (*)(const FGameplayTag*, const FGameplayTag*))(ImageBase + 0x189549C);
    static FGameplayTag UnownedTraits[] = { FGameplayTag{ FName(L"Item.Trait.Transient") }, FGameplayTag{ FName(L"Item.Trait.ShuffleTile") } };
    static auto CosmeticsTag = FGameplayTag{ FName(L"Cosmetics") };

    for (auto& Trait : UnownedTraits)
    {
        if (HasItemTrait(Object, &Trait))
            return false;
    }

    FGameplayTagContainer OwnedTags{};
    bool bPlayerCosmetic = false;

    GetOwnedGameplayTags(Object, OwnedTags);

    for (auto& Tag : OwnedTags.GameplayTags)
        bPlayerCosmetic |= MatchesTag(&Tag, &CosmeticsTag);

    FreeGameplayTags(OwnedTags);

    return bPlayerCosmetic && !IsExcludedCosmetic(Object);
}

static UObject* PickItem(std::vector<FPrimaryAssetId>& Ids, const UClass* Class)
{
    while (!Ids.empty())
    {
        auto Index = size_t(RandomFraction() * float(Ids.size()));
        auto Item = LoadPrimaryAsset(Ids[Index]);

        if (Item && Item->IsA(Class) && IsPlayerCosmetic(Item))
            return Item;

        Ids[Index] = Ids.back();
        Ids.pop_back();
    }

    return nullptr;
}

static FBotLoadout PickLoadout()
{
    static auto Characters = GetPrimaryAssetIds(L"AthenaCharacter");
    static auto Backpacks = GetPrimaryAssetIds(L"AthenaBackpack");
    static auto Gliders = GetPrimaryAssetIds(L"AthenaGlider");
    static auto Pickaxes = GetPrimaryAssetIds(L"AthenaPickaxe");
    static auto Contrails = GetPrimaryAssetIds(L"AthenaSkyDiveContrail");

    return {
        (UAthenaCharacterItemDefinition*)PickItem(Characters, UAthenaCharacterItemDefinition::StaticClass()),
        (UAthenaBackpackItemDefinition*)PickItem(Backpacks, UAthenaBackpackItemDefinition::StaticClass()),
        (UAthenaGliderItemDefinition*)PickItem(Gliders, UAthenaGliderItemDefinition::StaticClass()),
        (UAthenaPickaxeItemDefinition*)PickItem(Pickaxes, UAthenaPickaxeItemDefinition::StaticClass()),
        (UAthenaSkyDiveContrailItemDefinition*)PickItem(Contrails, UAthenaSkyDiveContrailItemDefinition::StaticClass())
    };
}

static std::vector<float> GetLibraryWeights(UFortAthenaAISpawnerDataComponent_CosmeticLibrary* Component)
{
    std::vector<float> Weights;

    for (int32 i = 0; i < Component->CosmeticLibraries.Num(); i++)
    {
        auto Weight = Component->DefaultCosmeticLibraryWeight.Evaluate(float(i));

        Weights.push_back(Weight > 0.f ? Weight : 0.f);
    }

    return Weights;
}

static UDataTable* GetEmotesDataTable(UFortAthenaAISpawnerDataComponent_CosmeticLibrary* Component, int32 LibraryIndex)
{
    auto Library = Component->CosmeticLibraries.Data[LibraryIndex].Get();

    return Library ? Library->EmotesDataTable.Get() : nullptr;
}

static FEmoteRowsInfo& GetEmoteRowsInfo(UDataTable* EmotesDataTable)
{
    auto [It, bInserted] = EmoteRowsInfos.try_emplace(EmotesDataTable);
    auto& Info = It->second;

    if (!bInserted)
        return Info;

    for (auto& Pair : EmotesDataTable->RowMap)
    {
        auto Row = (FFortBotCosmeticItemDataTableRow*)Pair.Value();

        if (!Row || Row->Weight <= 0.f)
            continue;

        auto Dance = LoadPrimaryAsset(Row->PrimaryAssetId);

        if (!Dance || !Dance->IsA(UAthenaDanceItemDefinition::StaticClass()) || IsExcludedCosmetic(Dance))
            continue;

        Dance->AddToRoot();
        Info.Rows.push_back({ (UAthenaDanceItemDefinition*)Dance, Row->Weight });
        Info.TotalWeight += Row->Weight;
    }

    return Info;
}

void PreloadBotCosmetics(UClass* SpawnerDataClass, int32 Count)
{
    for (int32 i = 0; i < Count; i++)
    {
        auto Loadout = PickLoadout();

        for (UObject* Item : { (UObject*)Loadout.Character, (UObject*)Loadout.Backpack, (UObject*)Loadout.Glider, (UObject*)Loadout.Pickaxe, (UObject*)Loadout.SkyDiveContrail })
        {
            if (Item)
                Item->AddToRoot();
        }

        Loadouts.push_back(Loadout);
    }

    auto SpawnerData = SpawnerDataClass ? (UFortAthenaAISpawnerData*)SpawnerDataClass->ClassDefaultObject : nullptr;
    auto CosmeticClass = SpawnerData ? SpawnerData->CosmeticComponent.Get() : nullptr;
    auto Component = CosmeticClass ? (UFortAthenaAISpawnerDataComponent_CosmeticLibrary*)CosmeticClass->ClassDefaultObject : nullptr;

    if (!Component || !Component->IsA(UFortAthenaAISpawnerDataComponent_CosmeticLibrary::StaticClass()))
        return;

    auto Weights = GetLibraryWeights(Component);

    for (int32 i = 0; i < int32(Weights.size()); i++)
    {
        if (Weights[i] <= 0.f)
            continue;

        if (auto EmotesDataTable = GetEmotesDataTable(Component, i))
            GetEmoteRowsInfo(EmotesDataTable);
    }
}

static const FFortAthenaLoadout* GetFirstSpawnLoadout(APawn* Pawn)
{
    auto Controller = Pawn ? (AFortAthenaAIBotController*)Pawn->Controller : nullptr;

    if (!Controller || !Controller->IsA(AFortAthenaAIBotController::StaticClass()) || !Controller->bIsAnAthenaGameParticipant)
        return nullptr;

    auto DefaultController = (AFortAthenaAIBotController*)Controller->Class->ClassDefaultObject;

    return Controller->CosmeticLoadoutBC.Character != DefaultController->CosmeticLoadoutBC.Character ? &Controller->CosmeticLoadoutBC : nullptr;
}

static void (*OnSpawnedOG)(UFortAthenaAISpawnerDataComponent_CosmeticLibrary*, APawn*);
static void OnSpawned(UFortAthenaAISpawnerDataComponent_CosmeticLibrary* Component, APawn* Pawn)
{
    FirstSpawnLoadout = GetFirstSpawnLoadout(Pawn);
    OnSpawnedOG(Component, Pawn);
    FirstSpawnLoadout = nullptr;
}

static void GetLoadout(UFortAthenaAISpawnerDataComponent_CosmeticLibrary*, FFortAthenaLoadout* Loadout)
{
    static auto AssignLoadout = (FFortAthenaLoadout* (*)(FFortAthenaLoadout*, const FFortAthenaLoadout*))(ImageBase + 0x25D58A0);

    if (FirstSpawnLoadout)
    {
        AssignLoadout(Loadout, FirstSpawnLoadout);
        return;
    }

    auto BotLoadout = Loadouts.empty() ? PickLoadout() : Loadouts[NextLoadout++ % Loadouts.size()];

    Loadout->Character = BotLoadout.Character;
    Loadout->Backpack = BotLoadout.Backpack;
    Loadout->Glider = BotLoadout.Glider;
    Loadout->Pickaxe = BotLoadout.Pickaxe;
    Loadout->SkyDiveContrail = BotLoadout.SkyDiveContrail;
}

static void GetDances(UFortAthenaAISpawnerDataComponent_CosmeticLibrary* Component, TArray<UAthenaDanceItemDefinition*>* Dances, AFortAthenaAIBotController*)
{
    auto LibraryWeights = GetLibraryWeights(Component);
    float TotalWeight = 0.f;

    for (auto Weight : LibraryWeights)
        TotalWeight += Weight;

    if (TotalWeight <= 0.f)
        return;

    auto Roll = RandomFraction() * TotalWeight;
    int32 LibraryIndex = 0;

    for (; LibraryIndex < int32(LibraryWeights.size()) - 1 && Roll >= LibraryWeights[LibraryIndex]; LibraryIndex++)
        Roll -= LibraryWeights[LibraryIndex];

    auto EmotesDataTable = GetEmotesDataTable(Component, LibraryIndex);

    if (!EmotesDataTable)
        return;

    auto& Info = GetEmoteRowsInfo(EmotesDataTable);
    auto Rows = Info.Rows;
    auto RowsWeight = Info.TotalWeight;

    for (auto Remaining = int32(Component->EmotesMaxCount.Evaluate()); Remaining > 0 && !Rows.empty(); Remaining--)
    {
        auto RowRoll = RandomFraction() * RowsWeight;
        size_t RowIndex = 0;

        for (; RowIndex < Rows.size() - 1 && RowRoll >= Rows[RowIndex].second; RowIndex++)
            RowRoll -= Rows[RowIndex].second;

        Dances->Add(Rows[RowIndex].first);
        RowsWeight -= Rows[RowIndex].second;
        Rows[RowIndex] = Rows.back();
        Rows.pop_back();
    }
}

void UFortAthenaAISpawnerDataComponent_CosmeticLibrary__Init()
{
    Hook<UFortAthenaAISpawnerDataComponent_CosmeticLibrary>(uint32(0x5B), OnSpawned, OnSpawnedOG);
    Hook<UFortAthenaAISpawnerDataComponent_CosmeticLibrary>(uint32(0x5F), GetDances);
    Hook<UFortAthenaAISpawnerDataComponent_CosmeticLibrary>(uint32(0x62), GetLoadout);
}
