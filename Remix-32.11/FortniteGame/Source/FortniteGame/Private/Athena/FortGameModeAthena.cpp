#include "pch.h"
#include "Shared.h"
#include "Config.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortGameModeAthena.h"
#include "FortniteGame/Source/FortniteAI/Public/AI/AthenaAISystem.h"
#include "FortniteGame/Source/FortniteAI/Public/AI/Navigation/AthenaNavSystem.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/ControllerComponents/FortControllerComponent_TournamentStatsTracker.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortAthenaVehicleSpawner.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortPlayerControllerAthena.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/GameStateComponents/FortGameStateComponent_BattleRoyaleGamePhaseLogic.h"
#include "FortniteGame/Source/FortniteGame/Public/Building/BGAConsumableSpawner.h"
#include "FortniteGame/Source/FortniteGame/Public/Building/BuildingItemCollectorActor.h"
#include "FortniteGame/Source/FortniteGame/Public/FortGameModeZone.h"
#include "FortniteGame/Source/FortniteGame/Public/FortKismetLibrary.h"
#include "FortniteGame/Source/FortniteGame/Public/Quests/FortQuestManager.h"

INIT_MODULE(AFortGameModeAthena);
INIT_POSTLOAD(AFortGameModeAthena);

static TArray<UFortAbilitySet*> AbilitySets;
static bool bSkipWarmup = false;
bool bSkipAircraft = false;
uint8 PauseWarmupMask = 0;

static TArray<FName> LevelsToStream;

static std::wstring ConfiguredPlaylist;
static bool bConfiguredLateGame = false;

int GetLateGamePhase(AFortGameStateAthena* GameState)
{
    auto& SafeZoneDefinition = GameState->MapInfo->SafeZoneDefinition;
    auto Count = (int)SafeZoneDefinition.Count.Evaluate();
    auto FinalPhase = Count - 1;

    for (int i = 1; i < Count; i++)
    {
        if (SafeZoneDefinition.Radius.Evaluate((float)i) <= 0.f)
        {
            FinalPhase = i;
            break;
        }
    }

    return FinalPhase - 9 + LateGameZone;
}

void PauseWarmup(EGameModePauseWarmupMaskId MaskId)
{
    PauseWarmupMask |= uint8(MaskId);
}

void UnPauseWarmup(EGameModePauseWarmupMaskId MaskId)
{
    auto bWasPaused = PauseWarmupMask != 0;

    PauseWarmupMask &= ~uint8(MaskId);

    auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(UWorld::GetWorld());

    if (bWasPaused && !PauseWarmupMask && GamePhaseLogic && GamePhaseLogic->GamePhase == EAthenaGamePhase::Warmup)
    {
        auto Time = (float)UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());

        GamePhaseLogic->WarmupCountdownStartTime = Time;
        GamePhaseLogic->WarmupCountdownEndTime = Time + GamePhaseLogic->WarmupCountdownDuration;
    }
}

static FProperty* FindProperty(const UObject* Object, const char* Name)
{
    for (const UStruct* Struct = Object->Class; Struct; Struct = Struct->SuperStruct)
    {
        auto Link = uint64(Struct->ChildProperties);

        for (auto Field = Link ? (FField*)(~_rotr64(Link ^ 0xAC0E7F64ull, 0x10)) : nullptr; Field; Field = Field->GetNext())
        {
            if (Field->Name.ToString() == Name)
                return (FProperty*)Field;
        }
    }

    return nullptr;
}

template <typename T>
static T& GetProperty(const UObject* Object, const FProperty* Property)
{
    return *(T*)(__int64(Object) + (Property->Offset ^ 0xE026103B));
}

static bool HasTag(const TArray<FGameplayTag>& Tags, const FGameplayTag& Tag)
{
    for (auto& Other : Tags)
    {
        if (Other.TagName == Tag.TagName)
            return true;
    }

    return false;
}

static bool bCalendarPlaylistDone = false;
static ULONGLONG CalendarWaitStart = 0;

static bool SelectCalendarPlaylist()
{
    if (bCalendarPlaylistDone || bReload)
        return true;

    auto ManagerClass = UObject::FindClassFast("BP_Clyde_DataLayerManager_C");
    auto Manager = ManagerClass ? GetFirstInstance(ManagerClass) : nullptr;
    auto CurrentPlaylist = FindObject<UFortPlaylistAthena>(Playlist.c_str());

    if (!Manager || !CurrentPlaylist || CurrentPlaylist->bIsTournament)
        return bCalendarPlaylistDone = true;

    auto EventsProperty = FindProperty(Manager, "ClydeWeekCalendarEventsToWatch");
    auto WeekTagsProperty = FindProperty(Manager, "Apollo Retro Seasonly Gameplay Tags");
    auto UsePlaylistsProperty = (FBoolProperty*)FindProperty(Manager, "bShouldUseClydePlaylists");

    if (!EventsProperty || !WeekTagsProperty)
        return bCalendarPlaylistDone = true;

    if (UsePlaylistsProperty && (GetProperty<uint8>(Manager, UsePlaylistsProperty) & UsePlaylistsProperty->FieldMask))
        return bCalendarPlaylistDone = true;

    auto& Events = GetProperty<TArray<FString>>(Manager, EventsProperty);
    auto& WeekTags = GetProperty<TArray<FGameplayTag>>(Manager, WeekTagsProperty);
    auto& PlaylistTags = CurrentPlaylist->GameplayTagContainer.GameplayTags;

    for (auto& Tag : PlaylistTags)
    {
        if (HasTag(WeekTags, Tag))
            return bCalendarPlaylistDone = true;
    }

    auto World = UWorld::GetWorld();
    auto GameInstance = World ? (UFortGameInstance*)World->OwningGameInstance : nullptr;
    auto EventManager = GameInstance ? GameInstance->SeasonalEventManager : nullptr;

    if (!EventManager)
        return bCalendarPlaylistDone = true;

    if (*(uint8*)(__int64(EventManager) + 0x180))
    {
        if (!CalendarWaitStart)
            CalendarWaitStart = GetTickCount64();

        if (GetTickCount64() - CalendarWaitStart < 60000)
            return false;

        return bCalendarPlaylistDone = true;
    }

    int32 Week = -1;

    for (int32 i = 0; i < Events.Num(); i++)
    {
        bool bIsEventActive = false;
        FTimespan TimeSinceBegin{};
        FTimespan TimeUntilEnd{};
        float TimespanRatio = 0.f;

        UFortKismetLibrary::GetCalendarEventInformation(World, Events[i], &bIsEventActive, &TimeSinceBegin, &TimeUntilEnd, &TimespanRatio);

        if (bIsEventActive)
            Week = i;
    }

    if (Week < 0 || Week >= WeekTags.Num())
        return bCalendarPlaylistDone = true;

    auto& WeekTag = WeekTags[Week];

    const UObject* ManagerPackage = Manager->Class;

    while (ManagerPackage && ManagerPackage->Outer)
        ManagerPackage = ManagerPackage->Outer;

    auto PackageName = ManagerPackage ? ManagerPackage->Name.GetRawString() : std::string();
    auto RootEnd = PackageName.size() > 1 ? PackageName.find('/', 1) : std::string::npos;

    if (RootEnd == std::string::npos)
        return bCalendarPlaylistDone = true;

    std::wstring Root(PackageName.begin(), PackageName.begin() + RootEnd + 1);
    auto Level = CurrentPlaylist->PreloadPersistentLevel.ObjectID.AssetPath.PackageName.GetRawString();
    std::wstring MatchPath;
    int32 Matches = 0;
    int32 BestSharedTags = -1;

    TArray<FPrimaryAssetId> AssetIds;
    UKismetSystemLibrary::GetPrimaryAssetIdList(FPrimaryAssetType{ FName(L"FortPlaylistAthena") }, &AssetIds);

    for (auto& AssetId : AssetIds)
    {
        auto PathString = UKismetSystemLibrary::Conv_SoftObjectReferenceToString(UKismetSystemLibrary::GetSoftObjectReferenceFromPrimaryAssetId(AssetId));

        if (PathString.Num() <= 1)
        {
            PathString.Free();
            continue;
        }

        std::wstring Path(PathString.Data);
        PathString.Free();

        if (Path.compare(0, Root.size(), Root) != 0)
            continue;

        auto Candidate = FindObject<UFortPlaylistAthena>(Path.c_str());

        if (!Candidate || Candidate == CurrentPlaylist || Candidate->MaxSquadSize != CurrentPlaylist->MaxSquadSize || Candidate->PreloadPersistentLevel.ObjectID.AssetPath.PackageName.GetRawString() != Level)
            continue;

        bool bHasWeekTag = false;
        bool bSameMode = true;
        int32 SharedTags = 0;

        for (auto& Tag : Candidate->GameplayTagContainer.GameplayTags)
        {
            if (Tag.TagName == WeekTag.TagName)
                bHasWeekTag = true;
            else if (HasTag(PlaylistTags, Tag))
                SharedTags++;
            else
                bSameMode = false;
        }

        if (!bHasWeekTag || !bSameMode || SharedTags < BestSharedTags)
            continue;

        if (SharedTags > BestSharedTags)
        {
            BestSharedTags = SharedTags;
            Matches = 0;
        }

        Matches++;
        MatchPath = Path;
    }

    AssetIds.Free();

    if (Matches == 1)
        Playlist = MatchPath;

    return bCalendarPlaylistDone = true;
}

static std::wstring GetPluginName(const FName& LevelToStream)
{
    auto Level = LevelToStream.GetRawString();
    auto PluginName = Level.substr(1, Level.find('/', 1) - 1);

    return std::wstring(PluginName.begin(), PluginName.end());
}

static bool IsGameFeaturePluginActive(const std::wstring& PluginName)
{
    static auto GetGameFeatureDataForActivePlugin = (UGameFeatureData* (*)(UGameFeaturePluginStateMachine*))(ImageBase + 0x1862848);
    auto PluginDescriptor = L"/" + PluginName + L".uplugin";

    for (auto& Pair : ((UGameFeaturesSubsystem*)USubsystemBlueprintLibrary::GetEngineSubsystem(UGameFeaturesSubsystem::StaticClass()))->GameFeaturePluginStateMachines)
    {
        if (Pair.Key().ToWString().ends_with(PluginDescriptor))
            return Pair.Value() && GetGameFeatureDataForActivePlugin(Pair.Value());
    }

    return false;
}

static UFortPlaylistAthena* FindPlaylist()
{
    if (auto Found = FindObject<UFortPlaylistAthena>(Playlist.c_str()))
        return Found;

    return FindObject<UFortPlaylistAthena>(L"/BRPlaylists/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo");
}

static void (*ReadyToStartMatchOG)(UObject*, FFrame&, bool*);
static void ReadyToStartMatch(UObject* Context, FFrame& Stack, bool* Ret)
{
    static auto GetWorldContextFromWorld = (void* (*)(UEngine*, UWorld*))(ImageBase + 0x1973A84);
    static auto CreateNetDriver_Local = (UNetDriver* (*)(UEngine*, void*, FName, FName))(ImageBase + 0x2EF3758);
    static auto SetWorld = (void (*)(UNetDriver*, UWorld*))(ImageBase + 0x29664C8);
    static auto InitListen = (bool (*)(UNetDriver*, UWorld*, FURL*, bool, FString&))(ImageBase + 0x734AB88);
    static auto SetAISettings = (void (*)(AFortGameModeAthena*, UAthenaAISettings*))(ImageBase + 0x91891A4);
    static auto StartStreamingAdditionalPlaylistLevel = (void (*)(AFortGameStateAthena*, FName, bool))(ImageBase + 0x9A65E50);
    static auto GetLevelsToStream = (bool (*)(UFortGameStateComponent_UEFNProjects*, TArray<FName>*))(ImageBase + 0x92CAAD8);
    static auto ChangeGameFeatureTargetState = (void (*)(UGameFeaturesSubsystem*, const FString&, EGameFeatureTargetState, const void*))(ImageBase + 0x23F5F80);
    static auto InitializePoiManager = (void (*)(AFortPoiManager*))(ImageBase + 0x927B760);
    static auto UpdateCullDistanceServer = (void (*)(ABuildingActor*))(ImageBase + 0x94CD838);
    static auto InitializeMapInfo = (AFortAthenaMapInfo* (*)(AFortGameModeAthena*, AFortGameStateAthena*))(ImageBase + 0x918DD7C);
    static auto InitializeFlightPath = (void (*)(AFortAthenaMapInfo*, AFortGameStateAthena*, UFortGameStateComponent_BattleRoyaleGamePhaseLogic*, bool, double, float, float))(ImageBase + 0x9101F80);

    Stack.IncrementCode();

    if (Context->IsA(AFortGameModeFrontEnd::StaticClass()))
    {
        *Ret = callOGWithRet(((AGameMode*)Context), Stack.CurrentNativeFunction, ReadyToStartMatch);
        return;
    }

    auto GameMode = (AFortGameModeAthena*)Context;
    auto GameState = (AFortGameStateAthena*)GameMode->GameState;

    if (!MissedSelfSpawns.empty() && GameState->bPlaylistDataIsLoaded)
        ReplayMissedSelfSpawns();

    if (GameMode->WarmupRequiredPlayerCount != 1)
    {
        if (!SelectCalendarPlaylist())
        {
            *Ret = false;
            return;
        }

        auto World = UWorld::GetWorld();
        auto Engine = UEngine::GetEngine();
        auto NetDriverName = FName(L"GameNetDriver");

        GameMode->bEnableReplicationGraph = true;

        auto WorldContext = GetWorldContextFromWorld(Engine, World);
        auto NetDriver = World->NetDriver = CreateNetDriver_Local(Engine, WorldContext, NetDriverName, FName());

        NetDriver->NetServerMaxTickRate = 30;
        NetDriver->NetDriverName = NetDriverName;
        NetDriver->World = World;
        *(bool*)(__int64(NetDriver) + 0x811) = true;

        for (auto& LevelCollection : World->LevelCollections)
            LevelCollection.NetDriver = NetDriver;

        FURL URL{};
        URL.Port = Port;

        FString Error;

        SetWorld(NetDriver, World);

        if (InitListen(NetDriver, World, &URL, false, Error))
            SetWorld(NetDriver, World);

        GameMode->WarmupRequiredPlayerCount = 1;

        auto Playlist = FindPlaylist();

        if (Playlist)
        {
            Playlist->GarbageCollectionFrequency = 9999999999999999.f;
            GameMode->PlaylistHotfixOriginalGCFrequency = 9999999999999999.f;
            GameMode->bDisableGCOnServerDuringMatch = true;
            GameMode->bPlaylistHotfixChangedGCDisabling = true;

            bForceRespawns = bForceRespawns && Playlist->RespawnType == EAthenaRespawnType::None;

            if (bForceRespawns)
            {
                Playlist->RespawnType = bStormRespawns ? EAthenaRespawnType::InfiniteRespawn : EAthenaRespawnType::InfiniteRespawnExceptStorm;
                Playlist->bRespawnInAir = true;
                Playlist->RespawnHeight.Value = 20000.f;
                Playlist->RespawnHeight.Curve.CurveTable = nullptr;
                Playlist->RespawnHeight.Curve.RowName = FName();
                Playlist->RespawnTime.Value = 3.f;
                Playlist->RespawnTime.Curve.CurveTable = nullptr;
                Playlist->RespawnTime.Curve.RowName = FName();
                Playlist->bAllowJoinInProgress = true;
            }

            if (auto AISettings = Playlist->AISettings.Get())
            {
                auto& AIServices = AISettings->AIServices;
                bool bHasPlayerBotsService = false;

                for (int32 i = 0; i < AIServices.Num(); i++)
                {
                    auto ServiceClass = (UClass*)AIServices[i];

                    if (ServiceClass && ServiceClass->ClassDefaultObject->IsA(UAthenaAIServicePlayerBots::StaticClass()))
                        bHasPlayerBotsService = true;
                }

                for (int32 i = 0; i < AIServices.Num() && !bHasPlayerBotsService; i++)
                {
                    if (!(UClass*)AIServices[i])
                    {
                        AIServices[i] = UAthenaAIServicePlayerBots::StaticClass();
                        bHasPlayerBotsService = true;
                    }
                }

                auto AISystem = (UFortAISystem*)World->AISystem;

                if (AISystem && AISystem->IsA(UAthenaAISystem::StaticClass()))
                    CreateSubsystems((UAthenaAISystem*)AISystem, GameMode);

                SetAISettings(GameMode, AISettings);
            }

            GameState->CurrentPlaylistInfo.BasePlaylist = Playlist;
            GameState->CurrentPlaylistInfo.PlaylistReplicationKey++;
            GameState->CurrentPlaylistInfo.MarkArrayDirty();
            GameState->OnRep_CurrentPlaylistInfo();

            GameMode->CurrentPlaylistId = Playlist->PlaylistId;
            GameState->CurrentPlaylistId = Playlist->PlaylistId;
            GameMode->CurrentPlaylistName = Playlist->PlaylistName;
            GameMode->GameSession->MaxPlayers = Playlist->MaxPlayers;

            auto StreamingPlaylist = GameState->CurrentPlaylistInfo.OverridePlaylist ? GameState->CurrentPlaylistInfo.OverridePlaylist : GameState->CurrentPlaylistInfo.BasePlaylist;

            for (auto& AdditionalLevel : StreamingPlaylist->AdditionalLevels)
                StartStreamingAdditionalPlaylistLevel(GameState, AdditionalLevel.ObjectID.AssetPath.PackageName, false);

            for (auto& AdditionalLevel : StreamingPlaylist->AdditionalLevelsServerOnly)
                StartStreamingAdditionalPlaylistLevel(GameState, AdditionalLevel.ObjectID.AssetPath.PackageName, true);

            if (StreamingPlaylist->SharedAssetGroup)
            {
                for (auto& SharedAsset : StreamingPlaylist->SharedAssetGroup->SharedAssetsToLoad)
                {
                    for (auto& AdditionalLevel : SharedAsset->SharedAdditionalLevels)
                        StartStreamingAdditionalPlaylistLevel(GameState, AdditionalLevel.ObjectID.AssetPath.PackageName, false);
                }
            }

            if (auto UEFNProjects = (UFortGameStateComponent_UEFNProjects*)GameState->GetComponentByClass(UFortGameStateComponent_UEFNProjects::StaticClass()))
            {
                GetLevelsToStream(UEFNProjects, &LevelsToStream);

                for (auto& LevelToStream : LevelsToStream)
                {
                    auto PluginName = GetPluginName(LevelToStream);
                    auto PluginURL = FString((L"file:../../../FortniteGame/Plugins/GameFeatures/" + PluginName + L"/" + PluginName + L".uplugin").c_str());
                    uint8 CompleteDelegate[0x10]{};

                    ChangeGameFeatureTargetState((UGameFeaturesSubsystem*)USubsystemBlueprintLibrary::GetEngineSubsystem(UGameFeaturesSubsystem::StaticClass()), PluginURL, EGameFeatureTargetState::Active, CompleteDelegate);
                    PluginURL.Free();
                }
            }
        }
        else
        {
            GameState->CurrentPlaylistId = GameMode->CurrentPlaylistId = 0;
            GameMode->GameSession->MaxPlayers = 100;
        }

        if (auto MeshNetworkSubsystem = (UMeshNetworkSubsystem*)USubsystemBlueprintLibrary::GetGameInstanceSubsystem(World, UMeshNetworkSubsystem::StaticClass()))
            MeshNetworkSubsystem->NodeType = EMeshNetworkNodeType::Edge;

        if (!GameMode->AIDirector)
        {
            GameMode->AIDirector = SpawnActor<AFortAIDirector>(AAthenaAIDirector::StaticClass(), FVector{}, {}, GameMode);

            if (GameMode->AIDirector)
                GameMode->AIDirector->Activate();
        }

        if (!GameMode->AIGoalManager)
            GameMode->AIGoalManager = SpawnActor<AFortAIGoalManager>(AFortAIGoalManager::StaticClass(), FVector{}, {}, GameMode);

        if (!GameMode->SpawningPolicyManager)
        {
            GameMode->SpawningPolicyManager = SpawnActor<AFortAthenaSpawningPolicyManager>(AFortAthenaSpawningPolicyManager::StaticClass(), FVector{});
            GameMode->SpawningPolicyManager->GameStateAthena = GameState;
            GameMode->SpawningPolicyManager->GameModeAthena = GameMode;
        }

        if (auto PoiManager = GameState->PoiManager; PoiManager && !PoiManager->bHasInitialized)
            InitializePoiManager(PoiManager);

        if (World->NetDriver && GameState->HasBegunPlay())
        {
            TArray<ABuildingActor*> BuildingActors;
            GetAll<ABuildingActor>(BuildingActors);

            for (auto& BuildingActor : BuildingActors)
            {
                if (BuildingActor && !BuildingActor->bAlwaysUseNetCullDistanceSquaredForRelevancy)
                    UpdateCullDistanceServer(BuildingActor);
            }

            BuildingActors.Free();
        }

        *Ret = false;
        return;
    }

    if (!GameMode->bWorldIsReady)
    {
        TArray<APlayerStart*> Starts;
        GetAll<APlayerStart>(Starts);
        auto StartsNum = Starts.Num();
        Starts.Free();

        if (StartsNum == 0 || !bInit)
        {
            *Ret = false;
            return;
        }

        TArray<AFortAthenaMapInfo*> AllMapInfos;
        GetAll<AFortAthenaMapInfo>(AllMapInfos);
        auto MapInfosNum = AllMapInfos.Num();
        AllMapInfos.Free();

        if (!GameState->MapInfo && (MapInfosNum > 0 || LevelsToStream.Num() > 0))
        {
            for (auto& LevelToStream : LevelsToStream)
            {
                if (!IsGameFeaturePluginActive(GetPluginName(LevelToStream)))
                {
                    *Ret = false;
                    return;
                }

                StartStreamingAdditionalPlaylistLevel(GameState, LevelToStream, false);
            }

            InitializeMapInfo(GameMode, GameState);

            if (!GameState->MapInfo)
            {
                *Ret = false;
                return;
            }
        }

        if (GameState->MapInfo && GameState->MapInfo->AircraftDropVolume && !GameState->bPlaylistDataIsLoaded)
        {
            *Ret = false;
            return;
        }

        if (GameState->MapInfo)
        {
            if (auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(GameState))
            {
                InitializeSafeZoneLocations(GamePhaseLogic);
                InitializeFlightPath(GameState->MapInfo, GameState, GamePhaseLogic, false, 0.f, 0.f, 360.f);
            }
        }

        auto Playlist = GameState->CurrentPlaylistInfo.BasePlaylist;

        if (Playlist)
        {
            bSkipWarmup = Playlist->bSkipWarmup;
            bSkipAircraft = Playlist->bSkipAircraft;
        }

        bLateGame = bLateGame && Playlist && (Playlist->RespawnType == EAthenaRespawnType::None || bForceRespawns) && !bSkipAircraft && GameState->MapInfo && LateGameZone >= 1 && GetLateGamePhase(GameState) >= 1 &&
                    GetLateGamePhase(GameState) < (int)GameState->MapInfo->SafeZoneDefinition.Count.Evaluate();

        if (bLateGame && GameState->MapInfo->FlightInfos.Num() > 0)
        {
            auto& FlightInfo = GameState->MapInfo->FlightInfos[0];
            auto BusPhase = GetLateGamePhase(GameState) - 1;
            auto& Center = SafeZoneLocations.Data[BusPhase];
            auto Radius = GameState->MapInfo->SafeZoneDefinition.Radius.Evaluate((float)BusPhase);

            if (bLateGameMovingBus && Radius > 0.f)
            {
                auto Speed = FlightInfo.FlightSpeed > 0.f && Radius / 10.f > FlightInfo.FlightSpeed ? FlightInfo.FlightSpeed : Radius / 10.f;
                auto Yaw = FlightInfo.FlightStartRotation.Yaw;

                (FVector&)FlightInfo.FlightStartLocation = FVector(Center.X - UKismetMathLibrary::DegCos(Yaw) * Speed * 5.f, Center.Y - UKismetMathLibrary::DegSin(Yaw) * Speed * 5.f, 17500.);
                FlightInfo.FlightSpeed = Speed;
                FlightInfo.TimeTillFlightEnd = 10.f;
                FlightInfo.TimeTillDropEnd = 10.f;
            }
            else
            {
                (FVector&)FlightInfo.FlightStartLocation = FVector(Center.X, Center.Y, 17500.);
                FlightInfo.FlightSpeed = 0.f;
                FlightInfo.TimeTillFlightEnd = 7.f;
                FlightInfo.TimeTillDropEnd = 7.f;
            }

            FlightInfo.TimeTillDropStart = 0.f;

            if (GameState->TeamFlightPaths.Num() > 0)
                GameState->TeamFlightPaths[0] = FlightInfo;

            if (GameState->TeamFlightPaths.Num() == 1)
                GameState->FlightPathMidLine = FlightInfo;
        }

        AbilitySets.Add(FindObject<UFortAbilitySet>(L"/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer"));

        auto TacticalSprintAbility = FindObject<UFortAbilitySet>(L"/TacticalSprintGame/Gameplay/AS_TacticalSprint.AS_TacticalSprint");

        if (!TacticalSprintAbility)
            TacticalSprintAbility = FindObject<UFortAbilitySet>(L"/TacticalSprint/Gameplay/AS_TacticalSprint.AS_TacticalSprint");

        AbilitySets.Add(TacticalSprintAbility);
        AbilitySets.Add(FindObject<UFortAbilitySet>(L"/Ascender/Gameplay/Ascender/AS_Ascender.AS_Ascender"));
        AbilitySets.Add(FindObject<UFortAbilitySet>(L"/DoorBashContent/Gameplay/AS_DoorBash.AS_DoorBash"));
        AbilitySets.Add(FindObject<UFortAbilitySet>(L"/HillScramble/Gameplay/AS_HillScramble.AS_HillScramble"));
        AbilitySets.Add(FindObject<UFortAbilitySet>(L"/SlideImpulse/Gameplay/AS_SlideImpulse.AS_SlideImpulse"));

        if (Playlist)
        {
            for (auto& ModifierPtr : Playlist->ModifierList)
            {
                auto Modifier = ModifierPtr.Get();

                if (!Modifier)
                    continue;

                for (auto& DeliveryInfo : Modifier->PersistentAbilitySets)
                {
                    if (!DeliveryInfo.DeliveryRequirements.bApplyToPlayerPawns)
                        continue;

                    for (auto& AbilitySetPtr : DeliveryInfo.AbilitySets)
                        AbilitySets.Add(AbilitySetPtr.Get());
                }
            }
        }

        for (auto& Set : AbilitySets)
        {
            if (Set)
                Set->AddToRoot();
        }

        GameState->DefaultParachuteDeployTraceForGroundDistance = 10000;

        if (auto GrindRailGameData = FindObject<UCurveTable>(L"/GrindRail/DataTables/GrindRailGameData.GrindRailGameData"))
        {
            static FName UseGrindingMME = FName(L"Default.GrindRails.UseGrindingMME");

            for (auto& Pair : GetRowMap(GrindRailGameData))
            {
                if (Pair.Key() != UseGrindingMME)
                    continue;

                auto Row = (FSimpleCurve*)Pair.Value();

                if (!Row)
                    continue;

                for (auto& Key : Row->Keys)
                    Key.Value = 0.f;
            }
        }

        if (GameState->MapInfo)
        {
            auto& VendingMachineRarityCount = GameState->MapInfo->VendingMachineRarityCount;

            for (int i = 0; i < 6; i++)
            {
                float RarityWeight;
                UDataTableFunctionLibrary::EvaluateCurveTableRow(VendingMachineRarityCount.Curve.CurveTable, VendingMachineRarityCount.Curve.RowName, (float)i, nullptr, &RarityWeight, FString());

                VendingRarityWeights[i] = RarityWeight;
                VendingRarityWeightSum += RarityWeight;
            }

            UDataTableFunctionLibrary::EvaluateCurveTableRow(VendingMachineRarityCount.Curve.CurveTable, VendingMachineRarityCount.Curve.RowName, 0.f, nullptr, &VendingRarityZeroWeight, FString());

            VendingTotalWeight = 0.0f;

            for (auto& [Rarity, RarityWeight] : VendingRarityWeights)
                VendingTotalWeight += RarityWeight;
        }

        GameMode->DefaultPawnClass = FindObject<UClass>(L"/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");

        for (auto& Pair : GameState->AllPlayerBuildableClassesIndexLookup)
            BuildingClassMap[Pair.Value()] = (UClass*)Pair.Key();

        bReady = true;
        SetConsoleTitleA("Remix 32.11 | Joinable");
        GameMode->bWorldIsReady = true;
    }

    int ReadyPlayers = 0;
    TArray<AFortPlayerControllerAthena*> PlayerList;
    GetAll<AFortPlayerControllerAthena>(PlayerList);

    for (auto& PlayerController : PlayerList)
    {
        if (!PlayerController->PlayerState->bIsSpectator && PlayerController->bReadyToStartMatch)
            ReadyPlayers++;
    }

    PlayerList.Free();

    auto VolumeManager = GameState->VolumeManager;

    bool bAllLevelsFinishedStreaming = true;
    auto& AdditionalPlaylistLevels = *(TArray<FPlaylistStreamedLevelData>*)(__int64(GameState) + 0x430);

    for (auto& AdditionalPlaylistLevel : AdditionalPlaylistLevels)
    {
        auto StreamingLevel = *(ULevelStreaming**)&AdditionalPlaylistLevel;

        if (!AdditionalPlaylistLevel.bIsFinishedStreaming || !StreamingLevel || !StreamingLevel->LoadedLevel->bIsVisible)
        {
            bAllLevelsFinishedStreaming = false;
            break;
        }
    }

    static auto WaitingToStart = FName(L"WaitingToStart");
    *Ret = GameMode->bWorldIsReady && GameState->bPlaylistDataIsLoaded && GameMode->MatchState == WaitingToStart && bAllLevelsFinishedStreaming && (!VolumeManager || !GameState->bInSpawningStartup) &&
           ReadyPlayers >= GameMode->WarmupRequiredPlayerCount && (bNavCacheAttached || bNavCacheDone);
}

static void SpawnDefaultPawnFor(UObject* Context, FFrame& Stack, APawn** Ret)
{
    AController* NewPlayer;
    AActor* StartSpot;
    Stack.StepCompiledIn(&NewPlayer);
    Stack.StepCompiledIn(&StartSpot);
    Stack.IncrementCode();

    auto GameMode = (AFortGameModeAthena*)Context;

    if (!NewPlayer || (!NewPlayer->IsA(AFortPlayerControllerAthena::StaticClass()) && (!NewPlayer->PlayerState || !NewPlayer->PlayerState->bIsABot)) || !StartSpot)
        return;

    auto PawnClass = GameMode->GetDefaultPawnClassForController(NewPlayer);

    if (!PawnClass)
        return;

    auto Pawn = (APawn*)SpawnActor(PawnClass, StartSpot->GetTransform(), NewPlayer, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);

    while (!Pawn)
    {
        auto PlayerStart = GameMode->ChoosePlayerStart(NewPlayer);

        if (PlayerStart)
            Pawn = (APawn*)SpawnActor(PawnClass, PlayerStart->GetTransform(), NewPlayer, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
    }

    *Ret = Pawn;

    if (!NewPlayer->IsA(AFortPlayerControllerAthena::StaticClass()))
        return;

    auto PlayerController = (AFortPlayerControllerAthena*)NewPlayer;
    auto WorldInventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;
    auto Num = WorldInventory ? WorldInventory->Inventory.ReplicatedEntries.Num() : 0;

    if (Num == 0)
    {
        auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;

        RegisterWithQuestManager(PlayerController);

        PlayerState->SeasonLevelUIDisplay = PlayerController->XPComponent->CurrentUILevel;
        PlayerState->OnRep_SeasonLevelUIDisplay();
    }
}

static void (*HandleStartingNewPlayerOG)(UObject*, FFrame&, void*);
static void HandleStartingNewPlayer(UObject* Context, FFrame& Stack, void* Ret)
{
    static auto CopyGameMemberInfo = (FGameMemberInfo* (*)(FGameMemberInfo*, const FGameMemberInfo*))(ImageBase + 0x34B112C);
    static auto NotifyGameMemberAdded = (void (*)(AFortGameStateAthena*, uint8, uint8, FUniqueNetIdRepl*))(ImageBase + 0x27E12C0);

    APlayerController* NewPlayer;
    Stack.StepCompiledIn(&NewPlayer);
    Stack.IncrementCode();

    auto GameMode = (AFortGameModeAthena*)Context;
    auto GameState = (AFortGameStateAthena*)GameMode->GameState;
    auto PlayerController = (AFortPlayerControllerAthena*)NewPlayer;
    auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;

    PlayerState->OnRep_SquadId();

    FGameMemberInfo Member{};

    Member.MostRecentArrayReplicationKey = -1;
    Member.ReplicationID = -1;
    Member.ReplicationKey = -1;
    Member.TeamIndex = PlayerState->TeamIndex;
    Member.SquadID = PlayerState->SquadID;
    Member.MemberUniqueId = PlayerState->UniqueID;

    auto& Members = GameState->GameMemberInfoArray.Members;

    Members.Reserve(1);

    auto& NewMember = Members.Data[Members.NumElements++];

    CopyGameMemberInfo(&NewMember, &Member);
    GameState->GameMemberInfoArray.MarkItemDirty(NewMember);
    NotifyGameMemberAdded(GameState, NewMember.SquadID, NewMember.TeamIndex, &NewMember.MemberUniqueId);

    if (!PlayerController->WorldInventory.ObjectPointer)
    {
        auto WorldInventory = SpawnActor<AFortInventory>(PlayerController->WorldInventoryClass.Get(), FVector{}, FRotator{}, PlayerController);

        PlayerController->WorldInventory.ObjectPointer = WorldInventory;
        PlayerController->WorldInventory.InterfacePointer = WorldInventory->GetInterface(IFortInventoryInterface::StaticClass());
        WorldInventory->InventoryType = EFortInventoryType::World;
    }

    PlayerState->WorldPlayerId = 0;

    callOG(GameMode, Stack.CurrentNativeFunction, HandleStartingNewPlayer, NewPlayer);

    TournamentPlayerJoined(GameMode, PlayerState);

    for (auto& OtherPlayerState : GameState->PlayerArray)
    {
        auto SquadPlayerState = (AFortPlayerStateAthena*)OtherPlayerState;

        if (SquadPlayerState && SquadPlayerState->IsA(AFortPlayerStateAthena::StaticClass()) && SquadPlayerState->SquadID == PlayerState->SquadID && SquadPlayerState->Owner && SquadPlayerState->Owner->IsA(AFortPlayerControllerZone::StaticClass()))
            SendVoiceChatChannelJoin(GameMode, (AFortPlayerControllerZone*)SquadPlayerState->Owner);
    }
}

static void (*HandleMatchHasStartedOG)(AFortGameModeAthena*);
static void HandleMatchHasStarted(AFortGameModeAthena* GameMode)
{
    HandleMatchHasStartedOG(GameMode);
    auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(GameMode);

    if (!bSkipWarmup)
    {
        auto Time = (float)UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());

        GamePhaseLogic->WarmupCountdownStartTime = Time;
        GamePhaseLogic->WarmupCountdownEndTime = Time + GamePhaseLogic->WarmupCountdownDuration;

        SetGamePhase(GamePhaseLogic, EAthenaGamePhase::Warmup);
        SetGamePhaseStep(GamePhaseLogic, EAthenaGamePhaseStep::Warmup);
    }
    else
        StartAircraftPhase(GamePhaseLogic);
}

static void AddLootTierData(UDataTable* Table)
{
    if (!Table)
        return;

    Table->AddToRoot();

    for (auto& Pair : Table->RowMap)
    {
        auto TierData = (FFortLootTierData*)Pair.Value();

        TierDataMap[TierData->TierGroup.ComparisonIndex].Add(TierData);
    }
}

static void AddLootPackageData(UDataTable* Table)
{
    if (!Table)
        return;

    Table->AddToRoot();

    std::unordered_map<int32, FFortLootPackageData*> PackagesByRowName;

    for (auto& Pair : Table->RowMap)
        PackagesByRowName[Pair.Key().ComparisonIndex] = (FFortLootPackageData*)Pair.Value();

    for (auto& [_, Package] : PackagesByRowName)
        LootPackageMap[Package->LootPackageID.ComparisonIndex].Add(Package);
}

static void (*FinishWorldInitializationOG)(AFortGameModeAthena*, AActor*);
static void FinishWorldInitialization(AFortGameModeAthena* GameMode, AActor* WorldManager)
{
    static auto GetGameFeatureDataForActivePlugin = (UGameFeatureData* (*)(UGameFeaturePluginStateMachine*))(ImageBase + 0x1862848);

    FinishWorldInitializationOG(GameMode, WorldManager);

    auto Playlist = FindPlaylist();
    auto LootTierData = Playlist ? Playlist->LootTierData.Get() : nullptr;

    if (!LootTierData)
        LootTierData = FindObject<UDataTable>(L"/Game/Items/Datatables/AthenaLootTierData_Client.AthenaLootTierData_Client");

    AddLootTierData(LootTierData);

    auto LootPackages = Playlist ? Playlist->LootPackages.Get() : nullptr;

    if (!LootPackages)
        LootPackages = FindObject<UDataTable>(L"/Game/Items/Datatables/AthenaLootPackages_Client.AthenaLootPackages_Client");

    AddLootPackageData(LootPackages);

    if (GameMode->GameFeatureLootTierData.Num() > 0)
    {
        for (auto& FeatureLootTierData : GameMode->GameFeatureLootTierData)
            AddLootTierData(FeatureLootTierData.Get());

        for (auto& FeatureLootPackageData : GameMode->GameFeatureLootPackageData)
            AddLootPackageData(FeatureLootPackageData.Get());
    }
    else if (auto GameFeaturesSubsystem = (UGameFeaturesSubsystem*)USubsystemBlueprintLibrary::GetEngineSubsystem(UGameFeaturesSubsystem::StaticClass()))
    {
        for (auto& Pair : GameFeaturesSubsystem->GameFeaturePluginStateMachines)
        {
            auto StateMachine = Pair.Value();
            auto GameFeatureData = StateMachine ? (UFortGameFeatureData*)GetGameFeatureDataForActivePlugin(StateMachine) : nullptr;

            if (!GameFeatureData || !GameFeatureData->IsA(UFortGameFeatureData::StaticClass()))
                continue;

            auto LootTableData = &GameFeatureData->DefaultLootTableData;

            if (Playlist)
            {
                for (auto& OverridePair : GameFeatureData->PlaylistOverrideLootTableData)
                {
                    if (Playlist->GameplayTagContainer.HasTag(OverridePair.Key()))
                    {
                        LootTableData = &OverridePair.Value();
                        break;
                    }
                }
            }

            auto FeatureLootTierData = LootTableData->LootTierData.Get();
            auto FeatureLootPackageData = LootTableData->LootPackageData.Get();

            AddLootTierData(FeatureLootTierData);
            AddLootPackageData(FeatureLootPackageData);
        }
    }
    else
    {
        for (int i = 0; i < UObject::GObjects->Num(); i++)
        {
            auto Object = UObject::GObjects->GetByIndex(i);

            if (!Object || !Object->Class || Object->IsDefaultObject() || !Object->IsA(UFortGameFeatureData::StaticClass()))
                continue;

            auto& LootTableData = ((UFortGameFeatureData*)Object)->DefaultLootTableData;

            AddLootTierData(LootTableData.LootTierData.Get());
            AddLootPackageData(LootTableData.LootPackageData.Get());
        }
    }

    *(bool*)(__int64(&GameMode->OnPlaylistLootTablesAppliedDelegate) + 0x10) = true;
    GameMode->OnPlaylistLootTablesAppliedDelegate.Process();

    SpawnFloorLootForContainer(FindObject<UClass>(L"/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_Warmup.Tiered_Athena_FloorLoot_Warmup_C"));
    SpawnFloorLootForContainer(FindObject<UClass>(L"/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C"));

    TArray<ABGAConsumableSpawner*> ConsumableSpawners{};
    GetAll<ABGAConsumableSpawner>(ConsumableSpawners);

    for (auto& Spawner : ConsumableSpawners)
        WorldReady(Spawner);

    ConsumableSpawners.Free();

    TArray<ABuildingItemCollectorActor*> Collectors{};
    GetAll<ABuildingItemCollectorActor>(Collectors);

    for (auto& CollectorActor : Collectors)
        FillVendingMachine(CollectorActor);

    Collectors.Free();

    auto VendWobbleFinishedFunc = FindObject<UFunction>(L"/Game/Athena/Items/Gameplay/VendingMachine/B_Athena_VendingMachine.B_Athena_VendingMachine_C:VendWobble__FinishedFunc");

    if (VendWobbleFinishedFunc && VendWobbleFinishedFunc->ExecFunction != (UFunction::FNativeFuncPtr)VendWobble__FinishedFunc)
        ExecHook(VendWobbleFinishedFunc, VendWobble__FinishedFunc, VendWobble__FinishedFuncOG);
}

void SpawnFloorLootForContainer(const UClass* ContainerType)
{
    TArray<ABuildingContainer*> Containers;
    GetAll<ABuildingContainer>(ContainerType, Containers);

    for (auto& BuildingContainer : Containers)
        BuildingContainer->K2_DestroyActor();

    Containers.Free();
}

static void (*OnPawnDeathNoRespawnOG)(AFortGameModeAthena*, AFortPlayerPawnAthena*);
static void OnPawnDeathNoRespawn(AFortGameModeAthena* GameMode, AFortPlayerPawnAthena* Pawn)
{
    static auto RemoveFromAlivePlayers = (void (*)(AFortGameModeAthena*, AFortPlayerControllerAthena*, AFortPlayerStateAthena*, AFortPlayerPawnAthena*, UFortItemDefinition*, EDeathCause, char))(ImageBase + 0x918B8D4);

    OnPawnDeathNoRespawnOG(GameMode, Pawn);

    auto PlayerController = Pawn->Controller && Pawn->Controller->IsA(AFortPlayerControllerAthena::StaticClass()) ? (AFortPlayerControllerAthena*)Pawn->Controller : nullptr;

    if (!PlayerController)
        return;

    auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;
    auto& DeathReport = PlayerController->PlayerDeathReport;
    auto KillerPlayerState = (AFortPlayerStateAthena*)DeathReport.KillerPlayerState;
    auto KillerPawn = (AFortPlayerPawnAthena*)DeathReport.KillerPawn.Get();
    auto KillerWeapon = GetKillerWeapon(DeathReport.DamageCauser.Get());
    auto DeathCause = PlayerState->DeathInfo.DeathCause;

    RemoveFromAlivePlayers(GameMode, PlayerController, KillerPlayerState == PlayerState ? nullptr : KillerPlayerState, KillerPawn, KillerWeapon, DeathCause, 0);

    Pawn->CharacterMovement->DisableMovement();

    ReportElimination(GameMode, PlayerState, KillerPlayerState, KillerPawn, KillerWeapon, DeathCause);
}

static void (*HandlePawnKilledOG)(AGameModeBase*, AActor*, AController*, AFortPawn*, FGameplayTagContainer*);
static void HandlePawnKilled(AGameModeBase* GameMode, AActor* Killer, AController* KilledController, AFortPawn* KilledPawn, FGameplayTagContainer* DeathTags)
{
    HandlePawnKilledOG(GameMode, Killer, KilledController, KilledPawn, DeathTags);

    SendRequestedMatchReports(-1.0);
}

void AFortGameModeAthena__Init()
{
    ConfiguredPlaylist = Playlist;
    bConfiguredLateGame = bLateGame;

    Hook(ImageBase + 0x9184E1C, HandleMatchHasStarted, HandleMatchHasStartedOG);
    ExecHook(AGameMode::StaticClass()->FindFunction("ReadyToStartMatch"), ReadyToStartMatch, ReadyToStartMatchOG);
    Patch<uint16_t>(ImageBase + 0x1AC2464, 0x9090);
    Hook(ImageBase + 0x918177C, FinishWorldInitialization, FinishWorldInitializationOG);

    for (int i = 0; i < 6; i++)
        Patch<uint8_t>(ImageBase + 0x21CB85D + i, 0x90);
}

void AFortGameModeAthena__PostLoad()
{
    ExecHook(AGameModeBase::StaticClass()->FindFunction("SpawnDefaultPawnFor"), SpawnDefaultPawnFor);
    Patch<uint16_t>(ImageBase + 0x6EDBF44, 0x9090);
    ExecHook(AGameModeBase::StaticClass()->FindFunction("HandleStartingNewPlayer"), HandleStartingNewPlayer, HandleStartingNewPlayerOG);
    Patch<uint16_t>(ImageBase + 0x1BED710, 0x9090);

    Hook(ImageBase + 0x91886F8, OnPawnDeathNoRespawn, OnPawnDeathNoRespawnOG);

    Hook(ImageBase + 0x9186CF8, HandlePawnKilled, HandlePawnKilledOG);
}
