#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Quests/FortQuestManager.h"
#include "Engine/Source/Runtime/Engine/Public/ReplaySubsystem.h"
#include "FortniteGame/Plugins/GameFeatures/Accolades/Source/AccoladesRuntime/Public/Components/FortGameStateComponent_Accolades.h"
#include "FortniteGame/Plugins/GameFeatures/VictoryCrownsGameplay/Source/VictoryCrownsRuntime/Public/FortControllerComponent_VictoryCrowns.h"
#include "SDK/SDK/AccoladesRuntime_classes.hpp"
#include "SDK/SDK/FortniteGame_parameters.hpp"

INIT_POSTLOAD(UFortQuestManager);
INIT_TICKER(UFortQuestManager);

struct FFortQuestStatEvent
{
    EFortQuestObjectiveStatEvent Type;
    uint8 Pad_1[0x3];
    FWeakObjectPtr TargetObject;
    uint8 Pad_C[0x4];
    FGameplayTagContainer TargetTags;
    FGameplayTagContainer SourceTags;
    FGameplayTagContainer ContextTags;
    FDataTableRowHandle ObjectiveStat;
    FName ObjectiveBackendName;
    int32 Count;
    uint8 Pad_88[0x88];
};
static_assert(offsetof(FFortQuestStatEvent, TargetTags) == 0x10 && offsetof(FFortQuestStatEvent, ObjectiveStat) == 0x70 && offsetof(FFortQuestStatEvent, Count) == 0x84 && sizeof(FFortQuestStatEvent) == 0x110);

struct FFortQuestObjectiveCompletionEntry
{
    FString StatName;
    int32 Count;
    int32 TimestampOffset;
    uint8 Pad_18[0x8];
    TArray<FWeakObjectPtr> Quests;
};
static_assert(offsetof(FFortQuestObjectiveCompletionEntry, Quests) == 0x20 && sizeof(FFortQuestObjectiveCompletionEntry) == sizeof(FFortQuestObjectiveCompletion));

struct FFortQuestSaveRequest
{
    int32 SaveId;
    uint8 Pad_4[0x4];
    TArray<FFortQuestObjectiveCompletion> Changes;
    uint8 Pad_18[0x10];
};
static_assert(sizeof(FFortQuestSaveRequest) == 0x28);

struct FVerbMessageView
{
    UScriptStruct* ScriptStruct;
    FVerbMessage* Message;
};

enum class EQuestVerb : uint8
{
    DamageDone,
    Hit,
    HealingDone,
    HealingReceived,
    GainShields,
    DestroyBuilding,
    PawnEliminated,
    Custom,
    CurrencySpent,
    MatchFinished,
    PlayerReceivedGameplayEvent,
    PlayerDowned
};

struct FVerbProcessor
{
    EQuestVerb Verb;
    UScriptStruct* MessageStruct;
    UScriptStruct* FilterStruct;
    EFortQuestObjectiveStatEvent Type;
    bool bBroadcastByGame;
};

struct FQuestManagerFlushState
{
    FWeakObjectPtr QuestManager;
    FWeakObjectPtr PlayerController;
    std::unordered_map<int32, int32> ElapsedSeconds;
    std::unordered_map<int32, double> Remainders;
    std::vector<int32> GameplayEventTags;
    bool bGameplayEventTagsCached = false;
    double NextFlushTime = -1.0;
    int32 UpdateSaveId = -1;
    int32 EndSaveId = -1;
    double EndRequestTime = -1.0;
    bool bUpdateInFlight = false;
    bool bEndRequested = false;
    bool bEndInFlight = false;
    bool bEnded = false;
};

static std::unordered_map<UFortQuestManager*, FQuestManagerFlushState> FlushStates;
FPendingDamageTaken PendingDamageTaken;

static FQuestManagerFlushState& GetFlushState(UFortQuestManager* QuestManager)
{
    auto& State = FlushStates[QuestManager];

    if (!State.QuestManager.IsValid())
        State = { FWeakObjectPtr(QuestManager) };

    return State;
}

static AFortPlayerControllerAthena* GetPlayerController(UFortQuestManager* QuestManager)
{
    static auto GetPlayerControllerInternal = (AController* (*)(UFortQuestManager*))(ImageBase + 0xA34F954);

    auto PlayerController = GetPlayerControllerInternal(QuestManager);

    if (!PlayerController)
    {
        auto It = FlushStates.find(QuestManager);
        PlayerController = It != FlushStates.end() && It->second.QuestManager.Get() == QuestManager ? (AController*)It->second.PlayerController.Get() : nullptr;
    }

    return PlayerController && PlayerController->IsA(AFortPlayerControllerAthena::StaticClass()) ? (AFortPlayerControllerAthena*)PlayerController : nullptr;
}

static void LinkQuestManagerController(UFortQuestManager* QuestManager, AFortPlayerControllerAthena* PlayerController)
{
    static auto GetPlayerControllerInternal = (AController* (*)(UFortQuestManager*))(ImageBase + 0xA34F954);

    if (!QuestManager || !PlayerController)
        return;

    auto& State = GetFlushState(QuestManager);

    if (State.PlayerController.Get() != PlayerController)
        State.PlayerController = FWeakObjectPtr(PlayerController);

    auto PlayerInfo = QuestManager->Outer;

    if (!GetPlayerControllerInternal(QuestManager) && PlayerInfo && PlayerInfo->IsA(UFortRegisteredPlayerInfo::StaticClass()))
        *(FWeakObjectPtr*)(__int64(PlayerInfo) + 0x21C) = FWeakObjectPtr(PlayerController);
}

static UFortPlaylistAthena* GetCurrentPlaylist()
{
    return ((AFortGameStateAthena*)UWorld::GetWorld()->GameState)->CurrentPlaylistInfo.BasePlaylist;
}

static UFortMcpProfileSubgame* GetBoundProfile(UFortQuestManager* QuestManager)
{
    return QuestManager->BoundProfile.Get();
}

static TArray<UFortQuestItem*> GetProfileQuests(UFortQuestManager* QuestManager)
{
    static auto GetQuestItems = (void (*)(UFortMcpProfileSubgame*, uint8, TArray<UFortQuestItem*>*))(ImageBase + 0x9F58CE8);

    TArray<UFortQuestItem*> Quests;

    if (auto Profile = GetBoundProfile(QuestManager))
        GetQuestItems(Profile, 0, &Quests);

    return Quests;
}

static void ReapplyPendingChanges(UFortQuestManager* QuestManager)
{
    static auto ReapplyPendingChangesInternal = (void (*)(UFortQuestManager*))(ImageBase + 0xA357B90);

    ReapplyPendingChangesInternal(QuestManager);
}

static void ScheduleFlush(UFortQuestManager* QuestManager)
{
    auto& State = GetFlushState(QuestManager);

    if (State.NextFlushTime >= 0.0)
        return;

    State.NextFlushTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld()) + QuestManager->DamageEventFlushDelaySeconds;
}

static bool DoesTagQueryMatch(const FGameplayTagQuery& TagQuery, const FGameplayTagContainer& Tags)
{
    return TagQuery.QueryTokenStream.Num() == 0 || UBlueprintGameplayTagLibrary::DoesContainerMatchTagQuery(Tags, TagQuery);
}

static bool DoesLocationMatch(const FGameplayTagQuery& TagQuery, FFortQuestStatEvent* Event)
{
    return TagQuery.QueryTokenStream.Num() == 0 || UBlueprintGameplayTagLibrary::DoesContainerMatchTagQuery(Event->SourceTags, TagQuery) ||
           UBlueprintGameplayTagLibrary::DoesContainerMatchTagQuery(Event->TargetTags, TagQuery);
}

template <typename TRange, typename TValue>
static bool IsInRange(const TRange& Range, TValue Value)
{
    if ((Range.LowerBound.Type == ERangeBoundTypes::Inclusive && Value < Range.LowerBound.Value) || (Range.LowerBound.Type == ERangeBoundTypes::Exclusive && Value <= Range.LowerBound.Value))
        return false;

    if ((Range.UpperBound.Type == ERangeBoundTypes::Inclusive && Value > Range.UpperBound.Value) || (Range.UpperBound.Type == ERangeBoundTypes::Exclusive && Value >= Range.UpperBound.Value))
        return false;

    return true;
}

static bool IsRequirementMet(EObjectiveProcessorPropertyRequirement Requirement, bool bValue)
{
    return Requirement == EObjectiveProcessorPropertyRequirement::Ignored || (Requirement == EObjectiveProcessorPropertyRequirement::Required) == bValue;
}

static int32 AccumulateProgress(UFortQuestManager* QuestManager, FName BackendName, double Amount)
{
    auto& Remainder = GetFlushState(QuestManager).Remainders[BackendName.ComparisonIndex];
    Remainder += Amount;

    auto Whole = (int32)Remainder;
    Remainder -= Whole;

    return Whole;
}

static void GetSubjectTags(UObject* Subject, FGameplayTagContainer* Tags)
{
    static auto GetSubjectTagsInternal = (void (*)(UObject*, FGameplayTagContainer*))(ImageBase + 0x30DB558);

    GetSubjectTagsInternal(Subject, Tags);
}

static void SetSubject(FSubjectTagsPair* Pair, UObject* Subject)
{
    static auto SetSubjectInternal = (void (*)(FSubjectTagsPair*, UObject*))(ImageBase + 0x30DB38C);

    SetSubjectInternal(Pair, Subject);
}

static UScriptStruct* FindFortniteGameStruct(const wchar_t* Name)
{
    return FindObject<UScriptStruct>((std::wstring(L"/Script/FortniteGame.") + Name).c_str());
}

static const std::vector<FVerbProcessor>& GetVerbProcessors()
{
    static std::vector<FVerbProcessor> Processors = {
        { EQuestVerb::DamageDone, FindFortniteGameStruct(L"FortVerbMessage_DamageDone"), FindFortniteGameStruct(L"FortObjectiveFilter_DamageDone"), EFortQuestObjectiveStatEvent::Damage, false },
        { EQuestVerb::Hit, FindFortniteGameStruct(L"FortVerbMessage_Hit"), FindFortniteGameStruct(L"FortObjectiveFilter_Hit"), EFortQuestObjectiveStatEvent::Hit, false },
        { EQuestVerb::HealingDone, FindFortniteGameStruct(L"FortVerbMessage_HealingDone"), FindFortniteGameStruct(L"FortObjectiveFilter_HealingDone"), EFortQuestObjectiveStatEvent::Heal, true },
        { EQuestVerb::HealingReceived, FindFortniteGameStruct(L"FortVerbMessage_HealingReceived"), FindFortniteGameStruct(L"FortObjectiveFilter_HealingReceived"), EFortQuestObjectiveStatEvent::Heal, true },
        { EQuestVerb::GainShields, FindFortniteGameStruct(L"FortVerbMessage_GainShields"), FindFortniteGameStruct(L"FortObjectiveFilter_GainShields"), EFortQuestObjectiveStatEvent::Heal, true },
        { EQuestVerb::DestroyBuilding, FindFortniteGameStruct(L"FortVerbMessage_DestroyBuilding"), FindFortniteGameStruct(L"FortObjectiveFilter_DestroyBuilding"), EFortQuestObjectiveStatEvent::Destroy, true },
        { EQuestVerb::PawnEliminated, FindFortniteGameStruct(L"FortVerbMessage_PawnEliminated"), FindFortniteGameStruct(L"FortObjectiveFilter_PawnEliminated"), EFortQuestObjectiveStatEvent::Kill, true },
        { EQuestVerb::Custom, FindFortniteGameStruct(L"VerbMessage_Custom"), FindFortniteGameStruct(L"ObjectiveFilter_Custom"), EFortQuestObjectiveStatEvent::Custom, true },
        { EQuestVerb::CurrencySpent, FindFortniteGameStruct(L"FortVerbMessage_CurrencySpent"), FindFortniteGameStruct(L"FortObjectiveFilter_CurrencySpent"), EFortQuestObjectiveStatEvent::Custom, true },
        { EQuestVerb::MatchFinished, FindFortniteGameStruct(L"FortVerbMessage_MatchFinished"), FindFortniteGameStruct(L"FortObjectiveFilter_MatchFinished"), EFortQuestObjectiveStatEvent::Finish, false },
        { EQuestVerb::PlayerReceivedGameplayEvent, FindFortniteGameStruct(L"FortVerbMessage_PlayerReceivedGameplayEvent"), FindFortniteGameStruct(L"FortObjectiveFilter_PlayerReceivedGameplayEvent"),
          EFortQuestObjectiveStatEvent::Custom, false },
        { EQuestVerb::PlayerDowned, FindFortniteGameStruct(L"FortVerbMessage_PlayerDowned"), FindFortniteGameStruct(L"FortObjectiveFilter_PlayerDowned"), EFortQuestObjectiveStatEvent::DownOrElim, true },
    };

    return Processors;
}

static const FVerbProcessor& GetVerbProcessor(EQuestVerb Verb)
{
    return GetVerbProcessors()[(int32)Verb];
}

AFortPlayerControllerAthena* ResolvePlayerController(UObject* Object)
{
    if (Object && Object->IsA(APawn::StaticClass()))
        Object = ((APawn*)Object)->Controller;
    else if (Object && Object->IsA(APlayerState::StaticClass()))
        Object = ((APlayerState*)Object)->Owner;

    return Object && Object->IsA(AFortPlayerControllerAthena::StaticClass()) ? (AFortPlayerControllerAthena*)Object : nullptr;
}

static EDistanceTravelMode GetTravelMode(AFortPlayerPawn* Pawn)
{
    if (Pawn->IsInVehicle())
        return Pawn->IsDrivingVehicle() ? EDistanceTravelMode::Driving : EDistanceTravelMode::Passenger;

    auto Movement = Pawn->CharacterMovement;

    if (!Movement)
        return EDistanceTravelMode::EDistanceTravelMode_MAX;

    switch (Movement->MovementMode)
    {
    case EMovementMode::MOVE_Walking:
    case EMovementMode::MOVE_NavWalking:
        if (Pawn->bIsSliding)
            return EDistanceTravelMode::Sliding;

        if (Pawn->bIsCrouched)
            return EDistanceTravelMode::Crouch;

        return Pawn->CurrentMovementStyle == EFortMovementStyle::Sprinting ? EDistanceTravelMode::Sprinting : EDistanceTravelMode::Running;
    case EMovementMode::MOVE_Falling:
        return Pawn->bIsParachuteOpen ? EDistanceTravelMode::Parachuting : Pawn->bIsSkydiving ? EDistanceTravelMode::SkyDiving : EDistanceTravelMode::Falling;
    case EMovementMode::MOVE_Swimming:
        return EDistanceTravelMode::Swimming;
    case EMovementMode::MOVE_Flying:
        return EDistanceTravelMode::Flying;
    case EMovementMode::MOVE_Custom:
        break;
    default:
        return EDistanceTravelMode::EDistanceTravelMode_MAX;
    }

    switch ((EFortCustomMovement)Movement->CustomMovementMode)
    {
    case EFortCustomMovement::Driving:
        return EDistanceTravelMode::Driving;
    case EFortCustomMovement::Passenger:
        return EDistanceTravelMode::Passenger;
    case EFortCustomMovement::Parachuting:
        return EDistanceTravelMode::Parachuting;
    case EFortCustomMovement::Skydiving:
        return EDistanceTravelMode::SkyDiving;
    case EFortCustomMovement::SkydiveFollowing:
        return EDistanceTravelMode::SkydiveFollowing;
    case EFortCustomMovement::Hover:
        return EDistanceTravelMode::Hover;
    case EFortCustomMovement::RemoteControl_Flying:
        return EDistanceTravelMode::RemoteControl_Flying;
    case EFortCustomMovement::Ziplining:
        return EDistanceTravelMode::Ziplining;
    case EFortCustomMovement::ZipliningOnSpline:
        return EDistanceTravelMode::ZipliningOnSpline;
    case EFortCustomMovement::Grinding:
        return EDistanceTravelMode::Grinding;
    case EFortCustomMovement::Ballooning:
        return EDistanceTravelMode::Ballooning;
    case EFortCustomMovement::SurfaceSwimming:
        return EDistanceTravelMode::SurfaceSwimming;
    case EFortCustomMovement::DBNOCarried:
        return EDistanceTravelMode::DBNOCarried;
    case EFortCustomMovement::Floating:
        return EDistanceTravelMode::Floating;
    case EFortCustomMovement::Goop:
        return EDistanceTravelMode::Goop;
    case EFortCustomMovement::Sliding:
        return EDistanceTravelMode::Sliding;
    case EFortCustomMovement::Skating:
        return EDistanceTravelMode::Skating;
    default:
        return EDistanceTravelMode::EDistanceTravelMode_MAX;
    }
}

static bool IsTravelModeAllowed(const TArray<EDistanceTravelMode>& Allowed, const TArray<EDistanceTravelMode>& Prohibited, EDistanceTravelMode Mode)
{
    for (auto Entry : Prohibited)
    {
        if (Entry == Mode)
            return false;
    }

    for (auto Entry : Allowed)
    {
        if (Entry == EDistanceTravelMode::Any || Entry == Mode)
            return true;
    }

    return Allowed.Num() == 0;
}

template <typename T>
static bool IsAllowed(const TArray<T>& Allowed, const TArray<T>& Prohibited, T Value)
{
    for (auto Entry : Prohibited)
    {
        if (Entry == Value)
            return false;
    }

    if (Allowed.Num() == 0)
        return true;

    for (auto Entry : Allowed)
    {
        if (Entry == Value)
            return true;
    }

    return false;
}

static AFortPlayerStateAthena* GetTargetPlayerState(UObject* TargetObject)
{
    auto TargetPawn = TargetObject && TargetObject->IsA(APawn::StaticClass()) ? (APawn*)TargetObject : nullptr;

    return TargetPawn && TargetPawn->PlayerState && TargetPawn->PlayerState->IsA(AFortPlayerStateAthena::StaticClass()) ? (AFortPlayerStateAthena*)TargetPawn->PlayerState : nullptr;
}

static EFortTeamAffiliation GetTeamAffiliation(AFortPlayerControllerAthena* PlayerController, UObject* TargetObject)
{
    auto TargetPlayerState = GetTargetPlayerState(TargetObject);
    auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;

    if (!TargetPlayerState || !PlayerState)
        return EFortTeamAffiliation::Neutral;

    return TargetPlayerState->TeamIndex == PlayerState->TeamIndex ? EFortTeamAffiliation::Friendly : EFortTeamAffiliation::Hostile;
}

static int32 EvaluateVerbFilter(void* Memory, UFortQuestManager* QuestManager, FName BackendName, AFortPlayerControllerAthena* PlayerController, const FVerbProcessor& Verb, FVerbMessage* Message)
{
    switch (Verb.Verb)
    {
    case EQuestVerb::DamageDone:
    {
        auto Data = (FFortObjectiveFilter_DamageDone*)Memory;
        auto Damage = (FFortVerbMessage_DamageDone*)Message;

        if ((Damage->bDamageDoneToSelf && !Data->bAllowSelfInflictedDamage) || !IsRequirementMet(Data->SelfInflictedDamage, Damage->bDamageDoneToSelf) ||
            !IsRequirementMet(Data->TeamDamage, Damage->bDamageDoneToTeamMember) || (Data->bRequireCriticalHit && Damage->DamageZone != EFortDamageZone::Critical) ||
            (Data->bUseDistance && !IsInRange(Data->Distance, Damage->Distance)) || (Data->bUseDamageAmount && !IsInRange(Data->DamageAmount, Damage->Damage)) ||
            (Data->bUseUncappedDamage && !IsInRange(Data->UncappedDamageAmount, Damage->UncappedDamage)))
            return 0;

        if (!DoesTagQueryMatch(Data->DamageSource.TagQuery, Damage->DamageSource.Tags) || !DoesTagQueryMatch(Data->DamagedActor.TagQuery, Damage->DamagedActor.Tags) ||
            !DoesTagQueryMatch(Data->DamageType.TagQuery, Damage->DamageType.Tags))
            return 0;

        return Data->bIncrementByUncappedDamage ? Damage->UncappedDamage : Data->bIncrementByDamageAmount ? Damage->Damage : 1;
    }
    case EQuestVerb::Hit:
    {
        auto Data = (FFortObjectiveFilter_Hit*)Memory;
        auto Hit = (FFortVerbMessage_Hit*)Message;

        if ((Hit->bHitSelf && !Data->bAllowSelfInflictedDamage) || !IsRequirementMet(Data->SelfInflictedDamage, Hit->bHitSelf) || !IsRequirementMet(Data->TeamDamage, Hit->bHitTeamMember) ||
            (Data->bRequireCriticalHit && Hit->DamageZone != EFortDamageZone::Critical) || (Data->bUseDistance && !IsInRange(Data->Distance, Hit->Distance)))
            return 0;

        if (!DoesTagQueryMatch(Data->DamageSource.TagQuery, Hit->DamageSource.Tags) || !DoesTagQueryMatch(Data->DamagedActor.TagQuery, Hit->DamagedActor.Tags) ||
            !DoesTagQueryMatch(Data->DamageType.TagQuery, Hit->DamageType.Tags))
            return 0;

        return Data->bIncrementByDistance ? AccumulateProgress(QuestManager, BackendName, Hit->Distance) : 1;
    }
    case EQuestVerb::HealingDone:
    case EQuestVerb::HealingReceived:
    {
        auto Data = (FFortObjectiveFilter_HealingBase*)Memory;
        auto Healing = (FFortVerbMessage_Healing*)Message;
        bool bTeammate = Verb.Verb == EQuestVerb::HealingDone ? ((FFortVerbMessage_HealingDone*)Message)->bHealedTeammate : ((FFortVerbMessage_HealingReceived*)Message)->bHealedByTeammate;

        if ((Data->SelfHealingBehavior == EFortSelfHealingHandling::Required && bTeammate) || (Data->SelfHealingBehavior == EFortSelfHealingHandling::Prohibited && !bTeammate) ||
            (Data->bRequireHealedToFull && !Healing->bHealedToFull) || (Data->bUseHealingAmount && !IsInRange(Data->HealingAmount, Healing->AmountHealedToHealth)) ||
            (Data->bUseOverHealing && !IsInRange(Data->HealthOverHealed, Healing->HealthOverHealingAmount)) || (Data->bUsePercentHealed && !IsInRange(Data->PercentHealed, Healing->PercentHealed)))
            return 0;

        if (!DoesTagQueryMatch(Data->HealingType.TagQuery, Healing->HealingType.Tags))
            return 0;

        if (Verb.Verb == EQuestVerb::HealingDone)
        {
            auto Done = (FFortObjectiveFilter_HealingDone*)Memory;

            if ((Done->bRequireHealingDoneToTeammate && !bTeammate) || !DoesTagQueryMatch(Done->SubjectHealed.TagQuery, ((FFortVerbMessage_HealingDone*)Message)->SubjectHealed.Tags))
                return 0;
        }
        else
        {
            auto Received = (FFortObjectiveFilter_HealingReceived*)Memory;

            if ((Received->bRequireHealingReceivedFromTeammate && !bTeammate) || !DoesTagQueryMatch(Received->HealSource.TagQuery, ((FFortVerbMessage_HealingReceived*)Message)->HealSource.Tags))
                return 0;
        }

        switch (Data->IncrementBy)
        {
        case EFortHealingIncrementType::Single:
            return 1;
        case EFortHealingIncrementType::HealthOverHealing:
            return AccumulateProgress(QuestManager, BackendName, Healing->HealthOverHealingAmount);
        case EFortHealingIncrementType::PercentHealed:
            return AccumulateProgress(QuestManager, BackendName, Healing->PercentHealed);
        default:
            return AccumulateProgress(QuestManager, BackendName, Healing->AmountHealedToHealth);
        }
    }
    case EQuestVerb::GainShields:
    {
        auto Data = (FFortObjectiveFilter_GainShields*)Memory;
        auto Shields = (FFortVerbMessage_GainShields*)Message;

        if ((Data->bRequireFullShields && !Shields->bGainedFullShields) || (Data->bRequireShieldGainedFromTeammate && !Shields->bGainedShieldsFromTeammate) || Data->bUseTotalShieldsGained ||
            (Data->bRequireAuthorIsTheShieldGainSource && ResolvePlayerController(Shields->ShieldGainSource.Subject.Get()) != PlayerController) ||
            (Data->bUseShieldAmountGained && !IsInRange(Data->AmountOfShieldsGained, Shields->AmountOfShieldsGained)) ||
            (Data->bUseOverFullShields && !IsInRange(Data->ShieldsGainedOverFullAmount, Shields->ShieldsGainedOverFullAmount)) ||
            (Data->bUseShieldGainedPercent && !IsInRange(Data->ShieldsPercentGained, Shields->ShieldPercentGained)))
            return 0;

        if (!DoesTagQueryMatch(Data->ShieldGainSource.TagQuery, Shields->ShieldGainSource.Tags) || !DoesTagQueryMatch(Data->ShieldGainType.TagQuery, Shields->ShieldGainType.Tags))
            return 0;

        switch (Data->IncrementBy)
        {
        case EFortShieldGainIncrementType::Single:
            return 1;
        case EFortShieldGainIncrementType::ShieldGainedOverFull:
            return AccumulateProgress(QuestManager, BackendName, Shields->ShieldsGainedOverFullAmount);
        case EFortShieldGainIncrementType::ShieldGainPercent:
            return AccumulateProgress(QuestManager, BackendName, Shields->ShieldPercentGained);
        default:
            return AccumulateProgress(QuestManager, BackendName, Shields->AmountOfShieldsGained);
        }
    }
    case EQuestVerb::DestroyBuilding:
    {
        auto Data = (FFortObjectiveFilter_DestroyBuilding*)Memory;
        auto Destroy = (FFortVerbMessage_DestroyBuilding*)Message;
        auto Pawn = PlayerController->Pawn && PlayerController->Pawn->IsA(AFortPlayerPawn::StaticClass()) ? (AFortPlayerPawn*)PlayerController->Pawn : nullptr;

        if (Data->bUseAdvancedVehicleFilter || (Data->bRequireInVehicle && (!Pawn || !Pawn->IsInVehicle())))
            return 0;

        return DoesTagQueryMatch(Data->DestroyedBuilding.TagQuery, Destroy->DestroyedBuilding.Tags) && DoesTagQueryMatch(Data->DamageSource.TagQuery, Destroy->DamageSource.Tags) ? 1 : 0;
    }
    case EQuestVerb::PawnEliminated:
    {
        auto Data = (FFortObjectiveFilter_PawnEliminated*)Memory;
        auto Elimination = (FFortVerbMessage_PawnEliminated*)Message;
        auto KillType = Elimination->bKilledPawnWasPlayer ? EEliminationKillType::Player : Elimination->bKilledPawnWasWildlife ? EEliminationKillType::Wildlife : EEliminationKillType::NPC;

        if ((Elimination->bWasSelfElimination && !Data->bAllowSelfEliminations) || (Data->bRequirePawnWasEliminatedByConvertedPawn && !Elimination->bWasEliminatedByConvertedPawn) ||
            !IsAllowed(Data->AllowedTeamAffiliations, Data->ProhibitedTeamAffiliations, Elimination->EliminatedAndKillerTeamAffiliation) ||
            !IsAllowed(Data->AllowedKilledPawnTypes, Data->ProhibitedKilledPawnTypes, KillType) || (Data->bUseDistance && !IsInRange(Data->Distance, Elimination->Distance)) ||
            (Data->bUseKillStreak && !IsInRange(Data->KillStreak, Elimination->KillStreak)))
            return 0;

        if (!DoesTagQueryMatch(Data->EliminatedPawn.TagQuery, Elimination->EliminatedPawn.Tags) || !DoesTagQueryMatch(Data->Killer.TagQuery, Elimination->Killer.Tags) ||
            !DoesTagQueryMatch(Data->EliminationSource.TagQuery, Elimination->EliminationSource.Tags))
            return 0;

        return Data->bIncrementByDistance ? AccumulateProgress(QuestManager, BackendName, Elimination->Distance) : 1;
    }
    case EQuestVerb::Custom:
    {
        auto Data = (FObjectiveFilter_Custom*)Memory;
        auto Custom = (FVerbMessage_Custom*)Message;

        if (Data->Verb.TagName != Custom->Verb.TagName || Data->Subjects.Num() > 0 || Data->Values.Num() > 0 || Data->bIncrementByNamedValue)
            return 0;

        return (std::max)(Custom->Amount, 1);
    }
    case EQuestVerb::CurrencySpent:
    {
        auto Data = (FFortObjectiveFilter_CurrencySpent*)Memory;
        auto Spent = (FFortVerbMessage_CurrencySpent*)Message;

        if ((Data->AllowedMessages == ECurrencySpentFilterEventBehavior::OnlyGeneral && Spent->bIsFromSpecificEvent) ||
            (Data->AllowedMessages == ECurrencySpentFilterEventBehavior::OnlySpecific && !Spent->bIsFromSpecificEvent) ||
            (Data->bUseSpentAmount && !IsInRange(Data->SpentAmountToCountForProgress, Spent->AmountSpent)) || !DoesTagQueryMatch(Data->PurchaseTags.TagQuery, Spent->PurchaseTags.Tags))
            return 0;

        return Data->bIncrementByAmountSpent ? Spent->AmountSpent : 1;
    }
    case EQuestVerb::MatchFinished:
    {
        auto Data = (FFortObjectiveFilter_MatchFinished*)Memory;
        auto Finished = (FFortVerbMessage_MatchFinished*)Message;

        if ((Data->bUseFinishingRankRange && !IsInRange(Data->FinishingRankRange, Finished->PlayerRank)) || (Data->bUsePlayersOutLived && !IsInRange(Data->PlayersOutLived, Finished->PlayersOutLived)))
            return 0;

        return DoesTagQueryMatch(Data->MatchMetaData.TagQuery, Finished->MatchMetaData.Tags) ? 1 : 0;
    }
    case EQuestVerb::PlayerReceivedGameplayEvent:
    {
        auto Data = (FFortObjectiveFilter_PlayerReceivedGameplayEvent*)Memory;
        auto Received = (FFortVerbMessage_PlayerReceivedGameplayEvent*)Message;
        auto& Filter = Data->GameplayEventFilter;
        auto& EventData = Received->EventData;

        if (Received->GameplayEventTag.TagName != Data->GameplayEventTag.TagName || (Data->bRequireAuthorInstigator && !Received->bAuthorIsInstigator) ||
            (Data->bRequireAuthorTarget && !Received->bAuthorIsTarget) || Filter.bUseAdvancedInstigatorClassFilter || Filter.bUseAdvancedTargetClassFilter || Filter.bUseAdvancedObject1ClassFilter ||
            Filter.bUseAdvancedObject2ClassFilter || (Filter.bUseEventMagnitude && !IsInRange(Filter.EventMagnitude, EventData.EventMagnitude)))
            return 0;

        if ((Filter.bUseInstigatorTags && !DoesTagQueryMatch(Filter.InstigatorTags, EventData.InstigatorTags)) || (Filter.bUseTargetTags && !DoesTagQueryMatch(Filter.TargetTags, EventData.TargetTags)) ||
            (Filter.bUseObject1Tags && !DoesTagQueryMatch(Filter.Object1TagFilter, EventData.OptionalObject1Tags)) ||
            (Filter.bUseObject2Tags && !DoesTagQueryMatch(Filter.Object2TagFilter, EventData.OptionalObject2Tags)))
            return 0;

        return Data->bIncrementByEventMagnitude ? AccumulateProgress(QuestManager, BackendName, EventData.EventMagnitude) : 1;
    }
    case EQuestVerb::PlayerDowned:
    {
        auto Data = (FFortObjectiveFilter_PlayerDowned*)Memory;
        auto Downed = (FFortVerbMessage_PlayerDowned*)Message;

        if ((Data->bIsHeadShot && !Downed->bIsHeadShot) || (Data->bIsSaviorDBNO && !Downed->bIsSaviorDBNO) || (Data->bIsAvengerDBNO && !Downed->bIsAvengerDBNO) ||
            (Data->bUseDownedDistance && !IsInRange(Data->DownedDistance, Downed->DownedDistance)))
            return 0;

        return DoesTagQueryMatch(Data->DownedPlayer.TagQuery, Downed->DownedPlayer.Tags) && DoesTagQueryMatch(Data->Downer.TagQuery, Downed->Downer.Tags) ? 1 : 0;
    }
    default:
        return 0;
    }
}

static int32 EvaluateObjectiveFilter(const FInstancedStruct& Filter, UFortQuestManager* QuestManager, FName BackendName, AFortPlayerControllerAthena* PlayerController, FFortQuestStatEvent* Event, UObject* TargetObject,
                                     const FVerbProcessor* Verb, FVerbMessage* Message)
{
    static auto KillContribution = FindFortniteGameStruct(L"FortObjectiveFilter_KillContribution");
    static auto Visit = FindFortniteGameStruct(L"FortObjectiveFilter_Visit");
    static auto DiscoverPOI = FindFortniteGameStruct(L"FortObjectiveFilter_DiscoverPOI");
    static auto Land = FindFortniteGameStruct(L"FortObjectiveFilter_Land");
    static auto LandFromBus = FindFortniteGameStruct(L"FortObjectiveFilter_LandFromBus");
    static auto Emote = FindFortniteGameStruct(L"FortObjectiveFilter_Emote");
    static auto Spray = FindFortniteGameStruct(L"FortObjectiveFilter_Spray");
    static auto Build = FindFortniteGameStruct(L"FortObjectiveFilter_Build");
    static auto AddToInventory = FindFortniteGameStruct(L"FortObjectiveFilter_AddToInventory");
    static auto Acquire = FindFortniteGameStruct(L"FortObjectiveFilter_Acquire");
    static auto Interact = FindFortniteGameStruct(L"FortObjectiveFilter_Interact");
    static auto RevivePlayer = FindFortniteGameStruct(L"FortObjectiveFilter_RevivePlayer");
    static auto RevivedFromDBNO = FindFortniteGameStruct(L"FortObjectiveFilter_RevivedFromDBNO");
    static auto StormPhaseChanged = FindFortniteGameStruct(L"FortObjectiveFilter_StormPhaseChanged");
    static auto PlacementUpdate = FindFortniteGameStruct(L"FortObjectiveFilter_PlacementUpdate");
    static auto MatchTimeElapsed = FindFortniteGameStruct(L"FortObjectiveFilter_MatchTimeElapsed");
    static auto DistanceTraveled = FindFortniteGameStruct(L"FortObjectiveFilter_DistanceTraveled");

    auto Struct = Filter.ScriptStruct;
    auto Memory = Filter.StructMemory;

    if (!Struct || !Memory)
        return 0;

    UScriptStruct* ExpectedStructs[2] = { Verb ? Verb->FilterStruct : nullptr };

    switch (Verb ? EFortQuestObjectiveStatEvent::EFortQuestObjectiveStatEvent_MAX : Event->Type)
    {
    case EFortQuestObjectiveStatEvent::KillContribution:
        ExpectedStructs[0] = KillContribution;
        break;
    case EFortQuestObjectiveStatEvent::Visit:
        ExpectedStructs[0] = Visit;
        break;
    case EFortQuestObjectiveStatEvent::VisitDiscoverPOI:
        ExpectedStructs[0] = DiscoverPOI;
        break;
    case EFortQuestObjectiveStatEvent::Land:
        ExpectedStructs[0] = Land;
        ExpectedStructs[1] = LandFromBus;
        break;
    case EFortQuestObjectiveStatEvent::Emote:
        ExpectedStructs[0] = Emote;
        break;
    case EFortQuestObjectiveStatEvent::Spray:
        ExpectedStructs[0] = Spray;
        break;
    case EFortQuestObjectiveStatEvent::Build:
        ExpectedStructs[0] = Build;
        break;
    case EFortQuestObjectiveStatEvent::Collect:
        ExpectedStructs[0] = AddToInventory;
        break;
    case EFortQuestObjectiveStatEvent::AthenaAcquire:
        ExpectedStructs[0] = Acquire;
        break;
    case EFortQuestObjectiveStatEvent::Interact:
        ExpectedStructs[0] = Interact;
        break;
    case EFortQuestObjectiveStatEvent::RevivePlayer:
        ExpectedStructs[0] = RevivePlayer;
        break;
    case EFortQuestObjectiveStatEvent::RevivedFromDBNO:
        ExpectedStructs[0] = RevivedFromDBNO;
        break;
    case EFortQuestObjectiveStatEvent::StormPhase:
        ExpectedStructs[0] = StormPhaseChanged;
        break;
    case EFortQuestObjectiveStatEvent::PlacementUpdate:
        ExpectedStructs[0] = PlacementUpdate;
        break;
    case EFortQuestObjectiveStatEvent::TimeElapsed:
        ExpectedStructs[0] = MatchTimeElapsed;
        break;
    case EFortQuestObjectiveStatEvent::DistanceTraveled:
    case EFortQuestObjectiveStatEvent::AthenaDistanceTraveled:
        ExpectedStructs[0] = DistanceTraveled;
        break;
    default:
        break;
    }

    if (!ExpectedStructs[0] || (Struct != ExpectedStructs[0] && Struct != ExpectedStructs[1]))
        return 0;

    auto ObjectiveFilter = (FObjectiveFilter*)Memory;

    if (!ObjectiveFilter->Author.bCanGainProgressForThisObjective)
        return 0;

    if (!DoesTagQueryMatch(ObjectiveFilter->Author.TagQuery, Event->SourceTags))
        return 0;

    if (!DoesTagQueryMatch(ObjectiveFilter->Context.TagQuery, Event->ContextTags))
        return 0;

    if (Verb)
        return EvaluateVerbFilter(Memory, QuestManager, BackendName, PlayerController, *Verb, Message);

    switch (Event->Type)
    {
    case EFortQuestObjectiveStatEvent::KillContribution:
    {
        auto Data = (FFortObjectiveFilter_KillContribution*)Memory;

        if (Data->bUseDamageContributed || Data->bUseDamageContributedPercent || Data->bUseDistanceFromEliminatedPawn)
            return 0;

        if (!IsAllowed(Data->AllowedTeamAffiliations, Data->ProhibitedTeamAffiliations, GetTeamAffiliation(PlayerController, TargetObject)))
            return 0;

        if (Data->FinalBlowGivesCredit == EFinalBlowAllowance::Never)
        {
            auto TargetPlayerState = GetTargetPlayerState(TargetObject);

            if (TargetPlayerState && TargetPlayerState->DeathInfo.FinisherOrDowner.Get() == PlayerController->PlayerState)
                return 0;
        }

        return DoesTagQueryMatch(Data->KilledActor.TagQuery, Event->TargetTags) && DoesTagQueryMatch(Data->KillerTags.TagQuery, Event->SourceTags) ? 1 : 0;
    }
    case EFortQuestObjectiveStatEvent::Visit:
        return DoesLocationMatch(((FFortObjectiveFilter_Visit*)Memory)->Location.TagQuery, Event) ? Event->Count : 0;
    case EFortQuestObjectiveStatEvent::VisitDiscoverPOI:
        return DoesLocationMatch(((FFortObjectiveFilter_DiscoverPOI*)Memory)->PoiTags.TagQuery, Event) ? Event->Count : 0;
    case EFortQuestObjectiveStatEvent::Land:
    {
        auto Data = (FFortObjectiveFilter_Land*)Memory;

        if (Data->NearbyPlayersFilter.bFilterEnabled || Data->AllowedSurfaces.Num() > 0)
            return 0;

        if (Struct == LandFromBus && !(TargetObject && TargetObject->IsA(AFortPlayerPawn::StaticClass()) && ((AFortPlayerPawn*)TargetObject)->bIsSkydivingFromBus))
            return 0;

        return 1;
    }
    case EFortQuestObjectiveStatEvent::Emote:
    {
        auto Data = (FFortObjectiveFilter_Emote*)Memory;

        if (Data->NearbyPlayersFilter.bFilterEnabled || Data->Target.TagQuery.QueryTokenStream.Num() > 0)
            return 0;

        return DoesTagQueryMatch(Data->Emote.TagQuery, Event->TargetTags) ? 1 : 0;
    }
    case EFortQuestObjectiveStatEvent::Spray:
    {
        auto Data = (FFortObjectiveFilter_Spray*)Memory;

        return Data->Target.TagQuery.QueryTokenStream.Num() == 0 && DoesTagQueryMatch(Data->Spray.TagQuery, Event->TargetTags) ? 1 : 0;
    }
    case EFortQuestObjectiveStatEvent::Build:
    {
        auto Data = (FFortObjectiveFilter_Build*)Memory;

        return !Data->bUseResourcesSpent && !Data->bIncrementByResourcesSpent && DoesTagQueryMatch(Data->Building.TagQuery, Event->TargetTags) ? 1 : 0;
    }
    case EFortQuestObjectiveStatEvent::Collect:
    {
        auto Data = (FFortObjectiveFilter_AddToInventory*)Memory;

        return DoesTagQueryMatch(Data->Item.TagQuery, Event->TargetTags) ? (Data->bIncrementByItemCount ? Event->Count : 1) : 0;
    }
    case EFortQuestObjectiveStatEvent::AthenaAcquire:
    {
        auto Data = (FFortObjectiveFilter_Acquire*)Memory;

        if ((Data->bUseItemCount && !IsInRange(Data->ItemCount, Event->Count)) || !DoesTagQueryMatch(Data->ItemAcquired.TagQuery, Event->TargetTags) ||
            !DoesTagQueryMatch(Data->AcquireSource.TagQuery, Event->SourceTags))
            return 0;

        switch (Data->IncrementType)
        {
        case EAcquireIncrementType::HighestMatchingItemCount:
        case EAcquireIncrementType::TotalMatchingItemCounts:
        case EAcquireIncrementType::TotalItemCount:
            return Event->Count;
        default:
            return 1;
        }
    }
    case EFortQuestObjectiveStatEvent::TimeElapsed:
    {
        auto Data = (FFortObjectiveFilter_MatchTimeElapsed*)Memory;
        auto UpdateTime = (int32)Data->UpdateTime;

        if (UpdateTime <= 0)
            return 0;

        auto& Elapsed = GetFlushState(QuestManager).ElapsedSeconds[BackendName.ComparisonIndex];
        Elapsed += Event->Count;

        auto Updates = Elapsed / UpdateTime;
        Elapsed -= Updates * UpdateTime;

        return Data->IncrementType == ETimeElapsedFilterIncrementType::TimeElapsed ? Updates * UpdateTime : Updates;
    }
    case EFortQuestObjectiveStatEvent::DistanceTraveled:
    case EFortQuestObjectiveStatEvent::AthenaDistanceTraveled:
    {
        auto Data = (FFortObjectiveFilter_DistanceTraveled*)Memory;
        auto Pawn = TargetObject && TargetObject->IsA(AFortPlayerPawn::StaticClass()) ? (AFortPlayerPawn*)TargetObject : nullptr;
        bool bHorizontal = Data->TravelDirection == EDistanceTravelDirection::Horizontal;

        if (!Pawn || (Event->Type == EFortQuestObjectiveStatEvent::AthenaDistanceTraveled) != bHorizontal || (!bHorizontal && Data->TravelDirection != EDistanceTravelDirection::Any) ||
            Data->bUsePerAxisSettings || Data->BoostRequirement != EObjectiveProcessorPropertyRequirement::Ignored || Data->AllowedVehicleSurfaces.Num() > 0)
            return 0;

        if (!IsTravelModeAllowed(Data->AllowedTravelModes, Data->ProhibitedTravelModes, GetTravelMode(Pawn)))
            return 0;

        auto Vehicle = Pawn->BP_GetVehicle();

        if (Data->bRequireVehicleInAir && (!Vehicle || !Vehicle->IsInAir()))
            return 0;

        if (Data->Vehicle.TagQuery.QueryTokenStream.Num() > 0)
        {
            FGameplayTagContainer VehicleTags{};
            GetSubjectTags(Vehicle, &VehicleTags);

            bool bMatched = Vehicle && DoesTagQueryMatch(Data->Vehicle.TagQuery, VehicleTags);
            FreeGameplayTags(VehicleTags);

            if (!bMatched)
                return 0;
        }

        return bHorizontal ? Event->Count : AccumulateProgress(QuestManager, BackendName, Event->Count / 100.0);
    }
    case EFortQuestObjectiveStatEvent::Interact:
        return DoesTagQueryMatch(((FFortObjectiveFilter_Interact*)Memory)->Target.TagQuery, Event->TargetTags) ? 1 : 0;
    case EFortQuestObjectiveStatEvent::RevivePlayer:
        return DoesTagQueryMatch(((FFortObjectiveFilter_RevivePlayer*)Memory)->RevivedPlayer.TagQuery, Event->TargetTags) ? 1 : 0;
    case EFortQuestObjectiveStatEvent::RevivedFromDBNO:
        return DoesTagQueryMatch(((FFortObjectiveFilter_RevivedFromDBNO*)Memory)->RevivingPlayer.TagQuery, Event->TargetTags) ? 1 : 0;
    case EFortQuestObjectiveStatEvent::StormPhase:
    {
        auto Data = (FFortObjectiveFilter_StormPhaseChanged*)Memory;

        if (Data->StormStepRequirement == EStormStepVerbRequirement::OnlyStormShrinking)
            return 0;

        if (Data->bFilterByStormPhase)
        {
            auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(UWorld::GetWorld());

            if (!GamePhaseLogic || !GamePhaseLogic->SafeZoneIndicator || !IsInRange(Data->StormPhase, GamePhaseLogic->SafeZoneIndicator->CurrentPhase))
                return 0;
        }

        return 1;
    }
    case EFortQuestObjectiveStatEvent::PlacementUpdate:
    {
        auto GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;

        if (!GameState)
            return 0;

        auto Data = (FFortObjectiveFilter_PlacementUpdate*)Memory;
        bool bAuthorSetPlacement = TargetObject == PlayerController;

        if ((Data->PlacementAuthorSettings == EPlacementAuthorHandling::RequireAuthorSetPlacement && !bAuthorSetPlacement) ||
            (Data->PlacementAuthorSettings == EPlacementAuthorHandling::ProhibitAuthorSetPlacement && bAuthorSetPlacement))
            return 0;

        if ((Data->bUseCurrentPlacement && !IsInRange(Data->CurrentPlacement, GameState->TeamsLeft)) || (Data->bUsePlayersAlive && !IsInRange(Data->PlayersAlive, GameState->PlayersLeft)))
            return 0;

        return DoesTagQueryMatch(Data->PlayerThatSetPlacement.TagQuery, Event->TargetTags) ? 1 : 0;
    }
    default:
        return 0;
    }
}

static void AddPendingChange(UFortQuestManager* QuestManager, UFortQuestItem* Quest, FName BackendName, int32 Count, std::vector<int32>& CountedNames)
{
    auto& PendingChanges = *(TArray<FFortQuestObjectiveCompletionEntry>*)&QuestManager->PendingChanges;
    auto RawName = BackendName.ToString();
    auto StatName = std::wstring(RawName.begin(), RawName.end());
    FFortQuestObjectiveCompletionEntry* Entry = nullptr;

    for (auto& Change : PendingChanges)
    {
        if (Change.StatName.Num() > 0 && _wcsicmp(Change.StatName.CStr(), StatName.c_str()) == 0)
        {
            Entry = &Change;
            break;
        }
    }

    if (!Entry)
    {
        FFortQuestObjectiveCompletionEntry NewEntry{};
        NewEntry.StatName = FString(StatName.c_str());
        Entry = &PendingChanges.Add(NewEntry);
    }

    if (std::find(CountedNames.begin(), CountedNames.end(), BackendName.ComparisonIndex) == CountedNames.end())
    {
        Entry->Count += Count;
        CountedNames.push_back(BackendName.ComparisonIndex);
    }

    for (auto& QuestPtr : Entry->Quests)
    {
        if (QuestPtr.Get() == Quest)
            return;
    }

    Entry->Quests.Add(FWeakObjectPtr(Quest));
}

static FGameplayTag GetProductTag(UObject* Context)
{
    static auto GetProductTagInternal = (FGameplayTag* (*)(FGameplayTag*, UObject*))(ImageBase + 0x9D70C74);

    FGameplayTag ProductTag{};
    GetProductTagInternal(&ProductTag, Context);

    return ProductTag;
}

static bool IsCompatibleWithProduct(UFortQuestItemDefinition* Definition, AFortPlayerControllerAthena* PlayerController, FGameplayTag& ProductTag)
{
    static auto IsCompatibleWithProductInternal = (bool (*)(UFortQuestItemDefinition*, UObject*, FGameplayTag*))(ImageBase + 0xA3396D8);

    if (ProductTag.TagName.IsNone() || Definition->Objectives.Num() == 0 || Definition->Objectives[0].ObjectiveFilters.Num() == 0)
        return true;

    return IsCompatibleWithProductInternal(Definition, PlayerController, &ProductTag);
}

static bool ProcessStatEventForQuests(UFortQuestManager* QuestManager, AFortPlayerControllerAthena* PlayerController, FFortQuestStatEvent* Event, UObject* TargetObject, const FVerbProcessor* Verb,
                                      FVerbMessage* Message)
{
    static auto HasCompletedQuest = (bool (*)(UFortQuestItem*))(ImageBase + 0xA337060);
    static auto IsObjectiveInStage = (bool (*)(FFortMcpQuestObjectiveInfo*, int32, UFortQuestItemDefinition*))(ImageBase + 0xA338C04);
    static auto DoesObjectiveStatHandleMatch = (bool (*)(FDataTableRowHandle*, FDataTableRowHandle*))(ImageBase + 0x21ECF80);
    static auto DoesObjectiveMatchStatEvent = (bool (*)(FFortMcpQuestObjectiveInfo*, FFortQuestStatEvent*, FString*))(ImageBase + 0xA32D6F0);
    static auto GetObjectiveCompletionCount = (int32 (*)(UFortQuestItem*, FName*))(ImageBase + 0xA331634);

    std::vector<int32> CountedNames;
    FString ContextString;
    bool bChanged = false;
    bool bHasObjectiveStat = Event->ObjectiveStat.DataTable || !Event->ObjectiveStat.RowName.IsNone();

    auto ProfileQuests = GetProfileQuests(QuestManager);
    auto ProductTag = GetProductTag(PlayerController);

    for (auto Quest : ProfileQuests)
    {
        if (!Quest || HasCompletedQuest(Quest))
            continue;

        auto Definition = (UFortQuestItemDefinition*)Quest->ItemDefinition;

        if (!Definition)
            continue;

        if (!IsCompatibleWithProduct(Definition, PlayerController, ProductTag))
            continue;

        for (auto& Objective : Definition->Objectives)
        {
            if (!IsObjectiveInStage(&Objective, Quest->CurrentStage, Definition))
                continue;

            UFortQuestObjectiveInfo* ObjectiveInfo = nullptr;

            for (auto Info : Quest->Objectives)
            {
                if (Info && Info->BackendName == Objective.BackendName)
                {
                    ObjectiveInfo = Info;
                    break;
                }
            }

            auto BackendName = Objective.BackendName;
            auto AchievedCount = ObjectiveInfo ? ObjectiveInfo->AchievedCount : GetObjectiveCompletionCount(Quest, &BackendName);
            auto RequiredCount = ObjectiveInfo ? ObjectiveInfo->RequiredCount : Objective.Count;

            if (AchievedCount >= RequiredCount)
                continue;

            int32 Increment = 0;

            if (Objective.ObjectiveFilters.Num() > 0)
            {
                for (auto& Filter : Objective.ObjectiveFilters)
                {
                    if ((Increment = EvaluateObjectiveFilter(Filter, QuestManager, Objective.BackendName, PlayerController, Event, TargetObject, Verb, Message)) > 0)
                        break;
                }
            }
            else if (!Verb)
            {
                bool bMatched = bHasObjectiveStat && Objective.InlineObjectiveStats.Num() <= 0 ? DoesObjectiveStatHandleMatch(&Objective.ObjectiveStatHandle, &Event->ObjectiveStat)
                                                                                              : DoesObjectiveMatchStatEvent(&Objective, Event, &ContextString);
                Increment = bMatched ? Event->Count : 0;
            }

            if (Increment <= 0)
                continue;

            AddPendingChange(QuestManager, Quest, Objective.BackendName, Increment, CountedNames);
            bChanged = true;
        }
    }

    ProfileQuests.Free();

    return bChanged;
}

static void ProcessStatEvent(UFortQuestManager* QuestManager, AFortPlayerControllerAthena* PlayerController, FFortQuestStatEvent* Event, UObject* TargetObject, const FVerbProcessor* Verb,
                             FVerbMessage* Message)
{
    if (ProcessStatEventForQuests(QuestManager, PlayerController, Event, TargetObject, Verb, Message))
    {
        ReapplyPendingChanges(QuestManager);
        ScheduleFlush(QuestManager);
    }

    ProcessAccoladeEvent(PlayerController, &Event->ContextTags, [&](const FInstancedStruct& Filter, FName BackendName)
                         { return EvaluateObjectiveFilter(Filter, QuestManager, BackendName, PlayerController, Event, TargetObject, Verb, Message); });
}

static void InitVerbMessage(FVerbMessage* Message, AFortPlayerControllerAthena* PlayerController)
{
    static auto GetVerbContextTags = (void (*)(UObject*, FGameplayTagContainer*))(ImageBase + 0x30DB518);
    static auto SetVerbAuthor = (void (*)(FVerbMessage*, UObject*))(ImageBase + 0x30DB374);

    GetVerbContextTags(PlayerController, &Message->ContextTags);
    SetVerbAuthor(Message, PlayerController);
}

static void FreeVerbMessage(FVerbMessage* Message)
{
    FreeGameplayTags(Message->ContextTags);
    FreeGameplayTags(Message->Author.Tags);
}

static void ProcessVerbMessage(AFortPlayerControllerAthena* PlayerController, const FVerbProcessor& Verb, FVerbMessage* Message, UObject* TargetObject)
{
    static auto ConstructStatEvent = (FFortQuestStatEvent* (*)(FFortQuestStatEvent*, EFortQuestObjectiveStatEvent, UObject*, FGameplayTagContainer*, FGameplayTagContainer*, FGameplayTagContainer*,
                                                               FDataTableRowHandle*, FName, int32, FGuid*))(ImageBase + 0xA3B65C0);
    static auto DestroyStatEvent = (void (*)(FFortQuestStatEvent*))(ImageBase + 0x925181C);

    auto QuestManager = PlayerController->GetQuestManager(ESubGame::Athena);

    if (!QuestManager)
        return;

    LinkQuestManagerController(QuestManager, PlayerController);

    FGameplayTagContainer TargetTags{};
    FGameplayTagContainer ContextTags{};
    ContextTags.AppendTags(Message->ContextTags);

    if (auto Playlist = GetCurrentPlaylist())
        ContextTags.AppendTags(Playlist->GameplayTagContainer);

    FDataTableRowHandle EmptyObjectiveStat{};
    FGuid EventId{};
    alignas(16) uint8 EventStorage[sizeof(FFortQuestStatEvent)];

    auto Event = ConstructStatEvent((FFortQuestStatEvent*)EventStorage, Verb.Type, TargetObject, &TargetTags, &Message->Author.Tags, &ContextTags, &EmptyObjectiveStat, FName(), 1, &EventId);

    ProcessStatEvent(QuestManager, PlayerController, Event, TargetObject, &Verb, Message);

    DestroyStatEvent(Event);
    FreeGameplayTags(ContextTags);
}

static void FillDamageSubjects(FPendingDamageTaken& Pending, AFortPawn* Pawn, FSubjectTagsPair& DamageSource, FSubjectTagsPair& DamagedActor, FSubjectTagsPair& DamageType)
{
    static_cast<FWeakObjectPtr&>(DamageSource.Subject) = Pending.DamageSource;
    DamageSource.Tags.AppendTags(Pending.DamageSourceTags);
    SetSubject(&DamagedActor, Pawn);
    DamageType.Tags.AppendTags(Pending.DamageTypeTags);
}

static void FreeDamageSubjects(FSubjectTagsPair& DamageSource, FSubjectTagsPair& DamagedActor, FSubjectTagsPair& DamageType)
{
    FreeGameplayTags(DamageSource.Tags);
    FreeGameplayTags(DamagedActor.Tags);
    FreeGameplayTags(DamageType.Tags);
}

void SendDamageVerbs(AFortPlayerControllerAthena* PlayerController, AFortPawn* Pawn, FPendingDamageTaken& Pending, float Damage, bool bCriticalHit)
{
    bool bSelf = Pawn == PlayerController->Pawn;
    bool bTeamMember = !bSelf && GetTeamAffiliation(PlayerController, Pawn) == EFortTeamAffiliation::Friendly;
    auto DamageZone = bCriticalHit ? EFortDamageZone::Critical : EFortDamageZone::Normal;

    FFortVerbMessage_DamageDone DamageDone{};
    InitVerbMessage(&DamageDone, PlayerController);
    FillDamageSubjects(Pending, Pawn, DamageDone.DamageSource, DamageDone.DamagedActor, DamageDone.DamageType);
    DamageDone.Damage = Pending.Damage;
    DamageDone.UncappedDamage = (std::max)((int32)Damage, Pending.Damage);
    DamageDone.Distance = Pending.Distance;
    DamageDone.DamageZone = DamageZone;
    DamageDone.bDamageDoneToTeamMember = bTeamMember;
    DamageDone.bDamageDoneToSelf = bSelf;

    ProcessVerbMessage(PlayerController, GetVerbProcessor(EQuestVerb::DamageDone), &DamageDone, Pawn);

    FreeDamageSubjects(DamageDone.DamageSource, DamageDone.DamagedActor, DamageDone.DamageType);
    FreeVerbMessage(&DamageDone);

    FFortVerbMessage_Hit Hit{};
    InitVerbMessage(&Hit, PlayerController);
    FillDamageSubjects(Pending, Pawn, Hit.DamageSource, Hit.DamagedActor, Hit.DamageType);
    Hit.Distance = Pending.Distance;
    Hit.DamageZone = DamageZone;
    Hit.bHitTeamMember = bTeamMember;
    Hit.bHitSelf = bSelf;

    ProcessVerbMessage(PlayerController, GetVerbProcessor(EQuestVerb::Hit), &Hit, Pawn);

    FreeDamageSubjects(Hit.DamageSource, Hit.DamagedActor, Hit.DamageType);
    FreeVerbMessage(&Hit);
}

static void OnVerbMessage(UScriptStruct* Struct, FVerbMessage* Message)
{
    static auto DamageTaken = FindFortniteGameStruct(L"FortVerbMessage_DamageTaken");

    if (Struct == DamageTaken)
    {
        auto Taken = (FFortVerbMessage_DamageTaken*)Message;

        FreeGameplayTags(PendingDamageTaken.DamageSourceTags);
        FreeGameplayTags(PendingDamageTaken.DamageTypeTags);
        PendingDamageTaken.Victim = FWeakObjectPtr();

        if (!ResolvePlayerController(Taken->DamageSource.Subject.Get()))
            return;

        PendingDamageTaken.Victim = Message->Author.Subject;
        PendingDamageTaken.DamageSource = Taken->DamageSource.Subject;
        PendingDamageTaken.DamageSourceTags.AppendTags(Taken->DamageSource.Tags);
        PendingDamageTaken.DamageTypeTags.AppendTags(Taken->DamageType.Tags);
        PendingDamageTaken.Damage = Taken->Damage;
        PendingDamageTaken.Distance = Taken->Distance;
        return;
    }

    for (auto& Verb : GetVerbProcessors())
    {
        if (!Verb.bBroadcastByGame || Verb.MessageStruct != Struct)
            continue;

        if (auto PlayerController = ResolvePlayerController(Message->Author.Subject.Get()))
            ProcessVerbMessage(PlayerController, Verb, Message, nullptr);

        return;
    }
}

static __int64 (*BroadcastVerbMessageOG)(UObject*, FVerbMessageView*);
static __int64 BroadcastVerbMessage(UObject* Author, FVerbMessageView* View)
{
    auto Result = BroadcastVerbMessageOG(Author, View);

    if (View && View->ScriptStruct && View->Message)
        OnVerbMessage(View->ScriptStruct, View->Message);

    return Result;
}

bool HasGameplayEventObjective(UFortQuestManager* QuestManager, FGameplayTag EventTag)
{
    auto& State = GetFlushState(QuestManager);

    if (!State.bGameplayEventTagsCached)
    {
        State.bGameplayEventTagsCached = true;

        auto FilterStruct = GetVerbProcessor(EQuestVerb::PlayerReceivedGameplayEvent).FilterStruct;
        auto ProfileQuests = GetProfileQuests(QuestManager);

        for (auto Quest : ProfileQuests)
        {
            auto Definition = Quest ? (UFortQuestItemDefinition*)Quest->ItemDefinition : nullptr;

            if (!Definition)
                continue;

            for (auto& Objective : Definition->Objectives)
            {
                for (auto& Filter : Objective.ObjectiveFilters)
                {
                    if (Filter.ScriptStruct == FilterStruct && Filter.StructMemory)
                        State.GameplayEventTags.push_back(((FFortObjectiveFilter_PlayerReceivedGameplayEvent*)Filter.StructMemory)->GameplayEventTag.TagName.ComparisonIndex);
                }
            }
        }

        ProfileQuests.Free();
    }

    return std::find(State.GameplayEventTags.begin(), State.GameplayEventTags.end(), EventTag.TagName.ComparisonIndex) != State.GameplayEventTags.end();
}

void SendPlayerReceivedGameplayEvent(AFortPlayerControllerAthena* PlayerController, FGameplayTag EventTag, const FGameplayEventData& Payload)
{
    auto Pawn = PlayerController->Pawn;

    FFortVerbMessage_PlayerReceivedGameplayEvent Received{};
    InitVerbMessage(&Received, PlayerController);

    Received.GameplayEventTag = EventTag;
    Received.bAuthorIsInstigator = Payload.Instigator && (Payload.Instigator == Pawn || Payload.Instigator == PlayerController);
    Received.bAuthorIsTarget = Payload.Target && (Payload.Target == Pawn || Payload.Target == PlayerController);

    auto& EventData = Received.EventData;
    EventData.EventTag = Payload.EventTag;
    EventData.Instigator = TWeakObjectPtr<AActor>(Payload.Instigator);
    EventData.Target = TWeakObjectPtr<AActor>(Payload.Target);
    EventData.OptionalObject = TWeakObjectPtr<UObject>(Payload.OptionalObject);
    EventData.OptionalObject2 = TWeakObjectPtr<UObject>(Payload.OptionalObject2);
    EventData.InstigatorTags.AppendTags(Payload.InstigatorTags);
    EventData.TargetTags.AppendTags(Payload.TargetTags);
    GetSubjectTags(Payload.OptionalObject, &EventData.OptionalObject1Tags);
    GetSubjectTags(Payload.OptionalObject2, &EventData.OptionalObject2Tags);
    EventData.EventMagnitude = Payload.EventMagnitude;

    ProcessVerbMessage(PlayerController, GetVerbProcessor(EQuestVerb::PlayerReceivedGameplayEvent), &Received, Payload.Target);

    FreeGameplayTags(EventData.InstigatorTags);
    FreeGameplayTags(EventData.TargetTags);
    FreeGameplayTags(EventData.OptionalObject1Tags);
    FreeGameplayTags(EventData.OptionalObject2Tags);
    FreeVerbMessage(&Received);
}

static void SendMatchFinished(AFortPlayerControllerAthena* PlayerController)
{
    auto GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
    auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;

    FFortVerbMessage_MatchFinished Finished{};
    InitVerbMessage(&Finished, PlayerController);

    Finished.MatchMetaData.Tags.AppendTags(Finished.ContextTags);

    if (auto Playlist = GetCurrentPlaylist())
        Finished.MatchMetaData.Tags.AppendTags(Playlist->GameplayTagContainer);

    Finished.PlayerRank = PlayerState ? PlayerState->Place : 0;
    Finished.PlayersOutLived = GameState && PlayerState ? (std::max)(GameState->TotalPlayers - PlayerState->Place, 0) : 0;

    ProcessVerbMessage(PlayerController, GetVerbProcessor(EQuestVerb::MatchFinished), &Finished, PlayerController);

    FreeGameplayTags(Finished.MatchMetaData.Tags);
    FreeVerbMessage(&Finished);
}

static void CallMcpOperation(UFortQuestManager* QuestManager, UFortMcpProfile* Profile, UFunction* Function, void* Parms, FDedicatedServerUrlContext* Context, void (*OnComplete)(UFortQuestManager*, uint8*))
{
    static auto AllocateDelegateInstance = (uint8* (*)(int32, FNativeDelegate*))(ImageBase + 0x27E8A98);
    static auto ConstructQuestManagerDelegateInstance = (void (*)(uint8*, UFortQuestManager*))(ImageBase + 0xA3410AC);
    static auto DestroyDelegate = (void (*)(FNativeDelegate*))(ImageBase + 0x16E0B28);
    static auto ConstructDedicatedServerUrlContext = (void (*)(FDedicatedServerUrlContext*, FNativeDelegate*))(ImageBase + 0x1AC3E9C);
    static auto DestroyUrlContext = (void (*)(FDedicatedServerUrlContext*))(ImageBase + 0x4B78DC0);

    FNativeDelegate OnCompleteDelegate{};

    if (auto Instance = AllocateDelegateInstance(40, &OnCompleteDelegate))
    {
        ConstructQuestManagerDelegateInstance(Instance, QuestManager);
        *(void**)(Instance + 32) = (void*)OnComplete;
    }

    ConstructDedicatedServerUrlContext(Context, &OnCompleteDelegate);
    DestroyDelegate(&OnCompleteDelegate);

    auto Flags = Function->FunctionFlags;
    Function->FunctionFlags |= 0x400;
    Profile->ProcessEvent(Function, Parms);
    Function->FunctionFlags = Flags;

    DestroyUrlContext(Context);
}

static void HandleQuestUpdate(UFortQuestManager* QuestManager, uint8* Result, int32 SaveId)
{
    static auto HandleQuestUpdateInternal = (void (*)(UFortQuestManager*, uint8*, int32))(ImageBase + 0xA34FE2C);

    HandleQuestUpdateInternal(QuestManager, Result, SaveId);
}

static void OnUpdateQuestsComplete(UFortQuestManager* QuestManager, uint8* Result)
{
    auto It = FlushStates.find(QuestManager);

    if (It == FlushStates.end())
        return;

    auto& State = It->second;

    HandleQuestUpdate(QuestManager, Result, State.UpdateSaveId);
    State.UpdateSaveId = -1;
    State.bUpdateInFlight = false;

    ReapplyPendingChanges(QuestManager);

    if (Result[0] && QuestManager->PendingChanges.Num() > 0)
        ScheduleFlush(QuestManager);
}

static void OnEndBattleRoyaleGameComplete(UFortQuestManager* QuestManager, uint8* Result)
{
    auto It = FlushStates.find(QuestManager);

    if (It == FlushStates.end())
        return;

    auto& State = It->second;

    HandleQuestUpdate(QuestManager, Result, State.EndSaveId);
    State.EndSaveId = -1;
    State.bEndInFlight = false;

    if (QuestManager->PendingChanges.Num() > 0)
        ScheduleFlush(QuestManager);

    auto PlayerController = GetPlayerController(QuestManager);
    auto Profile = GetBoundProfile(QuestManager);

    if (Result[0] && PlayerController && PlayerController->XPComponent && Profile)
        PlayerController->XPComponent->HasBRMatchReportCompletedProfileVer = Profile->ProfileRevision;
}

static void SendUpdateQuests(UFortQuestManager* QuestManager, FQuestManagerFlushState& State)
{
    static auto ProcessPendingStatEvents = (FFortQuestSaveRequest* (*)(UFortQuestManager*))(ImageBase + 0xA353748);
    static auto UpdateQuests = UFortMcpProfileSubgame::StaticClass()->FindFunction("UpdateQuests");

    auto Profile = GetBoundProfile(QuestManager);

    if (!Profile || !UpdateQuests)
        return;

    auto SaveRequest = ProcessPendingStatEvents(QuestManager);

    if (!SaveRequest)
        return;

    Params::FortMcpProfileSubgame_UpdateQuests Parms{};
    Parms.Advance = SaveRequest->Changes;

    State.UpdateSaveId = SaveRequest->SaveId;
    State.bUpdateInFlight = true;

    CallMcpOperation(QuestManager, Profile, UpdateQuests, &Parms, &Parms.Context, OnUpdateQuestsComplete);
}

static std::wstring CurrentMatchId;

static std::wstring GetMatchId()
{
    if (!CurrentMatchId.empty())
        return CurrentMatchId;

    auto GameState = (AFortGameState*)UWorld::GetWorld()->GameState;

    if (GameState && GameState->GameSessionId.Num() > 1)
        CurrentMatchId = GameState->GameSessionId.ToWString();
    else
    {
        auto GuidString = UKismetGuidLibrary::Conv_GuidToString(UKismetGuidLibrary::NewGuid());
        CurrentMatchId = GuidString.ToWString();
        GuidString.Free();
    }

    return CurrentMatchId;
}

static void OnAthenaRewardResult(AFortPlayerControllerAthena* PlayerController, const FAthenaRewardResult& Result)
{
    PlayerController->ClientSendEndBattleRoyaleMatchForPlayer(true, Result);
}

static void SendEndBattleRoyaleGame(UFortQuestManager* QuestManager, FQuestManagerFlushState& State)
{
    static auto ProcessPendingStatEvents = (FFortQuestSaveRequest* (*)(UFortQuestManager*))(ImageBase + 0xA353748);
    static auto EndBattleRoyaleGameV2 = UFortMcpProfileAthena::StaticClass()->FindFunction("EndBattleRoyaleGameV2");
    static auto AddUObject = (void (*)(void*, uint64*, UObject*, void (*)(AFortPlayerControllerAthena*, const FAthenaRewardResult&)))(ImageBase + 0x7FBBC00);

    auto PlayerController = GetPlayerController(QuestManager);
    auto Profile = GetBoundProfile(QuestManager);

    State.bEnded = true;

    if (!PlayerController || !Profile || !EndBattleRoyaleGameV2)
        return;

    State.NextFlushTime = -1.0;
    State.bEndInFlight = true;

    auto SaveRequest = ProcessPendingStatEvents(QuestManager);
    auto Playlist = GetCurrentPlaylist();
    auto RawPlaylistName = Playlist ? Playlist->PlaylistName.ToString() : std::string();
    auto PlaylistName = std::wstring(RawPlaylistName.begin(), RawPlaylistName.end());
    auto MatchId = GetMatchId();

    Params::FortMcpProfileAthena_EndBattleRoyaleGameV2 Parms{};

    if (SaveRequest)
    {
        Parms.Advance = SaveRequest->Changes;
        State.EndSaveId = SaveRequest->SaveId;
    }

    Parms.PlaylistId = FString(PlaylistName.c_str());
    Parms.MatchStats.MatchID = FString(MatchId.c_str());
    Parms.TotalXPAccum = PlayerController->XPComponent ? PlayerController->XPComponent->TotalXpEarned : 0;

    if (auto VictoryCrowns = (UFortControllerComponent_VictoryCrowns*)PlayerController->GetComponentByClass(UFortControllerComponent_VictoryCrowns::StaticClass()))
        Parms.VictoryCrownData = VictoryCrowns->VictoryCrownAccountData;

    uint64 RewardResultHandle = 0;
    AddUObject((uint8*)Profile + 0x4E8, &RewardResultHandle, PlayerController, OnAthenaRewardResult);

    CallMcpOperation(QuestManager, Profile, EndBattleRoyaleGameV2, &Parms, &Parms.Context, OnEndBattleRoyaleGameComplete);

    Parms.PlaylistId.Free();
    Parms.MatchStats.MatchID.Free();
}

void SendRequestedMatchReports(double Now)
{
    std::vector<UFortQuestManager*> QuestManagers;

    for (auto& [QuestManager, State] : FlushStates)
    {
        if (State.bEndRequested && !State.bEnded && State.QuestManager.IsValid() && (Now < 0.0 || Now > State.EndRequestTime))
            QuestManagers.push_back(QuestManager);
    }

    for (auto QuestManager : QuestManagers)
        SendEndBattleRoyaleGame(QuestManager, FlushStates[QuestManager]);
}

void EndBattleRoyaleGame(AFortPlayerControllerAthena* PlayerController)
{
    RecordMatchResult(PlayerController);

    static auto EndBattleRoyaleGameV2 = UFortMcpProfileAthena::StaticClass()->FindFunction("EndBattleRoyaleGameV2");

    auto QuestManager = PlayerController ? PlayerController->GetQuestManager(ESubGame::Athena) : nullptr;

    if (!QuestManager || GetFlushState(QuestManager).bEndRequested)
        return;

    auto Profile = GetBoundProfile(QuestManager);

    if (!Profile || !Profile->IsA(UFortMcpProfileAthena::StaticClass()) || !EndBattleRoyaleGameV2)
        return;

    LinkQuestManagerController(QuestManager, PlayerController);
    GrantVictoryCrownAwards(PlayerController);
    SendMatchFinished(PlayerController);

    if (auto AccoladeCollection = (UFortControllerComponent_AccoladeCollection*)PlayerController->GetComponentByClass(UFortControllerComponent_AccoladeCollection::StaticClass()))
        AccoladeCollection->OnEndMatchForPlayer(UFortControllerComponent_EndMatchPersistence::GetFromPlayerController(PlayerController, false));

    auto& State = GetFlushState(QuestManager);
    State.bEndRequested = true;
    State.EndRequestTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
}

void SendPlacementUpdate(AFortPlayerControllerAthena* Eliminator)
{
    auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;

    for (auto PlayerController : GameMode->AlivePlayers)
    {
        if (PlayerController)
            SendStatEvent(PlayerController->GetQuestManager(ESubGame::Athena), PlayerController, EFortQuestObjectiveStatEvent::PlacementUpdate, 1, true, PlayerController == Eliminator ? PlayerController : nullptr);
    }
}

static void BindScriptDelegate(TArray<FScriptDelegate>& InvocationList, UObject* Object, FName FunctionName)
{
    for (auto& Delegate : InvocationList)
    {
        if (Delegate.Object.Get() == Object && Delegate.FunctionName == FunctionName)
            return;
    }

    FScriptDelegate Delegate{};
    Delegate.Object = FWeakObjectPtr(Object);
    Delegate.FunctionName = FunctionName;
    InvocationList.Add(Delegate);
}

void RegisterWithQuestManager(AFortPlayerControllerAthena* PlayerController)
{
    static auto OnXPEvent = FName(L"OnXPEvent");
    static auto OnXpUpdated = FName(L"OnXpUpdated");

    auto XPComponent = PlayerController->XPComponent;
    auto QuestManager = PlayerController->GetQuestManager(ESubGame::Athena);

    if (!XPComponent)
        return;

    LinkQuestManagerController(QuestManager, PlayerController);

    for (int32 i = 0; QuestManager && i < QuestManager->Components.Num(); i++)
    {
        auto Component = QuestManager->Components[i];

        if (Component && Component->IsA(UFortQuestManagerComponent_Athena::StaticClass()))
        {
            BindScriptDelegate(*(TArray<FScriptDelegate>*)(__int64(Component) + 0x3E8), XPComponent, OnXPEvent);
            BindScriptDelegate(*(TArray<FScriptDelegate>*)(__int64(Component) + 0x3F8), XPComponent, OnXpUpdated);
            break;
        }
    }

    XPComponent->bRegisteredWithQuestManager = true;
    XPComponent->OnRep_bRegisteredWithQuestManager();
}

void SendStatEvent(UFortQuestManager* QuestManager, AActor* PlayerController, EFortQuestObjectiveStatEvent StatEvent, int32 Count, bool bAllowQueueStatEvent, UObject* TargetObject, FGameplayTagContainer TargetTags,
                   FGameplayTagContainer AdditionalSourceTags)
{
    static auto SendStatEventWithTags = (void (*)(UFortQuestManager*, EFortQuestObjectiveStatEvent, UObject*, FGameplayTagContainer*, FGameplayTagContainer*, FGameplayTagContainer*, int32))(ImageBase + 0xA356B68);

    if (!bAllowQueueStatEvent || !QuestManager)
        return;

    if (PlayerController && PlayerController->IsA(AFortPlayerControllerAthena::StaticClass()))
        LinkQuestManagerController(QuestManager, (AFortPlayerControllerAthena*)PlayerController);

    FGameplayTagContainer SourceTags{};
    FGameplayTagContainer ContextTags{};

    QuestManager->GetSourceAndContextTags(&SourceTags, &ContextTags);
    SourceTags.AppendTags(AdditionalSourceTags);

    SendStatEventWithTags(QuestManager, StatEvent, TargetObject, &TargetTags, &SourceTags, &ContextTags, Count);

    FreeGameplayTags(SourceTags);
    FreeGameplayTags(ContextTags);
}

static void QueueStatEvent(UFortQuestManager* QuestManager, EFortQuestObjectiveStatEvent InType, UObject* InTargetObject, FGameplayTagContainer* InTargetTags, FGameplayTagContainer* InSourceTags,
                           FGameplayTagContainer* InContextTags, FDataTableRowHandle* InObjectiveStat, FName InObjectiveBackendName, int32 InCount)
{
    static auto ConstructStatEvent = (FFortQuestStatEvent* (*)(FFortQuestStatEvent*, EFortQuestObjectiveStatEvent, UObject*, FGameplayTagContainer*, FGameplayTagContainer*, FGameplayTagContainer*,
                                                               FDataTableRowHandle*, FName, int32, FGuid*))(ImageBase + 0xA3B65C0);
    static auto DestroyStatEvent = (void (*)(FFortQuestStatEvent*))(ImageBase + 0x925181C);
    static auto BroadcastStatEvent = (void (*)(void*, FFortQuestStatEvent*, void*))(ImageBase + 0x5B6C24C);

    auto PlayerController = GetPlayerController(QuestManager);

    if (!PlayerController)
        return;

    if (auto Playlist = GetCurrentPlaylist())
        InContextTags->AppendTags(Playlist->GameplayTagContainer);

    FDataTableRowHandle EmptyObjectiveStat{};
    FGuid EventId{};
    alignas(16) uint8 EventStorage[sizeof(FFortQuestStatEvent)];

    auto Event = ConstructStatEvent((FFortQuestStatEvent*)EventStorage, InType, InTargetObject, InTargetTags, InSourceTags, InContextTags, InObjectiveStat ? InObjectiveStat : &EmptyObjectiveStat,
                                    InObjectiveBackendName, InCount, &EventId);

    BroadcastStatEvent((void*)(__int64(QuestManager) + 0x140), Event, nullptr);

    ProcessStatEvent(QuestManager, PlayerController, Event, InTargetObject, nullptr, nullptr);

    DestroyStatEvent(Event);
}

static double TimeElapsedCountedUntil = -1.0;

static void SendTimeElapsed(double Now)
{
    if (TimeElapsedCountedUntil < 0.0 || Now < TimeElapsedCountedUntil)
    {
        TimeElapsedCountedUntil = Now;
        return;
    }

    auto Seconds = (int32)(Now - TimeElapsedCountedUntil);

    if (Seconds <= 0)
        return;

    TimeElapsedCountedUntil += Seconds;

    auto World = UWorld::GetWorld();
    auto GameMode = (AFortGameModeAthena*)World->AuthorityGameMode;
    auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(World);

    if (!GameMode || !GameMode->IsA(AFortGameModeAthena::StaticClass()) || !GamePhaseLogic || GamePhaseLogic->GamePhase < EAthenaGamePhase::Aircraft ||
        GamePhaseLogic->GamePhase >= EAthenaGamePhase::EndGame)
        return;

    for (auto PlayerController : GameMode->AlivePlayers)
    {
        if (PlayerController)
            SendStatEvent(PlayerController->GetQuestManager(ESubGame::Athena), PlayerController, EFortQuestObjectiveStatEvent::TimeElapsed, Seconds, true);
    }
}

void UFortQuestManager__PostLoad()
{
    Hook(ImageBase + 0xA353AB8, QueueStatEvent);

    UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"fort.DisablePartialQS1InNonCampaign 0", nullptr);

    if (auto DisablePartialQS1InNonCampaign = *(uint8**)(ImageBase + 0x12F01AA8))
        *DisablePartialQS1InNonCampaign = 0;

    Hook(ImageBase + 0xA60BCB8, BroadcastVerbMessage, BroadcastVerbMessageOG);
}

void UFortQuestManager__Tick()
{
    auto World = UWorld::GetWorld();

    if (!World)
        return;

    auto Now = UGameplayStatics::GetTimeSeconds(World);

    SendTimeElapsed(Now);
    SendRequestedMatchReports(Now);

    for (auto It = FlushStates.begin(); It != FlushStates.end();)
    {
        auto& State = It->second;

        if (!State.QuestManager.IsValid())
        {
            It = FlushStates.erase(It);
            continue;
        }

        if ((State.bEnded || !State.bEndRequested) && !State.bEndInFlight && !State.bUpdateInFlight && State.NextFlushTime >= 0.0 && Now >= State.NextFlushTime)
        {
            State.NextFlushTime = -1.0;
            SendUpdateQuests(It->first, State);
        }

        ++It;
    }
}
