#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortAthenaSupplyDrop.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortPickup.h"

INIT_MODULE(AFortAthenaSupplyDrop);
INIT_POSTLOAD(AFortAthenaSupplyDrop);

void SpawnPickup(UObject* Context, FFrame& Stack, AFortPickup** Ret)
{
    UFortWorldItemDefinition* ItemDefinition;
    int32 NumberToSpawn;
    AFortPawn* TriggeringPawn;
    FVector Position;
    FVector Direction;
    Stack.StepCompiledIn(&ItemDefinition);
    Stack.StepCompiledIn(&NumberToSpawn);
    Stack.StepCompiledIn(&TriggeringPawn);
    Stack.StepCompiledIn(&Position);
    Stack.StepCompiledIn(&Direction);
    Stack.IncrementCode();

    auto Stats = ItemDefinition->IsA(UFortWeaponItemDefinition::StaticClass()) ? GetStats((UFortWeaponItemDefinition*)ItemDefinition) : nullptr;

    *Ret = SpawnPickup(Position, ItemDefinition, NumberToSpawn, Stats ? Stats->ClipSize : 0, EFortPickupSourceTypeFlag::Other, EFortPickupSpawnSource::SupplyDrop);
}

static FFortSupplyDropGamePhaseData* GetSupplyDropGamePhaseData(AFortAthenaSupplyDrop* SupplyDrop, UFortGameStateComponent_BattleRoyaleGamePhaseLogic* GamePhaseLogic)
{
    for (auto& SpawnData : GamePhaseLogic->SupplyDropSpawnDataList)
    {
        if (!SpawnData.SupplyDropInfo || SpawnData.SupplyDropInfo->SupplyDropClass != SupplyDrop->Class)
            continue;

        for (auto& GamePhaseData : SpawnData.SupplyDropInfo->GamePhaseData)
        {
            if (GamePhaseData.GamePhase == GamePhaseLogic->GamePhase)
                return &GamePhaseData;
        }
    }

    return nullptr;
}

static void GetSupplyDropSpeedData(UObject* Context, FFrame& Stack, void* Ret)
{
    float MinSpeedTemp = 0.f;
    auto& MinSpeed = Stack.StepCompiledInRef(&MinSpeedTemp);
    float MaxSpeedTemp = 0.f;
    auto& MaxSpeed = Stack.StepCompiledInRef(&MaxSpeedTemp);
    Stack.IncrementCode();

    auto SupplyDrop = (AFortAthenaSupplyDrop*)Context;
    auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(SupplyDrop);
    auto GamePhaseData = GamePhaseLogic ? GetSupplyDropGamePhaseData(SupplyDrop, GamePhaseLogic) : nullptr;

    MinSpeed = GamePhaseData ? (float)(int32)GamePhaseData->SupplyDropMinSpeed.Evaluate() : 280.f;
    MaxSpeed = GamePhaseData ? (float)(int32)GamePhaseData->SupplyDropMaxSpeed.Evaluate() : 500.f;
}

static void (*GetSupplyDropSpawnDataOG)(AFortAthenaSupplyDrop*, float&, float&);
static void GetSupplyDropSpawnData(AFortAthenaSupplyDrop* SupplyDrop, float& MinSpawnHeight, float& MaxSpawnHeight)
{
    auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(SupplyDrop);

    if (!GamePhaseLogic)
        return GetSupplyDropSpawnDataOG(SupplyDrop, MinSpawnHeight, MaxSpawnHeight);

    auto GamePhaseData = GetSupplyDropGamePhaseData(SupplyDrop, GamePhaseLogic);

    MinSpawnHeight = GamePhaseData ? (float)(int32)GamePhaseData->SupplyDropMinSpawnHeight.Evaluate() : 28000.f;
    MaxSpawnHeight = GamePhaseData ? (float)(int32)GamePhaseData->SupplyDropMaxSpawnHeight.Evaluate() : 32000.f;
}

void AFortAthenaSupplyDrop__Init()
{
    Hook(ImageBase + 0x3E88E08, GetSupplyDropSpawnData, GetSupplyDropSpawnDataOG);
}

void AFortAthenaSupplyDrop__PostLoad()
{
    ExecHook(AFortAthenaSupplyDrop::StaticClass()->FindFunction("SpawnPickup"), (void (*)(UObject*, FFrame&, AFortPickup**))SpawnPickup);
    ExecHook(AFortAthenaSupplyDrop::StaticClass()->FindFunction("GetSupplyDropSpeedData"), GetSupplyDropSpeedData);
}
