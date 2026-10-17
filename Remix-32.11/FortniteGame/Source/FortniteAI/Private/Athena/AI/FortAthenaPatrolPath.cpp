#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Plugins/GameFeatures/Lager/Source/LagerRuntime/Public/FortAthenaLivingWorldManager.h"

INIT_MODULE(AFortAthenaPatrolPath);

static void (*BeginPlayOG)(AFortAthenaPatrolPath*);
static void BeginPlay(AFortAthenaPatrolPath* PatrolPath)
{
    BeginPlayOG(PatrolPath);

    if (PatrolPath->Role != ENetRole::ROLE_Authority)
        return;

    auto World = UWorld::GetWorld();
    auto GameState = World ? (AFortGameStateAthena*)World->GameState : nullptr;

    if (!GameState || !GameState->IsA(AFortGameStateAthena::StaticClass()))
        return;

    NotifyWhenPlaylistLoaded(GameState, PatrolPath);
}

void AFortAthenaPatrolPath__Init()
{
    Hook<AFortAthenaPatrolPath>(uint32(0x74), BeginPlay, BeginPlayOG);
}
