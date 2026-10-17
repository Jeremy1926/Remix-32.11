#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Components/FortControllerComponent_IndicatedActorManagement.h"

void FreeData(FIndicatedActorData& Data)
{
    Data.GroupIdentifier.Free();
    FreeGameplayTags(Data.EventTags);
}

void FreeData(FStenciledActorData& Data)
{
    Data.GroupIdentifier.Free();
}

static void CopyFrom(FIndicatedActorData& Data, const FIndicatedActorData& Other)
{
    memcpy((PBYTE)&Data, (const PBYTE)&Other, sizeof(FIndicatedActorData));

    Data.GroupIdentifier = Other.GroupIdentifier.Num() > 0 && Other.GroupIdentifier.Data ? FString(Other.GroupIdentifier.Data) : FString();
    Data.EventTags.GameplayTags = TArray<FGameplayTag>();
    Data.EventTags.ParentTags = TArray<FGameplayTag>();

    for (auto& Tag : Other.EventTags.GameplayTags)
        Data.EventTags.GameplayTags.Add(Tag);

    for (auto& Tag : Other.EventTags.ParentTags)
        Data.EventTags.ParentTags.Add(Tag);
}

static void CopyFrom(FStenciledActorData& Data, const FStenciledActorData& Other)
{
    memcpy((PBYTE)&Data, (const PBYTE)&Other, sizeof(FStenciledActorData));

    Data.GroupIdentifier = Other.GroupIdentifier.Num() > 0 && Other.GroupIdentifier.Data ? FString(Other.GroupIdentifier.Data) : FString();
}

UFortControllerComponent_IndicatedActorManagement* GetIndicatedActorManagement(AActor* Controller)
{
    return (UFortControllerComponent_IndicatedActorManagement*)Controller->GetComponentByClass(UFortControllerComponent_IndicatedActorManagement::StaticClass());
}

static AFortPlayerControllerAthena* GetOwningPlayerController(UFortControllerComponent_IndicatedActorManagement* Component)
{
    return Component->GetOwner()->IsA(AFortPlayerControllerAthena::StaticClass()) ? (AFortPlayerControllerAthena*)Component->GetOwner() : nullptr;
}

static AFortPlayerStateAthena* GetOwningPlayerState(UFortControllerComponent_IndicatedActorManagement* Component)
{
    auto PlayerController = GetOwningPlayerController(Component);

    return PlayerController ? (AFortPlayerStateAthena*)PlayerController->PlayerState : nullptr;
}

static void GetMarkableActors(UFortControllerComponent_IndicatedActorManagement* Component, TArray<AActor*>& Actors, TArray<AActor*>& OutActors)
{
    static auto CannotBeMarkedByAnyTag = FGameplayTag{ FName(L"Athena.Gameplay.Status.CannotBeMarkedBy.Any") };
    static auto CannotBeMarkedByEnemyOnlyTag = FGameplayTag{ FName(L"Athena.Gameplay.Status.CannotBeMarkedBy.EnemyOnly") };

    auto PlayerState = GetOwningPlayerState(Component);

    for (auto Actor : Actors)
    {
        if (!Actor)
            continue;

        FGameplayTagContainer OwnedTags{};
        GetOwnedGameplayTags(Actor, OwnedTags);

        auto Pawn = Actor->IsA(AFortPlayerPawnAthena::StaticClass()) ? (AFortPlayerPawnAthena*)Actor : nullptr;
        auto ActorPlayerState = Pawn && Pawn->PlayerState && Pawn->PlayerState->IsA(AFortPlayerStateAthena::StaticClass()) ? (AFortPlayerStateAthena*)Pawn->PlayerState : nullptr;
        auto bIsEnemy = !PlayerState || !ActorPlayerState || ActorPlayerState->TeamIndex != PlayerState->TeamIndex;

        if (!OwnedTags.HasTag(CannotBeMarkedByAnyTag) && (!bIsEnemy || !OwnedTags.HasTag(CannotBeMarkedByEnemyOnlyTag)))
            OutActors.Add(Actor);

        FreeGameplayTags(OwnedTags);
    }
}

static void GetSharedComponents(UFortControllerComponent_IndicatedActorManagement* Component, EShareActorWith ShareActorWith, uint8 ShareActorWithMask, TArray<UFortControllerComponent_IndicatedActorManagement*>& OutComponents)
{
    OutComponents.Add(Component);

    auto PlayerController = GetOwningPlayerController(Component);
    auto PlayerState = PlayerController ? (AFortPlayerStateAthena*)PlayerController->PlayerState : nullptr;
    auto bShareWithSquad = ShareActorWith == EShareActorWith::SquadOnTeam || (ShareActorWithMask & (uint8)EShareActorWithMask::SquadOnTeam) != 0;
    auto bShareWithTeam = ShareActorWith == EShareActorWith::AllTeam || (ShareActorWithMask & (uint8)EShareActorWithMask::AllTeam) != 0;

    if (!PlayerState || (!bShareWithSquad && !bShareWithTeam))
        return;

    TArray<AFortPlayerControllerAthena*> PlayerControllers;
    GetAll<AFortPlayerControllerAthena>(PlayerControllers);

    for (auto OtherController : PlayerControllers)
    {
        auto OtherPlayerState = OtherController && OtherController != PlayerController ? (AFortPlayerStateAthena*)OtherController->PlayerState : nullptr;

        if (!OtherPlayerState || OtherPlayerState->TeamIndex != PlayerState->TeamIndex || (!bShareWithTeam && OtherPlayerState->SquadID != PlayerState->SquadID))
            continue;

        if (auto OtherComponent = GetIndicatedActorManagement(OtherController))
            OutComponents.Add(OtherComponent);
    }

    PlayerControllers.Free();
}

static UMaterialParameterCollection* StencilDefinitionCollection = nullptr;

static uint8 GetStencilIndex(FName StencilName)
{
    if (StencilName.IsNone())
        return 0;

    if (!StencilDefinitionCollection)
    {
        auto AssetManager = (UFortAssetManager*)UEngine::GetEngine()->AssetManager;
        auto GameData = AssetManager ? AssetManager->GameDataCommon : nullptr;

        StencilDefinitionCollection = GameData ? GameData->StencilDefinitionCollection.Get() : nullptr;
    }

    if (!StencilDefinitionCollection)
        return 0;

    for (auto& Parameter : StencilDefinitionCollection->ScalarParameters)
    {
        if (Parameter.ParameterName == StencilName)
            return (uint8)(int32)floorf(Parameter.DefaultValue + 0.5f);
    }

    return 0;
}

static void SetSoftActor(TSoftObjectPtr<AActor>& SoftActor, AActor* Actor)
{
    auto SoftObject = UKismetSystemLibrary::Conv_ObjectToSoftObjectReference(Actor);

    memcpy((PBYTE)&SoftActor, (const PBYTE)&SoftObject, sizeof(TSoftObjectPtr<AActor>));
}

template <class TEntry, class TList, class TData>
static void AddActorsToList(UFortControllerComponent_IndicatedActorManagement* Component, TList UFortControllerComponent_IndicatedActorManagement::*List, TArray<AActor*>& Actors, TData& Data, AFortPlayerStateAthena* MarkedBy,
                            bool bAddAsUnique, bool bReplaceExistingEntry, bool bRefreshExistingEntry)
{
    TArray<AActor*> MarkableActors;
    GetMarkableActors(Component, Actors, MarkableActors);

    TArray<UFortControllerComponent_IndicatedActorManagement*> Components;
    GetSharedComponents(Component, Data.ShareActorWith, Data.ShareActorWithMask, Components);

    auto GameState = UWorld::GetWorld()->GameState;
    auto StartTime = GameState ? (float)GameState->GetServerWorldTimeSeconds() : 0.f;
    auto EndTime = Data.duration > 0.f ? StartTime + Data.duration : 0.f;

    for (auto SharedComponent : Components)
    {
        auto& ActorList = SharedComponent->*List;
        auto& Entries = ActorList.Entries;
        bool bChanged = false;

        for (auto Actor : MarkableActors)
        {
            if (!Actor)
                continue;

            TEntry* ExistingEntry = nullptr;

            for (auto& Entry : Entries)
            {
                if (Entry.Actor.WeakPtr.Get() == Actor)
                {
                    ExistingEntry = &Entry;
                    break;
                }
            }

            if (ExistingEntry && (bAddAsUnique || bReplaceExistingEntry || bRefreshExistingEntry))
            {
                if (!bReplaceExistingEntry && !bRefreshExistingEntry)
                    continue;

                if (bReplaceExistingEntry)
                {
                    FreeData(ExistingEntry->Data);
                    CopyFrom(ExistingEntry->Data, Data);

                    if constexpr (std::is_same_v<TEntry, FStenciledActorInfoEntry>)
                        ExistingEntry->MarkedBy = MarkedBy;
                }

                ExistingEntry->StartTime = StartTime;
                ExistingEntry->EndTime = EndTime;
                ExistingEntry->bReplaceExistingWhenAdded = bReplaceExistingEntry;
                ExistingEntry->bRefreshExistingWhenAdded = bRefreshExistingEntry;

                ActorList.MarkItemDirty(*ExistingEntry);
                bChanged = true;
                continue;
            }

            TEntry NewEntry{};
            NewEntry.ReplicationID = -1;
            NewEntry.ReplicationKey = -1;
            NewEntry.MostRecentArrayReplicationKey = -1;
            SetSoftActor(NewEntry.Actor, Actor);

            if constexpr (std::is_same_v<TEntry, FStenciledActorInfoEntry>)
                NewEntry.MarkedBy = MarkedBy;

            NewEntry.StartTime = StartTime;
            NewEntry.EndTime = EndTime;
            CopyFrom(NewEntry.Data, Data);
            NewEntry.bReplaceExistingWhenAdded = bReplaceExistingEntry;
            NewEntry.bRefreshExistingWhenAdded = bRefreshExistingEntry;

            ActorList.MarkItemDirty(Entries.Add(NewEntry));
            bChanged = true;
        }

        if (!bChanged)
            continue;

        SharedComponent->SetComponentTickEnabled(true);

        if (auto Owner = SharedComponent->GetOwner())
            Owner->ForceNetUpdate();
    }

    Components.Free();
    MarkableActors.Free();
}

void AddIndicatedActors(UFortControllerComponent_IndicatedActorManagement* Component, TArray<AActor*>& IndicatedActors, FIndicatedActorData& Data, bool bAddAsUnique, bool bReplaceExistingEntry, bool bRefreshExistingEntry)
{
    AddActorsToList<FIndicatedActorInfoEntry>(Component, &UFortControllerComponent_IndicatedActorManagement::IndicatedActorList, IndicatedActors, Data, nullptr, bAddAsUnique, bReplaceExistingEntry, bRefreshExistingEntry);
}

void AddStenciledActors(UFortControllerComponent_IndicatedActorManagement* Component, TArray<AActor*>& StenciledActors, FStenciledActorData& Data, bool bAddAsUnique, bool bReplaceExistingEntry, bool bRefreshExistingEntry)
{
    auto MarkedBy = GetOwningPlayerState(Component);

    Data.FriendlyStencilIndex = GetStencilIndex(Data.FriendlyStencilName);
    Data.EnemyStencilIndex = GetStencilIndex(Data.EnemyStencilName);

    AddActorsToList<FStenciledActorInfoEntry>(Component, &UFortControllerComponent_IndicatedActorManagement::StenciledActorList, StenciledActors, Data, MarkedBy, bAddAsUnique, bReplaceExistingEntry, bRefreshExistingEntry);
}

static void GetActorsInRadius(AActor* InstigatingActor, FIndicatedActorDataWithFilter& FilterData, TArray<AActor*>& OutActors)
{
    TArray<AActor*> OverlappedActors;
    TArray<AActor*> ActorsToIgnore;
    auto ActorClassFilter = FilterData.ActorClassFilter.Get();
    auto Location = InstigatingActor->K2_GetActorLocation();

    if (FilterData.OverlapHalfHeight > 0.f)
        UKismetSystemLibrary::CapsuleOverlapActors(InstigatingActor, Location, FilterData.OverlapRadius, FilterData.OverlapHalfHeight, FilterData.ObjectTypes, ActorClassFilter, ActorsToIgnore, &OverlappedActors);
    else
        UKismetSystemLibrary::SphereOverlapActors(InstigatingActor, Location, FilterData.OverlapRadius, FilterData.ObjectTypes, ActorClassFilter, ActorsToIgnore, &OverlappedActors);

    auto bHasTagQuery = !UBlueprintGameplayTagLibrary::IsTagQueryEmpty(FilterData.IndicatedActorTagQuery);

    for (auto Actor : OverlappedActors)
    {
        if (!Actor)
            continue;

        if (FilterData.WithAffiliation.Num() > 0)
        {
            auto Affiliation = UFortKismetLibrary::GetTeamAffiliation(InstigatingActor, Actor);

            if (!FilterData.WithAffiliation.Contains(Affiliation))
                continue;
        }

        FGameplayTagContainer OwnedTags{};
        GetOwnedGameplayTags(Actor, OwnedTags);

        auto bMatchesTags = FilterData.IndicatedActorTags.GameplayTags.Num() == 0 || OwnedTags.HasAny(FilterData.IndicatedActorTags);
        auto bMatchesQuery = !bHasTagQuery || UBlueprintGameplayTagLibrary::DoesContainerMatchTagQuery(OwnedTags, FilterData.IndicatedActorTagQuery);

        if (bMatchesTags && bMatchesQuery)
            OutActors.Add(Actor);

        FreeGameplayTags(OwnedTags);
    }

    OverlappedActors.Free();
}

void AddActorsInRadius(FFrame& Stack, bool bStenciled)
{
    AController* InstigatingController;
    bool bAddAsUnique;
    bool bReplaceExistingEntry;
    bool bRefreshExistingEntry;
    AActor* InstigatingActorOverride;

    Stack.StepCompiledIn(&InstigatingController);
    TArray<FIndicatedActorDataWithFilter> ActorFilterDatasTemp{};
    auto& ActorFilterDatas = Stack.StepCompiledInRef(&ActorFilterDatasTemp);
    Stack.StepCompiledIn(&bAddAsUnique);
    Stack.StepCompiledIn(&bReplaceExistingEntry);
    Stack.StepCompiledIn(&bRefreshExistingEntry);
    Stack.StepCompiledIn(&InstigatingActorOverride);
    Stack.IncrementCode();

    auto Component = InstigatingController ? GetIndicatedActorManagement(InstigatingController) : nullptr;
    auto InstigatingActor = InstigatingActorOverride ? InstigatingActorOverride : (InstigatingController ? InstigatingController->Pawn : nullptr);

    if (!Component || !InstigatingActor)
        return;

    for (auto& FilterData : ActorFilterDatas)
    {
        TArray<AActor*> Actors;
        GetActorsInRadius(InstigatingActor, FilterData, Actors);

        if (Actors.Num() > 0)
        {
            if (bStenciled)
                AddStenciledActors(Component, Actors, FilterData.StenciledData, bAddAsUnique, bReplaceExistingEntry, bRefreshExistingEntry);
            else
                AddIndicatedActors(Component, Actors, FilterData.IndicatedData, bAddAsUnique, bReplaceExistingEntry, bRefreshExistingEntry);
        }

        Actors.Free();
    }
}
