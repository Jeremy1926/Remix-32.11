#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Plugins/GameFeatures/Accolades/Source/AccoladesRuntime/Public/Components/FortGameStateComponent_Accolades.h"
#include "SDK/SDK/AccoladesRuntime_structs.hpp"
#include "SDK/SDK/AccoladesRuntime_classes.hpp"

INIT_POSTLOAD(UFortGameStateComponent_Accolades);

using FAccoladesTable = TMap<FDataRegistryId, FFortAccoladesTableRow*>;
using FFilterEvaluator = std::function<int32(const FInstancedStruct&, FName)>;

struct FAccoladeGateState
{
    bool bOpen = false;
    double CloseTime = -1.0;
};

struct FAccoladeQuestState
{
    std::unordered_map<int32, int32> ObjectiveCounts;
    std::unordered_map<int32, std::vector<FAccoladeGateState>> Gates;
};

struct FAccoladeQuest
{
    UFortQuestItemDefinition* Definition = nullptr;
    std::unordered_map<int32, FObjectiveExtension_Gate*> Gates;
};

struct FAccoladePlayerState
{
    FWeakObjectPtr PlayerController;
    std::unordered_map<int32, int32> TriggerCounts;
    std::unordered_map<int32, int64> TrackedAmounts;
    std::unordered_map<int32, FAccoladeQuestState> Quests;
};

static std::unordered_map<AFortPlayerControllerAthena*, FAccoladePlayerState> PlayerStates;
static std::unordered_map<int32, int32> MatchTriggerCounts;

static FAccoladesTable* GetAccoladesTable()
{
    static auto GetGameStateComponent = (UFortGameStateComponent_Accolades* (*)(AGameStateBase*))(ImageBase + 0xB486788);

    auto GameState = UWorld::GetWorld()->GameState;
    auto Component = GameState ? GetGameStateComponent(GameState) : nullptr;

    return Component ? (FAccoladesTable*)(__int64(Component) + 0xD0) : nullptr;
}

static FAccoladePlayerState& GetPlayerState(AFortPlayerControllerAthena* PlayerController)
{
    auto& State = PlayerStates[PlayerController];

    if (State.PlayerController.Get() != PlayerController)
        State = { FWeakObjectPtr(PlayerController) };

    return State;
}

static UFortQuestManagerComponent_Athena* GetQuestManagerComponent(AFortPlayerControllerAthena* PlayerController)
{
    auto QuestManager = PlayerController->GetQuestManager(ESubGame::Athena);

    for (int32 i = 0; QuestManager && i < QuestManager->Components.Num(); i++)
    {
        auto Component = QuestManager->Components[i];

        if (Component && Component->IsA(UFortQuestManagerComponent_Athena::StaticClass()))
            return (UFortQuestManagerComponent_Athena*)Component;
    }

    return nullptr;
}

static int32 GetCount(const std::unordered_map<int32, int32>& Counts, FName RowName)
{
    auto It = Counts.find(RowName.ComparisonIndex);

    return It != Counts.end() ? It->second : 0;
}

static bool IsBelowLimit(int32 Limit, int32 Count)
{
    return Limit < 0 || Count < Limit;
}

static bool CanTriggerAccolade(const FAccoladePlayerState& State, FName RowName, FFortAccoladesTableRow* Row)
{
    return IsBelowLimit(Row->MaxPlayerCount, GetCount(State.TriggerCounts, RowName)) && IsBelowLimit(Row->MaxMatchCount, GetCount(MatchTriggerCounts, RowName));
}

static bool IsCompatibleWithProduct(FFortAccoladesTableRow* Row, const FGameplayTagContainer& ProductTags)
{
    return Row->ProductCompatibilityQuery.QueryTokenStream.Num() == 0 || UBlueprintGameplayTagLibrary::DoesContainerMatchTagQuery(ProductTags, Row->ProductCompatibilityQuery);
}

static int32 EvaluateFilters(TArray<FInstancedStruct>& Filters, FName BackendName, const FFilterEvaluator& EvaluateFilter)
{
    for (auto& Filter : Filters)
    {
        if (auto Increment = EvaluateFilter(Filter, BackendName); Increment > 0)
            return Increment;
    }

    return 0;
}

static bool IsSupportedGate(const FObjectiveEntry_Gate& Gate)
{
    if (Gate.GateType == EGateType::GuidBlocker)
        return true;

    return Gate.GateType == EGateType::Verb && !Gate.bUseOpenFilter && !Gate.bUseCloseFilter && !Gate.bUseAdvancedLogic && !Gate.bUseObjectiveProgressedCloseCount && !Gate.bUseSubjectTagQueries &&
           !Gate.bUseAutomaticOpenTime && Gate.GateResetBehavior == EGateResetBehavior::DoesNotReset;
}

static std::unordered_map<int32, FAccoladeQuest> AccoladeQuests;

static const FAccoladeQuest& GetAccoladeQuest(FName RowName, FFortEventConditional_Quest* Conditional)
{
    static auto GateExtension = FindObject<UScriptStruct>(L"/Script/FortniteGame.ObjectiveExtension_Gate");

    auto [It, bInserted] = AccoladeQuests.try_emplace(RowName.ComparisonIndex);
    auto& Quest = It->second;

    if (!bInserted)
        return Quest;

    auto Definition = (UFortQuestItemDefinition*)UKismetSystemLibrary::LoadAsset_Blocking(*(TSoftObjectPtr<UObject>*)&Conditional->Quest);

    if (!Definition)
        return Quest;

    for (auto Component : Definition->QuestDefinitionComponents)
    {
        if (!Component)
            continue;

        if (Component->IsA(UFortQuestDefinitionComponent_Accolade::StaticClass()))
            continue;

        if (!Component->IsA(UFortQuestDefinitionComponent_ObjectiveGate::StaticClass()))
            return Quest;

        for (auto& Pair : Component->ObjectivesExtensionData)
        {
            auto& Extension = Pair.Value();

            if (Extension.ScriptStruct != GateExtension || !Extension.StructMemory)
                return Quest;

            auto Gates = (FObjectiveExtension_Gate*)Extension.StructMemory;

            for (auto& Gate : Gates->ObjectiveGates)
            {
                if (!IsSupportedGate(Gate))
                    return Quest;
            }

            Quest.Gates[Pair.Key().ComparisonIndex] = Gates;
        }
    }

    Quest.Definition = Definition;

    return Quest;
}

static bool IsGateOpen(FAccoladeGateState& Gate, double Now)
{
    if (Gate.bOpen && Gate.CloseTime >= 0.0 && Now >= Gate.CloseTime)
    {
        Gate.bOpen = false;
        Gate.CloseTime = -1.0;
    }

    return Gate.bOpen;
}

static bool AreObjectiveGatesOpen(FObjectiveExtension_Gate* Gates, std::vector<FAccoladeGateState>& GateStates, double Now)
{
    bool bAnyVerbGate = false;
    bool bAnyOpen = false;
    bool bAllOpen = true;

    for (int32 i = 0; i < Gates->ObjectiveGates.Num(); i++)
    {
        if (Gates->ObjectiveGates[i].GateType != EGateType::Verb)
            continue;

        bool bOpen = IsGateOpen(GateStates[i], Now);
        bAnyVerbGate = true;
        bAnyOpen |= bOpen;
        bAllOpen &= bOpen;
    }

    if (!bAnyVerbGate)
        return true;

    return Gates->GateCheckType == EGateCheckType::RequireAny ? bAnyOpen : bAllOpen;
}

static void UpdateObjectiveGates(FObjectiveExtension_Gate* Gates, std::vector<FAccoladeGateState>& GateStates, const FFilterEvaluator& EvaluateFilter, double Now)
{
    if (GateStates.size() != (size_t)Gates->ObjectiveGates.Num())
    {
        GateStates.assign(Gates->ObjectiveGates.Num(), FAccoladeGateState{});

        for (int32 i = 0; i < Gates->ObjectiveGates.Num(); i++)
            GateStates[i].bOpen = Gates->ObjectiveGates[i].bShouldGateStartOpen;
    }

    for (int32 i = 0; i < Gates->ObjectiveGates.Num(); i++)
    {
        auto& Gate = Gates->ObjectiveGates[i];

        if (Gate.GateType != EGateType::Verb)
            continue;

        if (Gate.bUseOpenVerbFilter && EvaluateFilters(Gate.OpenVerbFilters, Gate.GateName, EvaluateFilter) > 0)
        {
            GateStates[i].bOpen = true;
            GateStates[i].CloseTime = Gate.bUseAutomaticCloseTime ? Now + Gate.TimeToCloseGateAfterOpen : -1.0;
        }

        if (Gate.bUseCloseVerbFilter && EvaluateFilters(Gate.CloseVerbFilters, Gate.GateName, EvaluateFilter) > 0)
        {
            GateStates[i].bOpen = false;
            GateStates[i].CloseTime = -1.0;
        }
    }
}

static bool ProcessAccoladeQuest(const FAccoladeQuest& Quest, FAccoladeQuestState& State, const FFilterEvaluator& EvaluateFilter, double Now)
{
    for (auto& [ObjectiveName, Gates] : Quest.Gates)
        UpdateObjectiveGates(Gates, State.Gates[ObjectiveName], EvaluateFilter, Now);

    bool bProgressed = false;
    bool bCompleted = true;

    for (auto& Objective : Quest.Definition->Objectives)
    {
        auto& Count = State.ObjectiveCounts[Objective.BackendName.ComparisonIndex];
        auto GatesIt = Quest.Gates.find(Objective.BackendName.ComparisonIndex);

        if (Count < Objective.Count && (GatesIt == Quest.Gates.end() || AreObjectiveGatesOpen(GatesIt->second, State.Gates[Objective.BackendName.ComparisonIndex], Now)))
        {
            if (auto Increment = EvaluateFilters(Objective.ObjectiveFilters, Objective.BackendName, EvaluateFilter); Increment > 0)
            {
                Count = (std::min)(Count + Increment, Objective.Count);
                bProgressed = true;
            }
        }

        bCompleted &= Count >= Objective.Count;
    }

    if (!bProgressed || !bCompleted)
        return false;

    State.ObjectiveCounts.clear();

    return true;
}

static int32 GetRewardXP(FFortAccoladesTableRow* Row, bool& bUseRestedExperience)
{
    static auto ResourceReward = FindObject<UScriptStruct>(L"/Script/FortniteGame.FortEventReaction_ResourceReward");
    static auto ResourceEntry = FindObject<UScriptStruct>(L"/Script/FortniteGame.FortRewardEntry_Resource");
    static auto SeasonalXP = FName(L"AthenaSeasonalXP");

    int32 XP = 0;

    for (auto& Reward : Row->EventRewards)
    {
        if (Reward.ScriptStruct != ResourceReward || !Reward.StructMemory)
            continue;

        for (auto& Entry : ((FFortEventReaction_ResourceReward*)Reward.StructMemory)->ResourceRewards)
        {
            auto Resource = Entry.ScriptStruct == ResourceEntry ? (FFortRewardEntry_Resource*)Entry.StructMemory : nullptr;

            if (!Resource || Resource->ResourceDefinition.ObjectID.AssetPath.AssetName.ComparisonIndex != SeasonalXP.ComparisonIndex)
                continue;

            XP += Resource->ResourceCount;
            bUseRestedExperience |= Resource->bUseRestedExperience;
        }
    }

    return XP;
}

static void TriggerAccolade(AFortPlayerControllerAthena* PlayerController, FAccoladePlayerState& State, FName RowName, FFortAccoladesTableRow* Row, FGameplayTagContainer* ContextTags)
{
    static auto GetAccoladesComponent = (UFortControllerComponent_Accolades* (*)(AActor*))(ImageBase + 0x5BB9F00);
    static auto BroadcastAccoladeTriggered = (void (*)(FFortCompletedAccoladeData*, UFortControllerComponent_Accolades*))(ImageBase + 0xB48E43C);
    static auto ConsumeRestedXP = (int32 (*)(uint8*, int32, bool))(ImageBase + 0xA3BAA90);
    static auto ConstructXPEventInfo = (FXPEventInfo* (*)(FXPEventInfo*))(ImageBase + 0x2B96F88);
    static auto DestroyXPEventInfo = (void (*)(FXPEventInfo*))(ImageBase + 0x8DE3EE8);
    static auto SendEventsForAccolade = (void (*)(UFortQuestManagerComponent_Athena*, FXPEventInfo*, UFortAccoladeItemDefinition*, int32, int32, int32, int32, int32, int32,
                                                  FGameplayTagContainer*))(ImageBase + 0xA3AD2FC);

    State.TriggerCounts[RowName.ComparisonIndex]++;
    MatchTriggerCounts[RowName.ComparisonIndex]++;

    bool bUseRestedExperience = false;
    auto XP = GetRewardXP(Row, bUseRestedExperience);
    auto Component = GetQuestManagerComponent(PlayerController);
    auto RestedXP = Component && bUseRestedExperience && XP > 0 ? ConsumeRestedXP((uint8*)Component + 0x474, XP, true) : 0;
    auto TotalXP = XP + RestedXP;

    if (auto AccoladesComponent = GetAccoladesComponent(PlayerController))
    {
        FFortCompletedAccoladeData Completed{};
        Completed.ReplicationID = -1;
        Completed.ReplicationKey = -1;
        Completed.MostRecentArrayReplicationKey = -1;
        Completed.AccoladeRowName = RowName;
        Completed.XPAwarded = TotalXP;
        Completed.AccoladeType = Row->AccoladeType;

        auto& CompletedAccolades = AccoladesComponent->CompletedAccoladeDataArray;
        auto& CompletedAccolade = CompletedAccolades.CompletedAccolades.Add(Completed);
        CompletedAccolades.MarkItemDirty(CompletedAccolade);
        BroadcastAccoladeTriggered(&CompletedAccolade, AccoladesComponent);
    }

    if (!Component || TotalXP <= 0)
        return;

    *(int32*)((uint8*)Component + 0x468) += TotalXP;
    *(int32*)((uint8*)Component + 0x46C) += TotalXP;

    alignas(16) uint8 InfoStorage[sizeof(FXPEventInfo)];
    auto Info = ConstructXPEventInfo((FXPEventInfo*)InfoStorage);

    Info->EventName = RowName;
    Info->Priority = Row->Priority;
    Info->EventXpValue = TotalXP;
    Info->TotalXpEarnedInMatch = *(int32*)((uint8*)Component + 0x46C);
    Info->RestedValuePortion = RestedXP;
    Info->RestedXPRemaining = *(int32*)((uint8*)Component + 0x474) - *(int32*)((uint8*)Component + 0x478);
    Info->PlayerController = PlayerController;

    SendEventsForAccolade(Component, Info, nullptr, 0, TotalXP, 0, 0, 0, TotalXP, ContextTags);

    DestroyXPEventInfo(Info);
}

void ProcessAccoladeEvent(AFortPlayerControllerAthena* PlayerController, FGameplayTagContainer* ContextTags, const FFilterEvaluator& EvaluateFilter)
{
    static auto GetProductTag = (FGameplayTag* (*)(FGameplayTag*, UObject*))(ImageBase + 0x9D70C74);
    static auto ConstructTagContainer = (FGameplayTagContainer* (*)(FGameplayTagContainer*, const FGameplayTag*))(ImageBase + 0x21FC89C);
    static auto VerbConditional = FindObject<UScriptStruct>(L"/Script/FortniteGame.FortEventConditional_Verb");
    static auto TrackedVerbConditional = FindObject<UScriptStruct>(L"/Script/FortniteGame.FortEventConditional_TrackedVerb");
    static auto QuestConditional = FindObject<UScriptStruct>(L"/Script/FortniteGame.FortEventConditional_Quest");

    auto Table = PlayerController ? GetAccoladesTable() : nullptr;

    if (!Table || Table->Num() == 0)
        return;

    FGameplayTag ProductTag{};
    GetProductTag(&ProductTag, PlayerController);

    FGameplayTagContainer ProductTags{};
    ConstructTagContainer(&ProductTags, &ProductTag);

    auto& State = GetPlayerState(PlayerController);
    auto Now = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
    std::vector<std::pair<FName, FFortAccoladesTableRow*>> Triggered;

    for (auto& Pair : *Table)
    {
        auto RowName = Pair.Key().ItemName;
        auto Row = Pair.Value();

        if (!Row || !Row->EventConditional.StructMemory || !CanTriggerAccolade(State, RowName, Row) || !IsCompatibleWithProduct(Row, ProductTags))
            continue;

        auto Struct = Row->EventConditional.ScriptStruct;
        auto Memory = Row->EventConditional.StructMemory;

        if (Struct == VerbConditional)
        {
            if (EvaluateFilters(((FFortEventConditional_Verb*)Memory)->VerbFilters, RowName, EvaluateFilter) > 0)
                Triggered.push_back({ RowName, Row });
        }
        else if (Struct == TrackedVerbConditional)
        {
            auto Conditional = (FFortEventConditional_TrackedVerb*)Memory;
            auto Increment = EvaluateFilters(Conditional->VerbFilters, RowName, EvaluateFilter);

            if (Increment <= 0 || Conditional->Amount == 0)
                continue;

            auto& Amount = State.TrackedAmounts[RowName.ComparisonIndex];
            Amount += Increment;

            if (Amount < (int64)Conditional->Amount)
                continue;

            Amount -= Conditional->Amount;
            Triggered.push_back({ RowName, Row });
        }
        else if (Struct == QuestConditional)
        {
            auto& Quest = GetAccoladeQuest(RowName, (FFortEventConditional_Quest*)Memory);

            if (Quest.Definition && ProcessAccoladeQuest(Quest, State.Quests[RowName.ComparisonIndex], EvaluateFilter, Now))
                Triggered.push_back({ RowName, Row });
        }
    }

    FreeGameplayTags(ProductTags);

    for (auto& [RowName, Row] : Triggered)
    {
        if (CanTriggerAccolade(State, RowName, Row))
            TriggerAccolade(PlayerController, State, RowName, Row, ContextTags);
    }
}

static void AddGameplayEventTags(TArray<FInstancedStruct>& Filters, std::unordered_set<int32>& Tags)
{
    static auto GameplayEventFilter = FindObject<UScriptStruct>(L"/Script/FortniteGame.FortObjectiveFilter_PlayerReceivedGameplayEvent");

    for (auto& Filter : Filters)
    {
        if (Filter.ScriptStruct == GameplayEventFilter && Filter.StructMemory)
            Tags.insert(((FFortObjectiveFilter_PlayerReceivedGameplayEvent*)Filter.StructMemory)->GameplayEventTag.TagName.ComparisonIndex);
    }
}

static std::unordered_set<int32> GameplayEventTags;
static bool bGameplayEventTagsCached = false;

bool HasAccoladeGameplayEvent(FGameplayTag EventTag)
{
    static auto VerbConditional = FindObject<UScriptStruct>(L"/Script/FortniteGame.FortEventConditional_Verb");
    static auto TrackedVerbConditional = FindObject<UScriptStruct>(L"/Script/FortniteGame.FortEventConditional_TrackedVerb");
    static auto QuestConditional = FindObject<UScriptStruct>(L"/Script/FortniteGame.FortEventConditional_Quest");

    if (!bGameplayEventTagsCached)
    {
        auto Table = GetAccoladesTable();

        if (!Table || Table->Num() == 0)
            return false;

        bGameplayEventTagsCached = true;

        for (auto& Pair : *Table)
        {
            auto Row = Pair.Value();
            auto Memory = Row ? Row->EventConditional.StructMemory : nullptr;

            if (!Memory)
                continue;

            if (Row->EventConditional.ScriptStruct == VerbConditional)
                AddGameplayEventTags(((FFortEventConditional_Verb*)Memory)->VerbFilters, GameplayEventTags);
            else if (Row->EventConditional.ScriptStruct == TrackedVerbConditional)
                AddGameplayEventTags(((FFortEventConditional_TrackedVerb*)Memory)->VerbFilters, GameplayEventTags);
            else if (Row->EventConditional.ScriptStruct == QuestConditional)
            {
                auto& Quest = GetAccoladeQuest(Pair.Key().ItemName, (FFortEventConditional_Quest*)Memory);

                if (!Quest.Definition)
                    continue;

                for (auto& Objective : Quest.Definition->Objectives)
                    AddGameplayEventTags(Objective.ObjectiveFilters, GameplayEventTags);

                for (auto& [ObjectiveName, Gates] : Quest.Gates)
                {
                    for (auto& Gate : Gates->ObjectiveGates)
                    {
                        AddGameplayEventTags(Gate.OpenVerbFilters, GameplayEventTags);
                        AddGameplayEventTags(Gate.CloseVerbFilters, GameplayEventTags);
                    }
                }
            }
        }
    }

    return GameplayEventTags.contains(EventTag.TagName.ComparisonIndex);
}

static void (*BeginPlayOG)(UFortGameStateComponent_Accolades*);
static void BeginPlay(UFortGameStateComponent_Accolades* Component)
{
    static auto OnWorldBeginPlay = (void (*)(UFortGameStateComponent_Accolades*))(ImageBase + 0x3CE5C34);

    BeginPlayOG(Component);

    auto Owner = Component->GetOwner();

    if (Owner && Owner->Role == ENetRole::ROLE_Authority)
        OnWorldBeginPlay(Component);
}

void UFortGameStateComponent_Accolades__PostLoad()
{
    Hook<UFortGameStateComponent_Accolades>(uint32(0x78), BeginPlay, BeginPlayOG);
}
