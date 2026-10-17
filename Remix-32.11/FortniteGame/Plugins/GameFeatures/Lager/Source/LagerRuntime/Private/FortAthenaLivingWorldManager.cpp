#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Plugins/GameFeatures/Lager/Source/LagerRuntime/Public/FortAthenaLivingWorldManager.h"

INIT_MODULE(UFortAthenaLivingWorldManager);

struct FPendingSpawnRequest
{
    uint8 Data[0x90];
};

struct FPendingSpawnActor
{
    uint8 Data[0x70];
};

static void (*OnRegisterOG)(UFortAthenaLivingWorldManager*);
static void (*TickComponentOG)(UFortAthenaLivingWorldManager*, float, uint8, void*);

static int32 NextActorSpawnRequestID = -2;

template <typename Fn>
static void ForEachSetElement(uint8* Set, int32 ElementSize, Fn&& Callback)
{
    auto Elements = *(uint8**)Set;
    auto SecondaryFlags = *(uint32**)(Set + 0x20);
    auto AllocationFlags = SecondaryFlags ? SecondaryFlags : (uint32*)(Set + 0x10);
    auto NumBits = *(int32*)(Set + 0x28);

    for (int32 i = 0; i < NumBits; i++)
        if ((AllocationFlags[i / 32] & (1u << (i % 32))) && !Callback(Elements + int64(i) * ElementSize))
            return;
}

static bool DoesSpawnerDataMatchQuery(const UClass* SpawnerDataClass, FGameplayTagQuery& Query)
{
    auto SpawnerData = (UFortAthenaSpawnerDataBase*)SpawnerDataClass->ClassDefaultObject;

    return SpawnerData && UBlueprintGameplayTagLibrary::DoesContainerMatchTagQuery(SpawnerData->DescriptorTag, Query);
}

static bool TrySpawnEvent(UFortAthenaLivingWorldManager* LivingWorldManager, uint8* CategoryRow, uint8* CategoryRuntime, FFortAthenaLivingWorldEvent* EventRow, uint8* EventRuntime, int32 EventDataIndex,
                          TScriptInterface<IFortAthenaLivingWorldPointProviderInterface> PointProvider)
{
    auto& EventDatas = *(TArray<uint8>*)EventRuntime;
    auto& SpawnDatas = *(TArray<uint8>*)(EventRuntime + 0x10);
    auto EventDataRuntime = EventDatas.Data + EventDataIndex * 0xA0;
    auto EventData = ((FWeakObjectPtr*)(EventDataRuntime + 0x68))->Get();

    float TotalWeight = 0.f;

    for (int32 i = 0; i < SpawnDatas.Num(); i++)
    {
        auto SpawnData = SpawnDatas.Data + i * 0x48;

        if (*(int32*)(SpawnData + 0x40) == EventDataIndex && *(float*)(SpawnData + 0x3C) > 0.f)
            TotalWeight += *(float*)(SpawnData + 0x3C);
    }

    if (TotalWeight <= 0.f)
        return false;

    uint8* SpawnData = nullptr;
    auto Pick = ((float)rand() / RAND_MAX) * TotalWeight;

    for (int32 i = 0; i < SpawnDatas.Num() && !SpawnData; i++)
    {
        auto Candidate = SpawnDatas.Data + i * 0x48;

        if (*(int32*)(Candidate + 0x40) != EventDataIndex || *(float*)(Candidate + 0x3C) <= 0.f)
            continue;

        Pick -= *(float*)(Candidate + 0x3C);

        if (Pick <= 0.f)
            SpawnData = Candidate;
    }

    if (!SpawnData)
        return false;

    auto& ActorDescriptions = *(TArray<uint8>*)SpawnData;
    auto MinActors = *(int32*)(SpawnData + 0x34);
    auto MaxActors = max(*(int32*)(SpawnData + 0x38), MinActors);
    auto NumActors = min(MinActors + (MaxActors > MinActors ? rand() % (MaxActors - MinActors + 1) : 0), ActorDescriptions.Num());

    if (NumActors <= 0)
        return false;

    auto& Limiter = LivingWorldManager->SpawnerDataLimiter;
    std::vector<const UClass*> SpawnerDataClasses;

    for (int32 i = 0; i < NumActors; i++)
    {
        auto ActorDescription = ActorDescriptions.Data + i * 0x88;
        auto Candidates = *(const UClass***)(ActorDescription + 0x8);
        auto NumCandidates = *(int32*)(ActorDescription + 0x10);

        if (!Candidates)
            Candidates = (const UClass**)ActorDescription;

        std::vector<const UClass*> ValidCandidates;

        for (int32 x = 0; x < NumCandidates; x++)
        {
            auto Class = Candidates[x];
            auto SpawnerData = Class ? Class->ClassDefaultObject : nullptr;

            if (!SpawnerData || (!SpawnerData->IsA(UFortAthenaAISpawnerData::StaticClass()) && !SpawnerData->IsA(UFortAthenaActorSpawnerData::StaticClass())))
                continue;

            if (Limiter.SpawnedSingleUseSpawnerData.Contains((UClass*)Class) || Limiter.SpawnedConcurrentSingleUseSpawnerData.Contains((UClass*)Class) ||
                std::find(SpawnerDataClasses.begin(), SpawnerDataClasses.end(), Class) != SpawnerDataClasses.end() &&
                    (DoesSpawnerDataMatchQuery(Class, Limiter.SingleUseSpawnerDataQuery) || DoesSpawnerDataMatchQuery(Class, Limiter.ConcurrentSingleUseSpawnerDataQuery)))
                continue;

            ValidCandidates.push_back(Class);
        }

        if (ValidCandidates.empty())
            return false;

        SpawnerDataClasses.push_back(ValidCandidates[rand() % ValidCandidates.size()]);
    }

    uint8 SpawnContext[0x38] = {};
    *(void**)(SpawnContext + 0x8) = EventRow->TagDensityConditions.Data;
    *(int32*)(SpawnContext + 0x10) = EventRow->TagDensityConditions.Num();
    *(int32*)(SpawnContext + 0x18) = NumActors;
    *(float*)(SpawnContext + 0x24) = EventRow->MaxActorDensityOverride;
    *(float*)(SpawnContext + 0x28) = EventRow->MinDistanceToPlayer;
    *(float*)(SpawnContext + 0x2C) = EventRow->MaxDistanceToPlayer;
    *(bool*)(SpawnContext + 0x30) = EventRow->bCanSpawnWithoutMatchingPlayerDistance;
    *(bool*)(SpawnContext + 0x31) = EventRow->bOnlyInSafeZone;
    *(bool*)(SpawnContext + 0x32) = EventRow->bOnlyInNextSafeZone;
    *(bool*)(SpawnContext + 0x33) = EventRow->bOnlyInNextSafeZoneIfStormShrinking;

    auto InterfaceVft = *(void***)PointProvider.InterfacePointer;

    if (!((bool (*)(const void*, void*))InterfaceVft[0xA])(PointProvider.InterfacePointer, SpawnContext))
        return false;

    FVector Location{};
    FRotator Rotation{};

    if (!((bool (*)(const void*, void*, FVector*, FRotator*))InterfaceVft[0x2])(PointProvider.InterfacePointer, SpawnContext, &Location, &Rotation))
        return false;

    auto IsAISpawnerData = [](const UClass* SpawnerDataClass) { return SpawnerDataClass->ClassDefaultObject->IsA(UFortAthenaAISpawnerData::StaticClass()); };
    auto bSpawnsAI = std::any_of(SpawnerDataClasses.begin(), SpawnerDataClasses.end(), IsAISpawnerData);
    auto AISpawner = bSpawnsAI ? UFortAIFunctionLibrary::GetAISpawner(LivingWorldManager) : nullptr;

    if (bSpawnsAI && !AISpawner)
        return false;

    std::vector<UFortAthenaAISpawnerDataComponentList*> ComponentLists;

    for (auto& SpawnerDataClass : SpawnerDataClasses)
    {
        if (!IsAISpawnerData(SpawnerDataClass))
        {
            ComponentLists.push_back(nullptr);
            continue;
        }

        auto ComponentList = UFortAthenaAISpawnerData::CreateComponentListFromClass((UClass*)SpawnerDataClass, LivingWorldManager);

        if (!ComponentList)
            return false;

        ComponentLists.push_back(ComponentList);
    }

    struct
    {
        const UObject* EventData;
        int32 NumActors;
    } EventRequest{ EventData, NumActors };

    ((void (*)(const void*, void*))InterfaceVft[0x7])(PointProvider.InterfacePointer, &EventRequest);

    *(int32*)(EventRuntime + 0x2C) += NumActors;
    *(int32*)(EventRuntime + 0x30) += NumActors;
    *(int32*)(EventRuntime + 0x34) += 1;
    *(int32*)(EventRuntime + 0x38) += 1;
    *(int32*)(CategoryRuntime + 0x24) += NumActors;
    *(int32*)(CategoryRuntime + 0x28) += NumActors;
    *(int32*)(CategoryRuntime + 0x2C) += 1;
    *(int32*)(CategoryRuntime + 0x30) += 1;

    FPendingSpawnRequest PendingRequest{};
    auto Request = PendingRequest.Data;
    *(uint8**)(Request + 0x0) = CategoryRow;
    *(uint8**)(Request + 0x8) = (uint8*)EventRow;
    *(uint8**)(Request + 0x10) = SpawnData;
    *(TScriptInterface<IFortAthenaLivingWorldPointProviderInterface>*)(Request + 0x20) = PointProvider;
    *(int32*)(Request + 0x30) = NumActors;
    memcpy(Request + 0x40, &Location, 0x18);
    memcpy(Request + 0x58, &Rotation, 0x18);

    auto& Actors = *(TArray<FPendingSpawnActor>*)(Request + 0x70);
    auto SpawnTransform = UKismetMathLibrary::MakeTransform(Location, Rotation, FVector(1, 1, 1));
    std::vector<int32> RequestIDs;

    for (int32 i = 0; i < NumActors; i++)
    {
        FPendingSpawnActor PendingActor{};
        auto Actor = PendingActor.Data;
        *(const UClass**)(Actor + 0x38) = SpawnerDataClasses[i];
        *(int32*)(Actor + 0x48) = ComponentLists[i] ? AISpawner->RequestSpawn(ComponentLists[i], SpawnTransform, false) : NextActorSpawnRequestID--;
        RequestIDs.push_back(*(int32*)(Actor + 0x48));
        *(int32*)(Actor + 0x4C) = -1;
        *(int32*)(Actor + 0x64) = 1;
        *(int32*)(Actor + 0x68) = i;
        Actors.Add(PendingActor);

        if (DoesSpawnerDataMatchQuery(SpawnerDataClasses[i], Limiter.SingleUseSpawnerDataQuery))
            Limiter.SpawnedSingleUseSpawnerData.Add((UClass*)SpawnerDataClasses[i]);

        if (DoesSpawnerDataMatchQuery(SpawnerDataClasses[i], Limiter.ConcurrentSingleUseSpawnerDataQuery))
            Limiter.SpawnedConcurrentSingleUseSpawnerData.Add((UClass*)SpawnerDataClasses[i]);
    }

    (*(TArray<FPendingSpawnRequest>*)((uint8*)LivingWorldManager + 0x808)).Add(PendingRequest);

    for (int32 i = 0; i < NumActors; i++)
    {
        if (ComponentLists[i])
            continue;

        auto ActorClass = ((UFortAthenaActorSpawnerData*)SpawnerDataClasses[i]->ClassDefaultObject)->ActorClass;
        auto SpawnedActor = ActorClass ? SpawnActor(ActorClass, SpawnTransform, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn) : nullptr;

        LivingWorldManager->OnActorSpawned(SpawnedActor, RequestIDs[i]);
    }

    return true;
}

static void GenerateEvents(UFortAthenaLivingWorldManager* LivingWorldManager, float Now)
{
    auto MaxEventSpawns = (int32)LivingWorldManager->CachedConfig->MaxEventSpawnPerTick.Evaluate();
    int32 EventSpawns = 0;

    if (MaxEventSpawns <= 0)
        MaxEventSpawns = INT32_MAX;

    std::vector<std::pair<FFortAthenaLivingWorldCategory*, uint8*>> Categories;

    ForEachSetElement((uint8*)LivingWorldManager + 0x900, 0x48, [&](uint8* Element)
    {
        Categories.push_back({ *(FFortAthenaLivingWorldCategory**)Element, Element + 8 });
        return true;
    });

    std::stable_sort(Categories.begin(), Categories.end(), [](auto& A, auto& B) { return A.first->Priority > B.first->Priority; });

    for (auto& [CategoryRow, CategoryRuntime] : Categories)
    {
        auto EventTable = *(UDataTable**)(CategoryRuntime + 0x18);

        if (!EventTable || !EventTable->IsA(UDataTable::StaticClass()))
            continue;

        std::vector<std::tuple<FName, FFortAthenaLivingWorldEvent*, uint8*>> Events;

        for (auto& Row : EventTable->RowMap)
        {
            auto EventRow = (FFortAthenaLivingWorldEvent*)Row.Value();
            uint8* EventRuntime = nullptr;

            ForEachSetElement((uint8*)LivingWorldManager + 0x8B0, 0x58, [&](uint8* Element)
            {
                if (*(FFortAthenaLivingWorldEvent**)Element != EventRow)
                    return true;

                EventRuntime = Element + 8;
                return false;
            });

            if (!EventRuntime || *(int32*)(EventRuntime + 0x40) != 0 || *(float*)(EventRuntime + 0x24) < 0.f || Now < *(float*)(EventRuntime + 0x24))
                continue;

            Events.push_back({ Row.Key(), EventRow, EventRuntime });
        }

        while (!Events.empty())
        {
            float TotalWeight = 0.f;

            for (auto& Event : Events)
                TotalWeight += max(std::get<1>(Event)->Weight, 0.f);

            auto Pick = ((float)rand() / RAND_MAX) * TotalWeight;
            size_t Index = 0;

            for (; Index < Events.size() - 1; Index++)
            {
                Pick -= max(std::get<1>(Events[Index])->Weight, 0.f);

                if (Pick <= 0.f)
                    break;
            }

            auto& [RowName, EventRow, EventRuntime] = Events[Index];
            auto& EventDatas = *(TArray<uint8>*)EventRuntime;
            bool bSpawned = false;

            for (int32 i = 0; i < EventDatas.Num() && !bSpawned; i++)
            {
                auto EventDataRuntime = EventDatas.Data + i * 0xA0;

                if (!*(bool*)(EventDataRuntime + 0x98))
                    continue;

                auto& CandidatePointProviders = *(TArray<TScriptInterface<IFortAthenaLivingWorldPointProviderInterface>>*)(EventDataRuntime + 0x10);
                std::vector<TScriptInterface<IFortAthenaLivingWorldPointProviderInterface>> PointProviders;

                for (int32 x = 0; x < CandidatePointProviders.Num(); x++)
                    if (CandidatePointProviders.Data[x].ObjectPointer && CandidatePointProviders.Data[x].InterfacePointer)
                        PointProviders.push_back(CandidatePointProviders.Data[x]);

                for (size_t x = PointProviders.size(); x > 1; x--)
                    std::swap(PointProviders[x - 1], PointProviders[rand() % x]);

                for (auto& PointProvider : PointProviders)
                {
                    FDataTableRowHandle EventEntry{ EventTable, RowName };

                    if (LivingWorldManager->QueryEventBudget(EventEntry, (AActor*)PointProvider.ObjectPointer) > 0 &&
                        TrySpawnEvent(LivingWorldManager, (uint8*)CategoryRow, CategoryRuntime, EventRow, EventRuntime, i, PointProvider))
                    {
                        bSpawned = true;
                        break;
                    }
                }
            }

            if (!bSpawned)
            {
                Events.erase(Events.begin() + Index);
                continue;
            }

            if (++EventSpawns >= MaxEventSpawns)
                return;
        }
    }
}

static void OnRegister(UFortAthenaLivingWorldManager* LivingWorldManager)
{
    LivingWorldManager->PrimaryComponentTick.bCanEverTick = true;

    OnRegisterOG(LivingWorldManager);

    auto GameState = (AFortGameStateAthena*)LivingWorldManager->GetOwner();

    if (!GameState || !GameState->IsA(AFortGameStateAthena::StaticClass()) || GameState->Role != ENetRole::ROLE_Authority)
        return;

    for (auto Outer = LivingWorldManager->Outer; Outer; Outer = Outer->Outer)
    {
        if (Outer->IsA(UWorld::StaticClass()))
        {
            LivingWorldManager->CachedWorld = (UWorld*)Outer;
            break;
        }
    }

    if (auto AISpawner = UFortAIFunctionLibrary::GetAISpawner(LivingWorldManager))
        AISpawner->OnPawnSpawnedEvent.Bind(LivingWorldManager, FName(L"OnActorSpawned"));

    NotifyWhenPlaylistLoaded(GameState, LivingWorldManager);
}

static void TickComponent(UFortAthenaLivingWorldManager* LivingWorldManager, float DeltaTime, uint8 TickType, void* ThisTickFunction)
{
    TickComponentOG(LivingWorldManager, DeltaTime, TickType, ThisTickFunction);

    if (!LivingWorldManager->CachedWorld || !LivingWorldManager->CachedConfig || !*(bool*)((uint8*)LivingWorldManager + 0x982))
        return;

    auto Now = (float)UGameplayStatics::GetTimeSeconds(LivingWorldManager->CachedWorld);
    auto& NextEventGenerationTime = *(float*)((uint8*)LivingWorldManager + 0x958);

    if (auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(LivingWorldManager))
    {
        auto SafeZoneIndex = GamePhaseLogic->SafeZoneIndicator ? GamePhaseLogic->SafeZoneIndicator->CurrentPhase : 0;

        ForEachSetElement((uint8*)LivingWorldManager + 0x8B0, 0x58, [&](uint8* Element)
        {
            auto EventRow = *(FFortAthenaLivingWorldEvent**)Element;
            auto EventRuntime = Element + 8;
            auto& ActivationTime = *(float*)(EventRuntime + 0x24);

            if (*(int32*)(EventRuntime + 0x40) != 0 || ActivationTime >= 0.f)
                return true;

            if (GamePhaseLogic->GamePhaseStep < EventRow->ActivationGamePhaseStep || SafeZoneIndex < EventRow->ActivationSafezoneIndex)
                return true;

            auto MaxDelay = max(EventRow->ActivationMaxDelay, EventRow->ActivationMinDelay);

            ActivationTime = Now + EventRow->ActivationMinDelay + (MaxDelay - EventRow->ActivationMinDelay) * ((float)rand() / RAND_MAX);
            return true;
        });
    }

    if (Now < NextEventGenerationTime)
        return;

    NextEventGenerationTime = Now + LivingWorldManager->CachedConfig->MinimumEventGenerationInterval.Evaluate();

    GenerateEvents(LivingWorldManager, Now);
}

void UFortAthenaLivingWorldManager__Init()
{
    Hook<UFortAthenaLivingWorldManager>(uint32(0x68), OnRegister, OnRegisterOG);
    Hook<UFortAthenaLivingWorldManager>(uint32(0x7B), TickComponent, TickComponentOG);
}
