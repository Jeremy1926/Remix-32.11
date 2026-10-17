#include "pch.h"
#include "Shared.h"
#include "Config.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/GameStateComponents/FortGameStateComponent_BattleRoyaleGamePhaseLogic.h"
#include "FortniteGame/Source/FortniteAI/Public/Athena/AI/Systems/AthenaAIServicePlayerBots.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/ControllerComponents/FortControllerComponent_TournamentStatsTracker.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortGameModeAthena.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortPlayerControllerAthena.h"
#include "FortniteGame/Source/FortniteGame/Public/Quests/FortQuestManager.h"

INIT_MODULE(UFortGameStateComponent_BattleRoyaleGamePhaseLogic);
INIT_TICKER(UFortGameStateComponent_BattleRoyaleGamePhaseLogic);

TArray<FVector> SafeZoneLocations;

void SetGamePhase(UFortGameStateComponent_BattleRoyaleGamePhaseLogic* GamePhaseLogic, EAthenaGamePhase GamePhase)
{
    static auto SetGamePhaseInternal = (void (*)(UFortGameStateComponent_BattleRoyaleGamePhaseLogic*, EAthenaGamePhase))(ImageBase + 0xA6EFD34);

    SetGamePhaseInternal(GamePhaseLogic, GamePhase);
}

void SetGamePhaseStep(UFortGameStateComponent_BattleRoyaleGamePhaseLogic* GamePhaseLogic, EAthenaGamePhaseStep GamePhaseStep)
{
    static auto BroadcastGamePhaseStepUpdatedEvent = (void (*)(const UClass*, UObject*, EAthenaGamePhaseStep*, UObject*))(ImageBase + 0x29D2A04);

    auto OldGamePhaseStep = GamePhaseLogic->GamePhaseStep;

    GamePhaseLogic->GamePhaseStep = GamePhaseStep;
    GamePhaseLogic->HandleGamePhaseStepChanged(GamePhaseStep);

    if (OldGamePhaseStep != GamePhaseStep)
        BroadcastGamePhaseStepUpdatedEvent(AFortGameState::StaticClass(), GamePhaseLogic->GetOwner(), &GamePhaseStep, GamePhaseLogic);
}

static void SetSafeZoneState(AFortSafeZoneIndicator* SafeZoneIndicator, EFortSafeZoneState SafeZoneState)
{
    static auto SetSafeZoneStateInternal = (void (*)(AFortSafeZoneIndicator*, EFortSafeZoneState))(ImageBase + 0x29D24C8);

    SetSafeZoneStateInternal(SafeZoneIndicator, SafeZoneState);
}

void StartAircraftPhase(UFortGameStateComponent_BattleRoyaleGamePhaseLogic* GamePhaseLogic)
{
    static auto Reset = (void (*)(AFortPlayerControllerAthena*))(ImageBase + 0x7113FD8);

    if (GamePhaseLogic->GamePhase >= EAthenaGamePhase::Aircraft)
        return;

    auto Time = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());

    auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
    auto Playlist = (UFortPlaylistAthena*)((AFortGameStateAthena*)GameMode->GameState)->CurrentPlaylistInfo.BasePlaylist;

    SetConsoleTitleA("Remix 32.11 | Match started");

    if (Playlist && Playlist->bAllowJoinInProgress)
        *(bool*)(__int64(GameMode) + 0x1018) = false;

    if (bSkipAircraft)
    {
        SetGamePhase(GamePhaseLogic, EAthenaGamePhase::SafeZones);
        SetGamePhaseStep(GamePhaseLogic, EAthenaGamePhaseStep::StormForming);
        TournamentMatchStarted(GameMode);

        return;
    }

    auto GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;

    if (GameState->MapInfo->FlightInfos.Num() > 0)
    {
        auto& FlightInfo = GameState->MapInfo->FlightInfos[0];

        if (bLateGame)
            GameState->DefaultParachuteDeployTraceForGroundDistance = 2500.f;

        if (!GameState->MapInfo->AircraftClass.Get())
            return;

        auto Aircraft = AFortAthenaAircraft::SpawnAircraft(UWorld::GetWorld(), GameState->MapInfo->AircraftClass, FlightInfo, nullptr);

        if (!Aircraft)
            return;

        Aircraft->FlightElapsedTime = 0;
        Aircraft->DropStartTime = (float)Time + FlightInfo.TimeTillDropStart;
        Aircraft->DropEndTime = (float)Time + FlightInfo.TimeTillDropEnd;
        Aircraft->FlightStartTime = (float)Time;
        Aircraft->FlightEndTime = (float)Time + FlightInfo.TimeTillFlightEnd;
        Aircraft->ReplicatedFlightTimestamp = (float)Time;
        GamePhaseLogic->bAircraftIsLocked = true;

        for (auto& Player : GameMode->AlivePlayers)
        {
            auto Pawn = (AFortPlayerPawnAthena*)Player->Pawn;

            if (Pawn)
            {
                if (Pawn->Role == ENetRole::ROLE_Authority && Pawn->bIsInAnyStorm)
                {
                    Pawn->bIsInAnyStorm = false;
                    Pawn->OnRep_IsInAnyStorm();
                }

                Pawn->bIsInsideSafeZone = true;
                Pawn->OnRep_IsInsideSafeZone();
                Pawn->OnEnteredAircraft.Process();
            }

            Player->ClientActivateSlot(EFortQuickBars(0), 0, 0.f, true, true);

            if (Pawn)
                Pawn->K2_DestroyActor();

            Reset(Player);
            Player->ClientGotoState(FName(L"Spectating"));
        }

        GamePhaseLogic->Aircrafts_GameMode.Add(TWeakObjectPtr<AFortAthenaAircraft>(Aircraft));
        GamePhaseLogic->Aircrafts_GameState.Add(TWeakObjectPtr<AFortAthenaAircraft>(Aircraft));
    }

    SetGamePhase(GamePhaseLogic, EAthenaGamePhase::Aircraft);
    SetGamePhaseStep(GamePhaseLogic, EAthenaGamePhaseStep::BusLocked);
    TournamentMatchStarted(GameMode);
}

static void StartNewSafeZonePhase(UFortGameStateComponent_BattleRoyaleGamePhaseLogic* GamePhaseLogic, int NewSafeZonePhase)
{
    static auto UpdateGamePhaseStep = (void (*)(UFortGameStateComponent_BattleRoyaleGamePhaseLogic*, float))(ImageBase + 0x1E34990);

    float TimeSeconds = (float)UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
    auto SafeZoneIndicator = GamePhaseLogic->SafeZoneIndicator;
    auto& SafeZonePhases = SafeZoneIndicator->SafeZonePhases;

    if (!SafeZonePhases.IsValidIndex(NewSafeZonePhase))
        return;

    if (SafeZonePhases.IsValidIndex(NewSafeZonePhase - 1))
    {
        auto& PreviousPhaseInfo = SafeZonePhases[NewSafeZonePhase - 1];

        (FVector&)SafeZoneIndicator->PreviousCenter = PreviousPhaseInfo.Center;
        SafeZoneIndicator->PreviousRadius = PreviousPhaseInfo.Radius;
    }

    auto& PhaseInfo = SafeZonePhases[NewSafeZonePhase];

    (FVector&)SafeZoneIndicator->NextCenter = PhaseInfo.Center;
    SafeZoneIndicator->NextRadius = PhaseInfo.Radius;
    SafeZoneIndicator->NextMegaStormGridCellThickness = PhaseInfo.MegaStormGridCellThickness;

    if (SafeZonePhases.IsValidIndex(NewSafeZonePhase + 1))
    {
        auto& NextPhaseInfo = SafeZonePhases[NewSafeZonePhase + 1];

        if (SafeZoneIndicator->FutureReplicator)
        {
            (FVector&)SafeZoneIndicator->FutureReplicator->NextNextCenter = NextPhaseInfo.Center;
            SafeZoneIndicator->FutureReplicator->NextNextRadius = NextPhaseInfo.Radius;
        }

        (FVector&)SafeZoneIndicator->NextNextCenter = NextPhaseInfo.Center;
        SafeZoneIndicator->NextNextRadius = NextPhaseInfo.Radius;
        SafeZoneIndicator->NextNextMegaStormGridCellThickness = NextPhaseInfo.MegaStormGridCellThickness;
    }

    SafeZoneIndicator->SafeZoneStartShrinkTime = bLateGame && bLateGameLongZone ? 676767.f : TimeSeconds + PhaseInfo.WaitTime;
    SafeZoneIndicator->SafeZoneFinishShrinkTime = SafeZoneIndicator->SafeZoneStartShrinkTime + PhaseInfo.ShrinkTime;

    SafeZoneIndicator->CurrentDamageInfo = PhaseInfo.DamageInfo;
    SafeZoneIndicator->OnRep_CurrentDamageInfo();

    auto OldPhase = SafeZoneIndicator->CurrentPhase;
    SafeZoneIndicator->CurrentPhase = NewSafeZonePhase;
    SafeZoneIndicator->OnRep_CurrentPhase();

    SafeZoneIndicator->OnSafeZonePhaseChanged.Process();

    bool bInitial = OldPhase <= 0;

    SetSafeZoneState(SafeZoneIndicator, EFortSafeZoneState::Holding);
    UpdateGamePhaseStep(GamePhaseLogic, 0.f);

    auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;

    if (!bInitial)
    {
        for (auto& PlayerController : GameMode->AlivePlayers)
            SendStatEvent(PlayerController->GetQuestManager(ESubGame::Athena), PlayerController, EFortQuestObjectiveStatEvent::StormPhase, 1, true);
    }
}

void InitializeSafeZoneLocations(UFortGameStateComponent_BattleRoyaleGamePhaseLogic* GamePhaseLogic)
{
    static auto SetSafeZoneLocations = (void (*)(UFortGameStateComponent_BattleRoyaleGamePhaseLogic*, TArray<FVector>*))(ImageBase + 0xA6F01CC);

    auto GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
    auto Playlist = (UFortPlaylistAthena*)GameState->CurrentPlaylistInfo.BasePlaylist;

    auto SafeZoneBlacklist = Playlist->SafeZoneLocationBlacklist.Get();

    if (!SafeZoneBlacklist)
        SafeZoneBlacklist = FindObject<UCurveTable>(L"/Game/Athena/Balance/DataTables/AthenaSafeZoneBlacklist.AthenaSafeZoneBlacklist");

    TArray<FVector4> BlacklistLocations;

    for (auto& Pair : GetRowMap(SafeZoneBlacklist))
    {
        auto Row = (FSimpleCurve*)Pair.Value();

        if (!Row)
            continue;

        FVector4 Loc{};

        for (auto& Key : Row->Keys)
        {
            if (Key.time == 0.f)
                Loc.X = Key.Value;
            else if (Key.time == 1.f)
                Loc.Y = Key.Value;
            else if (Key.time == 2.f)
                Loc.Z = Key.Value;
            else if (Key.time == 3.f)
                Loc.W = Key.Value;
        }

        if (Loc.X == 0 && Loc.Y == 0 && Loc.Z == 0 && Loc.W == 0)
            continue;

        BlacklistLocations.Add(Loc);
    }

    auto ZeroVector = FVector(0, 0, 0);
    auto SafeZoneCount = (float)Playlist->LastSafeZoneIndex;

    if (SafeZoneCount == -1)
        SafeZoneCount = GameState->MapInfo->SafeZoneDefinition.Count.Evaluate(0.f);
    else
        SafeZoneCount++;

    auto Center = GameState->MapInfo->GetMapCenter();

    SafeZoneLocations.Clear();
    SafeZoneLocations.Reserve((int)SafeZoneCount);

    for (int i = (int)(SafeZoneCount - 1); i >= 0; i--)
    {
        auto Params = GameState->MapInfo->ConstructSafeZoneLocationParams(i, Center, i == SafeZoneCount - 1 ? ZeroVector : SafeZoneLocations.Data[i + 1], i == SafeZoneCount - 1, 0);

        auto Location = GameState->MapInfo->PickSafeZoneLocation(Params, BlacklistLocations);

        SafeZoneLocations.Data[i] = Location;
    }

    auto Count = (int32)SafeZoneCount;

    if (Count <= 0)
        return;

    TArray<FVector> Locations;

    for (int32 i = 0; i < Count; i++)
        Locations.Add(SafeZoneLocations.Data[i]);

    SetSafeZoneLocations(GamePhaseLogic, &Locations);
    Locations.Free();
}

static bool bFinishedFlight = false;
static bool bGettingReady = false;
static bool bBusUnlocked = false;
static bool bStartedForming = false;
static bool bFormedZone = false;
static bool bSafeZoneShrinking = false;

static AFortAthenaAircraft* GetActiveAircraft(AFortGameModeAthena* GameMode, UFortGameStateComponent_BattleRoyaleGamePhaseLogic* GamePhaseLogic)
{
    if (GameMode->AlivePlayers.Num() <= 0 || GamePhaseLogic->Aircrafts_GameState.Num() <= 0)
        return nullptr;

    return GamePhaseLogic->Aircrafts_GameState.Data[0].Get();
}

static void UpdatePawnStormState(UFortGameStateComponent_BattleRoyaleGamePhaseLogic* GamePhaseLogic, AFortPlayerPawn* Pawn)
{
    bool bInZone = GamePhaseLogic->IsInCurrentSafeZone(Pawn->K2_GetActorLocation(), false);

    if (Pawn->bIsInsideSafeZone != bInZone || Pawn->bIsInAnyStorm != !bInZone)
    {
        Pawn->bIsInAnyStorm = !bInZone;
        Pawn->OnRep_IsInAnyStorm();
        Pawn->bIsInsideSafeZone = bInZone;
        Pawn->OnRep_IsInsideSafeZone();
    }
}

static void (*PlacePlayersInAircraftOG)(UFortGameStateComponent_BattleRoyaleGamePhaseLogic*);
static void PlacePlayersInAircraft(UFortGameStateComponent_BattleRoyaleGamePhaseLogic* GamePhaseLogic)
{
    static auto GetService = (UAthenaAIServicePlayerBots* (*)(UWorld*))(ImageBase + 0x8F35808);

    PlacePlayersInAircraftOG(GamePhaseLogic);

    if (auto Service = GetService(UWorld::GetWorld()))
    {
        FinishPendingRespawns(Service);
        UnspawnBotFromWarmup(Service);
        PlacePlayerBotsInAircraft(Service, GamePhaseLogic);
    }
}

void UFortGameStateComponent_BattleRoyaleGamePhaseLogic__Init()
{
    Hook(ImageBase + 0x348143C, PlacePlayersInAircraft, PlacePlayersInAircraftOG);
}

void UFortGameStateComponent_BattleRoyaleGamePhaseLogic__Tick()
{
    static auto GetPlayerBotsService = (UAthenaAIServicePlayerBots* (*)(UWorld*))(ImageBase + 0x8F35808);

    auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(UWorld::GetWorld());

    if (!GamePhaseLogic)
        return;

    auto Time = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
    auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;

    if (!bSkipAircraft)
    {
        if (GamePhaseLogic->GamePhase <= EAthenaGamePhase::Warmup && !PauseWarmupMask)
        {
            if (!bGettingReady && GamePhaseLogic->GamePhase == EAthenaGamePhase::Warmup && GameMode->AlivePlayers.Num() > 0 && GamePhaseLogic->WarmupCountdownEndTime != -1 && GamePhaseLogic->WarmupCountdownEndTime - 10.f <= Time)
            {
                bGettingReady = true;

                SetGamePhaseStep(GamePhaseLogic, EAthenaGamePhaseStep::GetReady);
                return;
            }

            if (bGettingReady && GameMode->AlivePlayers.Num() > 0 && GamePhaseLogic->WarmupCountdownEndTime != -1 && GamePhaseLogic->WarmupCountdownEndTime < Time)
            {
                StartAircraftPhase(GamePhaseLogic);
                return;
            }
        }

        if (GamePhaseLogic->GamePhase == EAthenaGamePhase::Aircraft)
        {
            if (!bBusUnlocked)
            {
                auto Aircraft = GetActiveAircraft(GameMode, GamePhaseLogic);

                if (Aircraft && Aircraft->DropStartTime < Time)
                {
                    bBusUnlocked = true;

                    GamePhaseLogic->bAircraftIsLocked = false;
                    SetGamePhaseStep(GamePhaseLogic, EAthenaGamePhaseStep::BusFlying);
                    return;
                }
            }

            if (!bStartedForming)
            {
                auto Aircraft = GetActiveAircraft(GameMode, GamePhaseLogic);

                if (Aircraft && Aircraft->DropEndTime != -1 && Aircraft->DropEndTime < Time)
                {
                    bStartedForming = true;

                    for (auto& Player : GameMode->AlivePlayers)
                    {
                        if (Player->IsInAircraft())
                            Player->GetAircraftComponent()->ServerAttemptAircraftJump(FRotator{});
                    }

                    if (auto PlayerBotsService = GetPlayerBotsService(UWorld::GetWorld()))
                        ForceAllExitAircraft(PlayerBotsService);

                    GamePhaseLogic->SafeZonesStartTime = bLateGame ? (float)Time : (float)Time + 60.f;

                    SetGamePhase(GamePhaseLogic, EAthenaGamePhase::SafeZones);
                    SetGamePhaseStep(GamePhaseLogic, EAthenaGamePhaseStep::StormForming);
                    return;
                }
            }
        }

        if (!bFinishedFlight)
        {
            auto Aircraft = GetActiveAircraft(GameMode, GamePhaseLogic);

            if (Aircraft && Aircraft->FlightEndTime != -1 && Aircraft->FlightEndTime < Time)
            {
                bFinishedFlight = true;

                Aircraft->K2_DestroyActor();
                GamePhaseLogic->Aircrafts_GameState.Clear();
                GamePhaseLogic->Aircrafts_GameMode.Clear();
                return;
            }
        }
    }
    else if (!bFinishedFlight)
    {
        bFinishedFlight = true;

        SetGamePhase(GamePhaseLogic, EAthenaGamePhase::SafeZones);
        SetGamePhaseStep(GamePhaseLogic, EAthenaGamePhaseStep::StormForming);
    }

    if (!bEnableZones || GamePhaseLogic->GamePhase != EAthenaGamePhase::SafeZones)
        return;

    if (bFinishedFlight && !bFormedZone && GameMode->AlivePlayers.Num() > 0 && GamePhaseLogic->SafeZonesStartTime != -1 && GamePhaseLogic->SafeZonesStartTime < Time)
    {
        bFormedZone = true;

        if (!GamePhaseLogic->SafeZoneIndicator)
        {
            auto SafeZoneIndicator = SpawnActor<AFortSafeZoneIndicator>(GamePhaseLogic->SafeZoneIndicatorClass.Get(), FVector{});

            if (SafeZoneIndicator)
            {
                auto GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
                auto& SafeZoneDefinition = GameState->MapInfo->SafeZoneDefinition;
                float SafeZoneCount = SafeZoneDefinition.Count.Evaluate();

                auto& SafeZonePhases = SafeZoneIndicator->SafeZonePhases;

                if (SafeZonePhases.IsValid())
                    SafeZonePhases.Free();

                const float TimeSeconds = (float)UGameplayStatics::GetTimeSeconds(GameState);

                for (float i = 0; i < SafeZoneCount; i++)
                {
                    FFortSafeZonePhaseInfo PhaseInfo{};

                    PhaseInfo.Radius = SafeZoneDefinition.Radius.Evaluate(i);
                    PhaseInfo.WaitTime = SafeZoneDefinition.WaitTime.Evaluate(i);
                    PhaseInfo.ShrinkTime = SafeZoneDefinition.ShrinkTime.Evaluate(i);
                    PhaseInfo.PlayerCap = (int)SafeZoneDefinition.PlayerCapSolo.Evaluate(i);

                    UDataTableFunctionLibrary::EvaluateCurveTableRow(GameState->AthenaGameDataTable, FName(L"Default.SafeZone.Damage"), i, nullptr, &PhaseInfo.DamageInfo.Damage, FString());

                    if (i == 0.f)
                        PhaseInfo.DamageInfo.Damage = 0.01f;

                    PhaseInfo.DamageInfo.bPercentageBasedDamage = true;
                    PhaseInfo.TimeBetweenStormCapDamage = GamePhaseLogic->TimeBetweenStormCapDamage.Evaluate(i);
                    PhaseInfo.StormCapDamagePerTick = GamePhaseLogic->StormCapDamagePerTick.Evaluate(i);
                    PhaseInfo.StormCampingIncrementTimeAfterDelay = GamePhaseLogic->StormCampingIncrementTimeAfterDelay.Evaluate(i);
                    PhaseInfo.StormCampingInitialDelayTime = GamePhaseLogic->StormCampingInitialDelayTime.Evaluate(i);
                    PhaseInfo.MegaStormGridCellThickness = (int)SafeZoneDefinition.MegaStormGridCellThickness.Evaluate(i);
                    PhaseInfo.UsePOIStormCenter = false;

                    PhaseInfo.Center = SafeZoneLocations.Data[(int)i];

                    SafeZonePhases.Add(PhaseInfo);

                    SafeZoneIndicator->PhaseCount++;
                }

                SafeZoneIndicator->OnRep_PhaseCount();

                SafeZoneIndicator->SafeZoneStartShrinkTime = TimeSeconds + SafeZonePhases.Data[0].WaitTime;
                SafeZoneIndicator->SafeZoneFinishShrinkTime = SafeZoneIndicator->SafeZoneStartShrinkTime + SafeZonePhases.Data[0].ShrinkTime;

                SafeZoneIndicator->CurrentPhase = 0;
                SafeZoneIndicator->OnRep_CurrentPhase();
            }

            GamePhaseLogic->SafeZoneIndicator = SafeZoneIndicator;
            GamePhaseLogic->OnRep_SafeZoneIndicator();
        }

        StartNewSafeZonePhase(GamePhaseLogic, bLateGame ? GetLateGamePhase((AFortGameStateAthena*)UWorld::GetWorld()->GameState) : 1);
        return;
    }

    auto SafeZoneIndicator = GamePhaseLogic->SafeZoneIndicator;

    if (!bFormedZone || !SafeZoneIndicator)
        return;

    if (SafeZoneIndicator->SafeZonePhases.IsValidIndex(SafeZoneIndicator->CurrentPhase))
    {
        if (!bSafeZoneShrinking && SafeZoneIndicator->SafeZoneStartShrinkTime < Time)
        {
            bSafeZoneShrinking = true;

            SetSafeZoneState(SafeZoneIndicator, EFortSafeZoneState::Shrinking);
            SetGamePhaseStep(GamePhaseLogic, EAthenaGamePhaseStep::StormShrinking);
        }
        else if (SafeZoneIndicator->SafeZoneFinishShrinkTime < Time && SafeZoneIndicator->SafeZonePhases.IsValidIndex(SafeZoneIndicator->CurrentPhase + 1))
        {
            StartNewSafeZonePhase(GamePhaseLogic, SafeZoneIndicator->CurrentPhase + 1);
            bSafeZoneShrinking = false;
        }
    }

    for (auto& Player : GameMode->AlivePlayers)
    {
        if (auto Pawn = Player->MyFortPawn)
            UpdatePawnStormState(GamePhaseLogic, Pawn);
    }

    TArray<AFortPlayerPawnAthena*> Pawns;
    GetAll<AFortPlayerPawnAthena>(Pawns);

    for (auto& Pawn : Pawns)
    {
        if (!Pawn || Pawn->bActorIsBeingDestroyed || !Pawn->Controller || Pawn->Controller->IsA(AFortPlayerControllerAthena::StaticClass()))
            continue;

        UpdatePawnStormState(GamePhaseLogic, Pawn);

        auto AbilitySystemComponent = Pawn->AbilitySystemComponent;

        if (AbilitySystemComponent && !Pawn->SafeZoneAppliedGE)
        {
            if (auto Effect = ApplyOutsideSafeZoneEffect(AbilitySystemComponent))
                Pawn->SafeZoneAppliedGE = (UClass*)Effect;
        }
    }

    Pawns.Free();
}
