#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteAI/Public/Athena/AI/Systems/AthenaAIServiceLoot.h"
#include "FortniteGame/Source/FortniteGame/Public/FortKismetLibrary.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortPickup.h"

INIT_MODULE(ABuildingContainer);

static bool SpawnLoot(ABuildingContainer* Container)
{
    if (Container->bAlreadySearched)
        return false;

    auto RealTierGroup = Container->SearchLootTierGroup;
    auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;

    for (auto& Pair : GameMode->RedirectAthenaLootTierGroups)
    {
        if (Pair.Key().ComparisonIndex == Container->SearchLootTierGroup.ComparisonIndex)
        {
            RealTierGroup.ComparisonIndex = Pair.Value().ComparisonIndex;
            break;
        }
    }

    TArray<FFortItemEntry*> LootDrops{};

    ChooseLootForContainer(LootDrops, RealTierGroup, -1, ((AFortGameStateAthena*)GameMode->GameState)->WorldLevel);

    for (auto& LootDrop : LootDrops)
    {
        SpawnPickup(Container, *LootDrop);
        free(LootDrop);
    }

    Container->bAlreadySearched = true;
    Container->OnRep_bAlreadySearched();
    Container->SearchBounceData.SearchAnimationCount++;
    Container->BounceContainer();

    return true;
}

static void (*BeginPlayOG)(ABuildingContainer*);
static void BeginPlay(ABuildingContainer* Container)
{
    BeginPlayOG(Container);
    RegisterChestLoot(Container);
}

static void (*OnAuthorityRandomUpgradeAppliedOG)(ABuildingContainer*, FName&);
static void OnAuthorityRandomUpgradeApplied(ABuildingContainer* Container, FName& UpgradeTierGroup)
{
    static auto GetSparseClassData = (FBuildingSMActorClassData* (*)(UClass*, uint8))(ImageBase + 0x1BBBDC8);

    auto ChosenRandomUpgrade = Container->ChosenRandomUpgrade;
    auto ClassData = GetSparseClassData(Container->Class, 1);

    if (ChosenRandomUpgrade < 0 || ChosenRandomUpgrade >= ClassData->AlternateMeshes.Num())
        return OnAuthorityRandomUpgradeAppliedOG(Container, UpgradeTierGroup);

    auto& AlternateMeshSet = ClassData->AlternateMeshes[ChosenRandomUpgrade];

    Container->ReplicatedLootTier = AlternateMeshSet.Tier;
    Container->OnRep_LootTier();

    return OnAuthorityRandomUpgradeAppliedOG(Container, UpgradeTierGroup);
}

void ABuildingContainer__Init()
{
    Hook(ImageBase + 0x94CCD48, SpawnLoot);
    Hook(ImageBase + 0x94C7DCC, OnAuthorityRandomUpgradeApplied, OnAuthorityRandomUpgradeAppliedOG);
    Hook<ABuildingContainer>(uint32(0x74), BeginPlay, BeginPlayOG);
}
