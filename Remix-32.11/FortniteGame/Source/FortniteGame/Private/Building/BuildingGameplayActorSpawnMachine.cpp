#include "pch.h"
#include "Shared.h"

INIT_MODULE(ABuildingGameplayActorSpawnMachine);
INIT_TICKER(ABuildingGameplayActorSpawnMachine);

static void (*SetSpawnMachineStateOG)(ABuildingGameplayActorSpawnMachine*, ESpawnMachineState);

static std::vector<std::pair<TWeakObjectPtr<ABuildingGameplayActorSpawnMachine>, float>> PendingResurrections;

static FSpawnMachineRepData* GetSpawnMachineRepData(UFortGameStateComponent_Reboot* RebootComponent, ABuildingGameplayActorSpawnMachine* SpawnMachine)
{
    for (auto& RepData : RebootComponent->SpawnMachineRepData.SpawnMachineRepDataArray)
    {
        if (RepData.SpawnMachineRepDataHandle == SpawnMachine->HandleIntoGameState)
            return &RepData;
    }

    return nullptr;
}

static void SetSpawnMachineState(ABuildingGameplayActorSpawnMachine* SpawnMachine, ESpawnMachineState NewState)
{
    static auto HandleIntoGameStateName = FName(L"HandleIntoGameState");

    auto GameState = UWorld::GetWorld()->GameState;
    auto RebootComponent = SpawnMachine->Role == ENetRole::ROLE_Authority && GameState ? (UFortGameStateComponent_Reboot*)GameState->GetComponentByClass(UFortGameStateComponent_Reboot::StaticClass()) : nullptr;

    if (!RebootComponent)
        return SetSpawnMachineStateOG(SpawnMachine, NewState);

    auto& SpawnMachineRepData = RebootComponent->SpawnMachineRepData;

    if (!GetSpawnMachineRepData(RebootComponent, SpawnMachine))
    {
        FSpawnMachineRepData RepData{};

        RepData.ReplicationID = -1;
        RepData.ReplicationKey = -1;
        RepData.MostRecentArrayReplicationKey = -1;
        RepData.Location = SpawnMachine->K2_GetActorLocation();
        RepData.SpawnMachineRepDataHandle = SpawnMachineRepData.SpawnMachineRepDataArray.Num() + 1;
        RepData.EnabledConfig.bAllowAnyClass = true;
        RepData.EnabledConfig.bIsEnabled = true;

        SpawnMachineRepData.SpawnMachineRepDataArray.Add(RepData);

        SpawnMachine->HandleIntoGameState = RepData.SpawnMachineRepDataHandle;
        UNetPushModelHelpers::MarkPropertyDirty(SpawnMachine, HandleIntoGameStateName);
    }

    if (NewState == ESpawnMachineState::BeingUsed && !SpawnMachine->ResurrectLocation)
    {
        auto ResurrectLocation = SpawnMachine->K2_GetActorLocation();
        ResurrectLocation.Z += SpawnMachine->ResurrectionHeightOffset.Evaluate();

        SpawnMachine->ResurrectLocation = SpawnActor<AFortPlayerStart>(ResurrectLocation, {}, SpawnMachine);
    }

    SetSpawnMachineStateOG(SpawnMachine, NewState);

    auto RepData = GetSpawnMachineRepData(RebootComponent, SpawnMachine);

    if (!RepData)
        return;

    RepData->SpawnMachineState = SpawnMachine->SpawnMachineState;

    if (RepData->SpawnMachineState == ESpawnMachineState::OnCooldown)
    {
        RepData->SpawnMachineCooldownStartTime = (float)GameState->GetServerWorldTimeSeconds();
        RepData->SpawnMachineCooldownEndTime = RepData->SpawnMachineCooldownStartTime + SpawnMachine->GetCooldownLength();
    }

    SpawnMachineRepData.MarkItemDirty(*RepData);
}

static AFortPlayerControllerAthena* GetTeamPlayerController(APlayerState* Player, uint8 TeamIndex)
{
    auto PlayerState = (AFortPlayerStateAthena*)Player;

    if (!PlayerState || !PlayerState->IsA(AFortPlayerStateAthena::StaticClass()) || PlayerState->TeamIndex != TeamIndex || !PlayerState->Owner || !PlayerState->Owner->IsA(AFortPlayerControllerAthena::StaticClass()))
        return nullptr;

    return (AFortPlayerControllerAthena*)PlayerState->Owner;
}

static void ResurrectNextPlayer(ABuildingGameplayActorSpawnMachine* SpawnMachine)
{
    static auto AddToAlivePlayers = (void (*)(AFortGameModeAthena*, AFortPlayerControllerAthena*))(ImageBase + 0x91769C0);
    static auto SetResurrectingNow = (void (*)(UAthenaResurrectionComponent*, bool, bool))(ImageBase + 0x943B638);
    static auto RemovePlayerIdForResurrection = (void (*)(UAthenaResurrectionComponent*, const FUniqueNetIdRepl&))(ImageBase + 0x943A414);
    static auto RemoveAll = (void (*)(void*, UObject*))(ImageBase + 0x16E075C);
    static auto EndSpectating = (void (*)(AFortPlayerStateZone*))(ImageBase + 0x27DFD2C);

    auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
    auto GameState = (AFortGameStateAthena*)GameMode->GameState;
    AFortPlayerControllerAthena* ResurrectedPlayer = nullptr;
    bool bHasNextPlayer = false;

    for (auto& Player : GameState->PlayerArray)
    {
        auto PlayerController = GetTeamPlayerController(Player, SpawnMachine->ActiveTeam);

        if (!PlayerController || !((AFortPlayerStateAthena*)Player)->bResurrectingNow || !PlayerController->ResurrectionComponent || PlayerController->ResurrectionComponent->ResurrectionLocation.Get() != SpawnMachine->ResurrectLocation)
            continue;

        if (ResurrectedPlayer)
        {
            bHasNextPlayer = true;
            break;
        }

        ResurrectedPlayer = PlayerController;
    }

    if (ResurrectedPlayer)
    {
        if (ResurrectedPlayer->Pawn)
            ResurrectedPlayer->UnPossess();

        GameMode->RestartPlayer(ResurrectedPlayer);

        auto PlayerPawn = ResurrectedPlayer->Pawn && ResurrectedPlayer->Pawn->IsA(AFortPlayerPawn::StaticClass()) ? (AFortPlayerPawn*)ResurrectedPlayer->Pawn : nullptr;

        if (PlayerPawn)
        {
            auto PlayerState = (AFortPlayerStateAthena*)ResurrectedPlayer->PlayerState;

            UKismetSystemLibrary::K2_ClearAndInvalidateTimerHandle(ResurrectedPlayer, *(FTimerHandle*)(uint64(ResurrectedPlayer) + 0x3600));
            EndSpectating(PlayerState);

            memset((PBYTE)&PlayerState->DeathInfo, 0, sizeof(FDeathInfo));
            PlayerState->DeathInfo.DeathCause = EDeathCause::Unspecified;
            PlayerState->DeathInfo.DeathClassSlot = (uint8)-1;
            PlayerState->OnRep_DeathInfo();
            PlayerState->RebootCounter++;

            AddToAlivePlayers(GameMode, ResurrectedPlayer);
            SpawnMachine->OnPlayerPawnResurrected(PlayerPawn);
        }
        else
            SetResurrectingNow(ResurrectedPlayer->ResurrectionComponent, false, false);

        for (auto& Player : GameState->PlayerArray)
        {
            auto TeamController = GetTeamPlayerController(Player, SpawnMachine->ActiveTeam);

            if (TeamController && TeamController->ResurrectionComponent)
                RemovePlayerIdForResurrection(TeamController->ResurrectionComponent, ResurrectedPlayer->PlayerState->UniqueID);
        }
    }

    if (bHasNextPlayer)
    {
        PendingResurrections.push_back({ SpawnMachine, (float)UGameplayStatics::GetTimeSeconds(SpawnMachine) + SpawnMachine->ResurrectionNextPlayerDelay.Evaluate() });
        return;
    }

    RemoveAll((uint8*)GameMode + 0x1428, SpawnMachine);
    SpawnMachine->OnResurrectionCompleted();
}

void ABuildingGameplayActorSpawnMachine__Init()
{
    Hook(ImageBase + 0x951BC84, SetSpawnMachineState, SetSpawnMachineStateOG);
    Rel32Hook(ImageBase + 0x951ACF0 + 3, ResurrectNextPlayer);
}

void ABuildingGameplayActorSpawnMachine__Tick()
{
    if (PendingResurrections.empty())
        return;

    auto Time = (float)UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
    std::vector<ABuildingGameplayActorSpawnMachine*> SpawnMachines;

    std::erase_if(PendingResurrections,
                  [&](auto& PendingResurrection)
                  {
                      auto SpawnMachine = PendingResurrection.first.Get();

                      if (SpawnMachine && Time < PendingResurrection.second)
                          return false;

                      if (SpawnMachine && SpawnMachine->SpawnMachineState == ESpawnMachineState::Active)
                          SpawnMachines.push_back(SpawnMachine);

                      return true;
                  });

    for (auto SpawnMachine : SpawnMachines)
        ResurrectNextPlayer(SpawnMachine);
}
