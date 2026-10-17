#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortNonPlayerConversationParticipantComponent.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationLibrary.h"
#include "FortniteGame/Source/FortniteGame/Public/FortKismetLibrary.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"

INIT_MODULE(UFortNonPlayerConversationParticipantComponent);

struct FServiceState
{
    bool bInitialized = false;
    std::vector<FServiceEntry> Services;
    std::vector<const FNPCSaleInventoryRow*> Sales;
    std::unordered_map<uint64, FChosenSaleItem> ChosenItems;
};

static std::unordered_map<uint64, int32> NPCUniqueIDTags;
static std::unordered_map<uint64, FServiceState> ServiceStates;

uint64 GetObjectKey(const UObject* Object)
{
    FWeakObjectPtr WeakObject(Object);

    return (uint64(uint32(WeakObject.ObjectIndex)) << 32) | uint32(WeakObject.ObjectSerialNumber);
}

void SetNPCUniqueIDTag(const UObject* Component, const FGameplayTag& Tag)
{
    if (Component && Tag.TagName.ComparisonIndex != 0)
        NPCUniqueIDTags[GetObjectKey(Component)] = Tag.TagName.ComparisonIndex;
}

FGameplayTag GetServiceProviderTag(const UFortNonPlayerConversationParticipantComponent* Component)
{
    return Component->ServiceProviderIDTag.TagName.ComparisonIndex != 0 ? Component->ServiceProviderIDTag : Component->ConversationEntryTag;
}

UFortNonPlayerConversationParticipantComponent* GetNPCComponent(const FConversationContext* Context)
{
    auto Conversation = Context ? Context->ActiveConversation : nullptr;

    if (!Conversation)
        return nullptr;

    for (int32 i = 0; i < Conversation->Participants.List.Num(); i++)
    {
        auto Actor = Conversation->Participants.List.Data[i].Actor;

        if (!Actor)
            continue;

        if (auto Component = Actor->GetComponentByClass(UFortNonPlayerConversationParticipantComponent::StaticClass()))
            return (UFortNonPlayerConversationParticipantComponent*)Component;
    }

    return nullptr;
}

static UDataTable* ResolveTable(UDataTable* DefaultTable, const FGameplayTag& OverrideTag)
{
    if (OverrideTag.TagName.ComparisonIndex == 0)
        return DefaultTable;

    for (int32 i = 0; i < UObject::GObjects->Num(); i++)
    {
        auto Season = (UAthenaSeasonItemDefinition*)UObject::GObjects->GetByIndex(i);

        if (!Season || Season->IsDefaultObject() || !Season->IsA(UAthenaSeasonItemDefinition::StaticClass()) || Season->SeasonNumber != 32)
            continue;

        for (auto& SeasonDataTable : Season->SeasonDataTables)
            if (SeasonDataTable.Key().TagName == OverrideTag.TagName)
                return SeasonDataTable.Value();

        break;
    }

    return DefaultTable;
}

static bool DoesTagMatch(const FGameplayTag& Tag, const FGameplayTag& TagToCheck, bool bExactMatch)
{
    if (Tag.TagName.ComparisonIndex == 0 || TagToCheck.TagName.ComparisonIndex == 0)
        return false;

    if (Tag.TagName == TagToCheck.TagName)
        return true;

    if (bExactMatch)
        return false;

    auto TagString = Tag.TagName.ToString();
    auto CheckString = TagToCheck.TagName.ToString();

    return TagString.size() > CheckString.size() && TagString.compare(0, CheckString.size(), CheckString) == 0 && TagString[CheckString.size()] == '.';
}

static FServiceState& GetServiceState(const UFortNonPlayerConversationParticipantComponent* Component)
{
    auto& State = ServiceStates[GetObjectKey(Component)];

    if (State.bInitialized)
        return State;

    State.bInitialized = true;

    auto Found = NPCUniqueIDTags.find(GetObjectKey(Component));
    auto NPCUniqueIDTag = GetServiceProviderTag(Component);

    if (Found != NPCUniqueIDTags.end())
        NPCUniqueIDTag.TagName.ComparisonIndex = Found->second;

    if (auto ServicesTable = ResolveTable(Component->Services, Component->ServicesOverride))
    {
        std::vector<std::pair<int32, FServiceEntry>> Candidates;

        for (auto& Row : ServicesTable->RowMap)
        {
            auto ServiceRow = (FNPCDynamicServiceRow*)Row.Value();

            if (!ServiceRow || ServiceRow->NPC.TagName != NPCUniqueIDTag.TagName)
                continue;

            auto Chance = ServiceRow->Chance;

            if (Chance <= 0.f || (Chance < 1.f && float(rand()) / float(RAND_MAX) >= Chance))
                continue;

            Candidates.push_back({ ServiceRow->Priority, { ServiceRow->ServiceTag.TagName.ComparisonIndex, ServiceRow->AdditionalRequirementTag.TagName.ComparisonIndex, ServiceRow->bShowAsLockedIfUnavailable } });
        }

        std::stable_sort(Candidates.begin(), Candidates.end(), [](auto& Left, auto& Right) { return Left.first > Right.first; });

        for (auto& [Priority, Entry] : Candidates)
        {
            if (Component->MaxServices > 0 && int32(State.Services.size()) >= Component->MaxServices)
                break;

            State.Services.push_back(Entry);
        }
    }

    if (auto SalesTable = ResolveTable(Component->SalesInventory, Component->SalesInventoryOverride))
    {
        for (auto& Row : SalesTable->RowMap)
        {
            auto SaleRow = (const FNPCSaleInventoryRow*)Row.Value();

            if (SaleRow && SaleRow->NPC.TagName == NPCUniqueIDTag.TagName)
                State.Sales.push_back(SaleRow);
        }

        std::stable_sort(State.Sales.begin(), State.Sales.end(), [](auto Left, auto Right) { return Left->Priority > Right->Priority; });
    }

    return State;
}

const FServiceEntry* FindSupportedService(const UFortNonPlayerConversationParticipantComponent* Component, const FGameplayTag& ServiceTag, bool bExactMatch)
{
    for (auto& Service : GetServiceState(Component).Services)
    {
        FGameplayTag SupportedTag{};
        SupportedTag.TagName.ComparisonIndex = Service.ServiceTag;

        if (DoesTagMatch(SupportedTag, ServiceTag, bExactMatch))
            return &Service;
    }

    return nullptr;
}

const FChosenSaleItem* GetSaleItem(const UFortNonPlayerConversationParticipantComponent* Component, int32 SellSlot, const FGameplayTag& FilterCategoryTag)
{
    auto& State = GetServiceState(Component);
    std::vector<const FNPCSaleInventoryRow*> FilteredSales;

    for (auto Sale : State.Sales)
        if (FilterCategoryTag.TagName.ComparisonIndex == 0 || Sale->OptionalFilterCategoryTag.TagName == FilterCategoryTag.TagName)
            FilteredSales.push_back(Sale);

    if (SellSlot < 1 || SellSlot > int32(FilteredSales.size()))
        return nullptr;

    auto Key = (uint64(uint32(FilterCategoryTag.TagName.ComparisonIndex)) << 32) | uint32(SellSlot);
    auto Found = State.ChosenItems.find(Key);

    if (Found != State.ChosenItems.end())
        return Found->second.ItemDefinition ? &Found->second : nullptr;

    auto Row = FilteredSales[SellSlot - 1];
    FChosenSaleItem ChosenItem{ nullptr, 0, Row };
    TArray<FFortItemEntry*> LootDrops{};

    if (Row->LootLevel >= 0)
        ChooseLootForContainer(LootDrops, Row->LootTier, -1, Row->LootLevel);
    else
        ChooseLootForContainer(LootDrops, Row->LootTier);

    for (auto& LootDrop : LootDrops)
    {
        auto ItemDefinition = (UFortItemDefinition*)LootDrop->ItemDefinition;

        if (ItemDefinition && (!ChosenItem.ItemDefinition || (!IsPrimaryQuickbar(ChosenItem.ItemDefinition) && IsPrimaryQuickbar(ItemDefinition))))
        {
            ChosenItem.ItemDefinition = ItemDefinition;
            ChosenItem.StackSize = LootDrop->Count > 0 ? LootDrop->Count : 1;
        }

        free(LootDrop);
    }

    LootDrops.Free();

    auto& Stored = State.ChosenItems[Key] = ChosenItem;

    return Stored.ItemDefinition ? &Stored : nullptr;
}

float GetDiscountMultiplier(const UFortNonPlayerConversationParticipantComponent* Component)
{
    if (!Component->DiscountTable)
        return 1.f;

    for (auto& Row : Component->DiscountTable->RowMap)
    {
        auto DiscountRow = (FNPCDiscountServicesRow*)Row.Value();

        if (DiscountRow && DiscountRow->NPCConversationTag.TagName == Component->ConversationEntryTag.TagName)
            return DiscountRow->DiscountMultiplier;
    }

    return 1.f;
}

bool CanStartConversationWith(const UFortNonPlayerConversationParticipantComponent* Component, AActor* PlayerController)
{
    return ((bool (*)(const UFortNonPlayerConversationParticipantComponent*, AActor*))Component->VTable[0xA7])(Component, PlayerController);
}

UConversationInstance* StartConversation(const UFortNonPlayerConversationParticipantComponent* Component, const FGameplayTag& InConversationEntryTag, AActor* Instigator, AActor* Target, const UClass* ConversationInstanceClass, const UConversationDatabase* Graph)
{
    return StartConversation(InConversationEntryTag, Instigator, Component->InteractorParticipantTag, Target, Component->SelfParticipantTag, (UClass*)ConversationInstanceClass, (UConversationDatabase*)Graph);
}

void StartConversation(UObject* Context, FFrame& Stack)
{
    FGameplayTag InConversationEntryTag{};
    AActor* Instigator = nullptr;
    AActor* Target = nullptr;

    Stack.StepCompiledIn(&InConversationEntryTag);
    Stack.StepCompiledIn(&Instigator);
    Stack.StepCompiledIn(&Target);
    Stack.IncrementCode();

    StartConversation((UFortNonPlayerConversationParticipantComponent*)Context, InConversationEntryTag, Instigator, Target);
}

void StartConversationFromGraph(UObject* Context, FFrame& Stack)
{
    FGameplayTag InConversationEntryTag{};
    AActor* Instigator = nullptr;
    AActor* Target = nullptr;
    UConversationDatabase* Conversation = nullptr;

    Stack.StepCompiledIn(&InConversationEntryTag);
    Stack.StepCompiledIn(&Instigator);
    Stack.StepCompiledIn(&Target);
    Stack.StepCompiledIn(&Conversation);
    Stack.IncrementCode();

    if (Conversation)
        StartConversation((UFortNonPlayerConversationParticipantComponent*)Context, InConversationEntryTag, Instigator, Target, nullptr, Conversation);
}

void UFortNonPlayerConversationParticipantComponent__Init()
{
    ExecHook(UFortNonPlayerConversationParticipantComponent::StaticClass()->FindFunction("StartConversation"), (void (*)(UObject*, FFrame&))StartConversation);
    ExecHook(UFortNonPlayerConversationParticipantComponent::StaticClass()->FindFunction("StartConversationFromGraph"), (void (*)(UObject*, FFrame&))StartConversationFromGraph);
}
