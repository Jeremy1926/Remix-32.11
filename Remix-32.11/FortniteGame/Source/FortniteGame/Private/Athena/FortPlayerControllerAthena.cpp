#include "pch.h"
#include "Shared.h"
#include "Config.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortPlayerControllerAthena.h"
#include "FortniteGame/Plugins/GameFeatures/VictoryCrownsGameplay/Source/VictoryCrownsRuntime/Public/FortControllerComponent_VictoryCrowns.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/ControllerComponents/FortControllerComponent_TournamentStatsTracker.h"
#include "FortniteGame/Source/FortniteGame/Public/FortKismetLibrary.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortPickup.h"
#include "FortniteGame/Source/FortniteGame/Public/Quests/FortQuestManager.h"

INIT_POSTLOAD(AFortPlayerControllerAthena);

static void (*GetPlayerViewPointOG)(AFortPlayerControllerAthena*, FVector&, FRotator&);
static void (*OnPawnDiedOG)(AFortPlayerControllerAthena*, AFortPawn*, float, const FGameplayTagContainer&, const FGameplayEffectContextHandle&, AController*, AActor*, AActor*);

static std::optional<UFortItemDefinition*> CachedDefaultPickaxe;
static std::optional<UFortItemDefinition*> CachedWallBuild;
static std::optional<UFortItemDefinition*> CachedFloorBuild;
static std::optional<UFortItemDefinition*> CachedStairBuild;
static std::optional<UFortItemDefinition*> CachedConeBuild;
static std::optional<UFortItemDefinition*> CachedEditTool;

static std::optional<UClass*> CachedOutsideSafeZoneEffect;

static void InitializePlayerGameplayAbilities(AFortPlayerControllerAthena* PlayerController)
{
    static auto InitializePlayerGameplayAbilities_ = (void (*)(void*))(ImageBase + 0x9A8894C);

    if (auto Interface = PlayerController->PlayerState->GetInterface(IFortAbilitySystemInterface::StaticClass()))
        InitializePlayerGameplayAbilities_(Interface);
}

static void GetPlayerViewPoint(AFortPlayerControllerAthena* PlayerController, FVector& Location, FRotator& Rotation)
{
    if (auto ViewTarget = PlayerController->GetViewTarget())
    {
        ViewTarget->GetActorEyesViewPoint(&Location, &Rotation);
        return;
    }

    GetPlayerViewPointOG(PlayerController, Location, Rotation);
}

static void ApplyCosmeticLoadoutToPawn(AFortPlayerControllerAthena* PlayerController, APawn* Pawn)
{
    if (!Pawn || !Pawn->IsA(AFortPlayerPawnAthena::StaticClass()) || !PlayerController->CosmeticLoadoutPC.Character)
        return;

    ((void (*)(APawn*, const FFortAthenaLoadout*, bool, UObject*))Pawn->VTable[0x29E])(Pawn, &PlayerController->CosmeticLoadoutPC, true, nullptr);
}

static AFortPlayerControllerAthena* GetPlayerControllerAthena(AFortPlayerStateAthena* PlayerState)
{
    return PlayerState && PlayerState->Owner && PlayerState->Owner->IsA(AFortPlayerControllerAthena::StaticClass()) ? (AFortPlayerControllerAthena*)PlayerState->Owner : nullptr;
}

void RemoveDroppableEntries(AFortInventory* WorldInventory)
{
    std::vector<FGuid> GuidsToRemove;

    for (auto& Entry : WorldInventory->Inventory.ReplicatedEntries)
    {
        if (CanBeDropped((UFortItemDefinition*)Entry.ItemDefinition))
            GuidsToRemove.push_back(Entry.ItemGuid);
    }

    for (auto& Guid : GuidsToRemove)
        Remove(WorldInventory, Guid);
}

struct FLateGameItem
{
    UFortItemDefinition* ItemDefinition;
    int32 Count;
    float Weight;
};

struct FLateGamePool
{
    std::vector<FLateGameItem> Shotguns;
    std::vector<FLateGameItem> Rifles;
    std::vector<FLateGameItem> Utilities;
    std::vector<FLateGameItem> Heals;
    std::vector<FLateGameItem> Traps;
    std::vector<FLateGameItem> Ammo;
};

static void AddLateGameItem(std::vector<FLateGameItem>& Items, UFortItemDefinition* ItemDefinition, int32 Count, float Weight)
{
    for (auto& Item : Items)
    {
        if (Item.ItemDefinition != ItemDefinition)
            continue;

        Item.Count = (std::max)(Item.Count, Count);
        Item.Weight += Weight;
        return;
    }

    Items.push_back({ ItemDefinition, Count, Weight });
}

static FLateGamePool LateGamePool;
static bool bLateGamePoolBuilt = false;

static FLateGamePool& GetLateGamePool()
{
    if (bLateGamePoolBuilt)
        return LateGamePool;

    bLateGamePoolBuilt = true;

    auto LightAmmo = FindObject<UFortWorldItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight");
    auto MediumAmmo = FindObject<UFortWorldItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium");
    auto HeavyAmmo = FindObject<UFortWorldItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy");
    auto ShellsAmmo = FindObject<UFortWorldItemDefinition>(L"/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells");

    for (auto& [PackageId, Packages] : LootPackageMap)
    {
        for (auto Package : Packages)
        {
            if (!Package || Package->Weight <= 0.f || Package->LootPackageCall.Num() > 1)
                continue;

            auto ItemDefinition = Package->ItemDefinition.Get();

            if (!ItemDefinition)
                continue;

            auto Count = Package->Count > 0 ? Package->Count : 1;

            if (ItemDefinition->IsA(UFortAmmoItemDefinition::StaticClass()))
                AddLateGameItem(LateGamePool.Ammo, ItemDefinition, Count, Package->Weight);
            else if (ItemDefinition->ItemType == EFortItemType::Trap)
                AddLateGameItem(LateGamePool.Traps, ItemDefinition, Count, Package->Weight);
            else if (ItemDefinition->ItemType == EFortItemType::Consumable)
                AddLateGameItem(LateGamePool.Heals, ItemDefinition, Count, Package->Weight);
            else if (ItemDefinition->IsA(UFortWeaponRangedItemDefinition::StaticClass()))
            {
                auto AmmoDefinition = ((UFortWorldItemDefinition*)ItemDefinition)->GetAmmoWorldItemDefinition_BP();

                if (AmmoDefinition && AmmoDefinition == ShellsAmmo)
                    AddLateGameItem(LateGamePool.Shotguns, ItemDefinition, 1, Package->Weight);
                else if (AmmoDefinition && AmmoDefinition == MediumAmmo)
                    AddLateGameItem(LateGamePool.Rifles, ItemDefinition, 1, Package->Weight);
                else if (AmmoDefinition && (AmmoDefinition == LightAmmo || AmmoDefinition == HeavyAmmo))
                    AddLateGameItem(LateGamePool.Utilities, ItemDefinition, 1, Package->Weight);
            }
        }
    }

    return LateGamePool;
}

static const FLateGameItem* PickLateGameItem(const std::vector<FLateGameItem>& Items)
{
    auto bHasRare = std::any_of(Items.begin(), Items.end(), [](const FLateGameItem& Item) { return Item.ItemDefinition->Rarity >= EFortRarity::Rare; });
    float TotalWeight = 0.f;

    for (auto& Item : Items)
    {
        if (!bHasRare || Item.ItemDefinition->Rarity >= EFortRarity::Rare)
            TotalWeight += Item.Weight;
    }

    auto Roll = (float)UKismetMathLibrary::RandomFloatInRange(0., TotalWeight);
    const FLateGameItem* Picked = nullptr;

    for (auto& Item : Items)
    {
        if (bHasRare && Item.ItemDefinition->Rarity < EFortRarity::Rare)
            continue;

        Picked = &Item;
        Roll -= Item.Weight;

        if (Roll <= 0.f)
            break;
    }

    return Picked;
}

static int32 GetLateGameCount(const UFortItemDefinition* ItemDefinition, int32 Min, int32 Max)
{
    auto MaxStack = ItemDefinition->GetMaxStackSize(nullptr);

    if (MaxStack > 0 && Max > MaxStack)
        Max = MaxStack;

    return UKismetMathLibrary::RandomIntegerInRange((std::min)(Min, Max), Max);
}

void GiveLateGameLoadout(AFortInventory* WorldInventory, AFortPawn* Pawn)
{
    RemoveDroppableEntries(WorldInventory);
    Pawn->SetShield(100.f);

    auto& Pool = GetLateGamePool();
    std::vector<FLateGameItem> Loadout;

    if (LateGameLoadout.empty())
    {
        for (auto Items : { &Pool.Shotguns, &Pool.Rifles, &Pool.Utilities, &Pool.Heals, &Pool.Heals, &Pool.Traps })
        {
            if (auto Item = PickLateGameItem(*Items))
                Loadout.push_back(*Item);
        }
    }
    else
    {
        for (auto& [Path, Count] : LateGameLoadout)
        {
            if (auto ItemDefinition = FindObject<UFortItemDefinition>(Path.c_str()))
                Loadout.push_back({ ItemDefinition, Count, 0.f });
        }
    }

    for (auto ResourceType : { EFortResourceType::Wood, EFortResourceType::Stone, EFortResourceType::Metal })
    {
        if (auto Resource = UFortKismetLibrary::K2_GetResourceItemDefinition(ResourceType))
            GiveItem(WorldInventory, Resource, Resource->GetMaxStackSize(nullptr));
    }

    for (auto& Ammo : Pool.Ammo)
        GiveItem(WorldInventory, Ammo.ItemDefinition, GetLateGameCount(Ammo.ItemDefinition, Ammo.Count * 4, Ammo.Count * 10));

    for (auto& Item : Loadout)
    {
        auto WeaponDefinition = Item.ItemDefinition->IsA(UFortGadgetItemDefinition::StaticClass()) ? ((UFortGadgetItemDefinition*)Item.ItemDefinition)->GetWeaponItemDefinition()
                                : Item.ItemDefinition->IsA(UFortWeaponItemDefinition::StaticClass()) ? (UFortWeaponItemDefinition*)Item.ItemDefinition
                                                                                                      : nullptr;
        auto Stats = GetStats(WeaponDefinition);

        GiveItem(WorldInventory, Item.ItemDefinition, Item.Count, Stats ? Stats->ClipSize : 0);
    }
}

static void ServerAcknowledgePossession(UObject* Context, FFrame& Stack)
{
    APawn* P;

    Stack.StepCompiledIn(&P);
    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;

    if (!P)
        return;

    auto bAlreadyAcknowledged = PlayerController->AcknowledgedPawn == P;

    ((void (*)(AFortPlayerControllerAthena*, APawn*))AFortPlayerController::GetDefaultObj()->VTable[0x135])(PlayerController, P);

    if (bAlreadyAcknowledged || P != PlayerController->Pawn)
        return;

    ApplyCosmeticLoadoutToPawn(PlayerController, P);

    auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
    auto WorldInventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;
    auto Num = WorldInventory->Inventory.ReplicatedEntries.Num();

    ApplyOutsideSafeZoneEffect(((AFortPlayerStateAthena*)PlayerController->PlayerState)->AbilitySystemComponent);
    InitializePlayerGameplayAbilities(PlayerController);

    if (Num != 0)
    {
        if (bLateGame)
            GiveLateGameLoadout(WorldInventory, (AFortPawn*)P);

        return;
    }

    auto DefaultPickaxe = FindCachedObject(CachedDefaultPickaxe, L"/Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01");

    auto Pickaxe = PlayerController->CosmeticLoadoutPC.Pickaxe && PlayerController->CosmeticLoadoutPC.Pickaxe->WeaponDefinition ? (UFortItemDefinition*)PlayerController->CosmeticLoadoutPC.Pickaxe->WeaponDefinition
                                                                                                                                 : DefaultPickaxe;

    if (GameMode->StartingItems.Num() == 0)
    {
        auto WallBuild = FindCachedObject(CachedWallBuild, L"/Game/Items/Weapons/BuildingTools/BuildingItemData_Wall.BuildingItemData_Wall");
        auto FloorBuild = FindCachedObject(CachedFloorBuild, L"/Game/Items/Weapons/BuildingTools/BuildingItemData_Floor.BuildingItemData_Floor");
        auto StairBuild = FindCachedObject(CachedStairBuild, L"/Game/Items/Weapons/BuildingTools/BuildingItemData_Stair_W.BuildingItemData_Stair_W");
        auto ConeBuild = FindCachedObject(CachedConeBuild, L"/Game/Items/Weapons/BuildingTools/BuildingItemData_RoofS.BuildingItemData_RoofS");
        auto EditTool = FindCachedObject(CachedEditTool, L"/Game/Items/Weapons/BuildingTools/EditTool.EditTool");

        GiveItem(WorldInventory, Pickaxe);
        GiveItem(WorldInventory, WallBuild);
        GiveItem(WorldInventory, FloorBuild);
        GiveItem(WorldInventory, StairBuild);
        GiveItem(WorldInventory, ConeBuild);
        GiveItem(WorldInventory, EditTool);
    }
    else
    {
        GiveItem(WorldInventory, Pickaxe);

        for (auto& StartingItem : GameMode->StartingItems)
        {
            auto Item = (UFortItemDefinition*)StartingItem.Item;

            if (Item && Item->ItemType == EFortItemType::WeaponHarvest)
                continue;

            if (StartingItem.Count && !Item->IsA(UFortSmartBuildingItemDefinition::StaticClass()))
                GiveItem(WorldInventory, Item, StartingItem.Count);
        }
    }

    UFortKismetLibrary::UpdatePlayerCustomCharacterPartsVisualization((AFortPlayerState*)PlayerController->PlayerState);
}

const UClass* ApplyOutsideSafeZoneEffect(UAbilitySystemComponent* AbilitySystemComponent)
{
    auto Effect = FindCachedObject(CachedOutsideSafeZoneEffect, L"/Game/Athena/SafeZone/GE_OutsideSafeZoneDamage.GE_OutsideSafeZoneDamage_C");

    for (auto& ActiveEffect : AbilitySystemComponent->ActiveGameplayEffects.GameplayEffects_Internal)
    {
        if (ActiveEffect.Spec.Def && ActiveEffect.Spec.Def->IsA(Effect))
            return Effect;
    }

    auto ActiveEffectHandle = AbilitySystemComponent->BP_ApplyGameplayEffectToSelf((UClass*)Effect, 0.f, FGameplayEffectContextHandle());

    AbilitySystemComponent->UpdateActiveGameplayEffectSetByCallerMagnitude(ActiveEffectHandle, FGameplayTag{ FName(L"SetByCaller.StormCampingDamage") }, 1.f);

    return ActiveEffectHandle.Handle != -1 ? Effect : nullptr;
}

static UClass* HealthEffect = nullptr;
static UClass* ShieldEffect = nullptr;
static bool bResolvedEffects = false;

static void ApplySiphon(AFortPlayerStateAthena* KillerPlayerState, AFortPlayerPawnAthena* KillerPawn, AFortPlayerControllerAthena* KillerPlayerController)
{
    if (!bResolvedEffects)
    {
        bResolvedEffects = true;
        HealthEffect = FindObject<UClass>(L"/Game/Athena/Playlists/Showdown/Siphon/GE_SD_SiphonEffect_Heal.GE_SD_SiphonEffect_Heal_C");
        ShieldEffect = FindObject<UClass>(L"/Game/Athena/Playlists/Showdown/Siphon/GE_SD_SiphonEffect_Shield.GE_SD_SiphonEffect_Shield_C");

        if (!HealthEffect || !ShieldEffect)
        {
            HealthEffect = FindObject<UClass>(L"/Game/Athena/Heroes/Abilities/Siphon/GE_SiphonEffect_Heal.GE_SiphonEffect_Heal_C");
            ShieldEffect = FindObject<UClass>(L"/Game/Athena/Heroes/Abilities/Siphon/GE_SiphonEffect_Shield.GE_SiphonEffect_Shield_C");
        }
    }

    auto AbilitySystemComponent = KillerPlayerState->AbilitySystemComponent;

    if (!KillerPawn->IsDBNO())
    {
        auto Health = KillerPawn->GetHealth();
        auto MaxHealth = KillerPawn->GetMaxHealth();
        auto Shield = KillerPawn->GetShield();
        auto MaxShield = KillerPawn->GetMaxShield();
        auto Pool = 50.f;
        float HealthHeal = 0.f;
        float ShieldHeal = 0.f;

        if (MaxHealth > Health)
        {
            HealthHeal = MaxHealth - Health < Pool ? MaxHealth - Health : Pool;
            Pool -= ceilf(HealthHeal);
        }

        if (MaxShield > Shield && Pool > 0.f)
            ShieldHeal = floorf(Pool) < MaxShield - Shield ? floorf(Pool) : MaxShield - Shield;

        if (HealthHeal > 0.f)
        {
            if (HealthEffect)
                AbilitySystemComponent->BP_ApplyGameplayEffectSpecToSelf(UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
                    AbilitySystemComponent->MakeOutgoingSpec(HealthEffect, 1.f, AbilitySystemComponent->MakeEffectContext()), FGameplayTag{ FName(L"SetByCaller.Healing") }, HealthHeal));

            if (KillerPawn->GetHealth() < Health + HealthHeal - 0.5f)
                KillerPawn->SetHealth(Health + HealthHeal);
        }

        if (ShieldHeal > 0.f)
        {
            if (ShieldEffect)
                AbilitySystemComponent->BP_ApplyGameplayEffectSpecToSelf(UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
                    AbilitySystemComponent->MakeOutgoingSpec(ShieldEffect, 1.f, AbilitySystemComponent->MakeEffectContext()), FGameplayTag{ FName(L"SetByCaller.Shield") }, ShieldHeal));

            if (KillerPawn->GetShield() < Shield + ShieldHeal - 0.5f)
                KillerPawn->SetShield(Shield + ShieldHeal);
        }
    }

    static auto SiphonCueTag = FGameplayTag{ FName(L"GameplayCue.Shield.PotionConsumed") };

    auto EffectContext = AbilitySystemComponent->MakeEffectContext();
    AbilitySystemComponent->NetMulticast_InvokeGameplayCueAdded(SiphonCueTag, FPredictionKey{}, EffectContext);
    AbilitySystemComponent->NetMulticast_InvokeGameplayCueExecuted(SiphonCueTag, FPredictionKey{}, EffectContext);

    auto KillerBotController = !KillerPlayerController && KillerPlayerState->Owner && KillerPlayerState->Owner->IsA(AFortAthenaAIBotController::StaticClass()) ? (AFortAthenaAIBotController*)KillerPlayerState->Owner : nullptr;
    auto WorldInventory = KillerPlayerController ? (AFortInventory*)KillerPlayerController->WorldInventory.ObjectPointer
                                                 : (KillerBotController && KillerBotController->bIsAnAthenaGameParticipant ? (AFortInventory*)KillerBotController->Inventory.ObjectPointer : nullptr);

    if (!WorldInventory)
        return;

    for (auto ResourceType : { EFortResourceType::Wood, EFortResourceType::Stone, EFortResourceType::Metal })
        AddItemCount(WorldInventory, UFortKismetLibrary::K2_GetResourceItemDefinition(ResourceType), 50);
}

void DropItemsAsPickups(TArray<UFortWorldItem*>& ItemsToDropViaPickup, AFortPawn* DestructionPawn, AFortInventory* Inventory, AActor* PickupPostSpawnObject, void (*PickupPostSpawn)(AActor*, AFortPickup*, int32))
{
    auto Location = DestructionPawn->K2_GetActorLocation();

    for (auto Item : ItemsToDropViaPickup)
    {
        auto ItemGuid = Item->ItemEntry.ItemGuid;

        for (auto& Entry : Inventory->Inventory.ReplicatedEntries)
        {
            if (Entry.ItemGuid != ItemGuid)
                continue;

            auto Pickup = SpawnPickup(Location, Entry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::PlayerElimination, DestructionPawn);

            if (Pickup && PickupPostSpawn)
                PickupPostSpawn(PickupPostSpawnObject, Pickup, -1);

            break;
        }

        Remove(Inventory, ItemGuid);
    }
}

static void OnPawnDied(AFortPlayerControllerAthena* PlayerController, AFortPawn* DeadPawn, float Damage, const FGameplayTagContainer& DamageTags, const FGameplayEffectContextHandle& EffectContext,
                       AController* InstigatedBy, AActor* DamageCauser, AActor* KillerActor)
{
    auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;

    if (PlayerState && DeadPawn)
    {
        PlayerState->DeathInfo.DeathLocation = DeadPawn->K2_GetActorLocation();
        PlayerState->DeathInfo.bInitialized = true;
    }

    OnPawnDiedOG(PlayerController, DeadPawn, Damage, DamageTags, EffectContext, InstigatedBy, DamageCauser, KillerActor);
}

void ReportDeath(AController* Controller, AFortPlayerStateAthena* PlayerState, AFortPlayerPawnAthena* Pawn, AFortPlayerStateAthena* KillerPlayerState, AFortPlayerPawnAthena* KillerPawn, const FGameplayTagContainer& Tags)
{
    static auto GetTeamMembers = (TArray<TWeakObjectPtr<AFortPlayerStateAthena>>* (*)(AFortPlayerStateAthena*))(ImageBase + 0x927AA08);

    auto KillerPlayerController = GetPlayerControllerAthena(KillerPlayerState);

    PlayerState->PawnDeathLocation = Pawn ? Pawn->K2_GetActorLocation() : FVector();

    auto& DeathInfo = PlayerState->DeathInfo;

    memset((PBYTE)&DeathInfo, 0, sizeof(FDeathInfo));
    DeathInfo.bDBNO = Pawn ? Pawn->IsDBNO() : false;
    DeathInfo.DeathLocation = PlayerState->PawnDeathLocation;
    if (Pawn)
        UBlueprintGameplayTagLibrary::AppendGameplayTagContainers(DeathInfo.DeathTags, *(FGameplayTagContainer*)(uint64(&Pawn->MoveSoundStimulusBroadcastInterval) + 0x10));
    DeathInfo.DeathClassSlot = (uint8)-1;
    DeathInfo.DeathCause = AFortPlayerStateAthena::ToDeathCause(Tags, DeathInfo.bDBNO);
    DeathInfo.FinisherOrDowner = KillerPlayerState ? KillerPlayerState : PlayerState;
    UBlueprintGameplayTagLibrary::AppendGameplayTagContainers(DeathInfo.FinisherOrDownerTags, KillerPawn ? KillerPawn->GameplayTags : Pawn->GameplayTags);
    UBlueprintGameplayTagLibrary::AppendGameplayTagContainers(DeathInfo.VictimTags, Pawn->GameplayTags);
    DeathInfo.Distance = Pawn ? (DeathInfo.DeathCause != EDeathCause::FallDamage ? (KillerPawn ? KillerPawn->GetDistanceTo(Pawn) : 0) : Pawn->LastFallDistance) : 0;
    DeathInfo.bInitialized = true;
    PlayerState->OnRep_DeathInfo();

    if (KillerPlayerState && KillerPawn && KillerPawn->Controller && KillerPawn->Controller != Controller)
    {
        KillerPlayerState->KillScore++;
        KillerPlayerState->OnRep_Kills();

        if (!PlayerState->bIsABot)
            KillerPlayerState->HumanKillScore++;

        KillerPlayerState->ClientReportKill(PlayerState);

        for (auto& TeamMember : *GetTeamMembers(KillerPlayerState))
        {
            auto TeamPlayerState = TeamMember.Get();

            if (!TeamPlayerState)
                continue;

            TeamPlayerState->TeamKillScore++;
            TeamPlayerState->OnRep_TeamKillScore();
            TeamPlayerState->ClientReportTeamKill(TeamPlayerState->TeamKillScore);
        }
    }

    if (Pawn && KillerPlayerState && KillerPlayerState->AbilitySystemComponent && KillerPawn && KillerPawn->Controller != Controller)
        ApplySiphon(KillerPlayerState, KillerPawn, KillerPlayerController);
}

static void StartRespawnTimer(AFortPlayerControllerAthena* PlayerController, float RespawnTime, bool bNextRespawnInAir)
{
    static auto StartRespawnTimerOG = (void (*)(AFortPlayerControllerAthena*, float, bool))(ImageBase + 0x9228A2C);
    static auto SetServerTimeForRespawn = (void (*)(AFortPlayerControllerAthena*, float))(ImageBase + 0x9228C1C);

    SetServerTimeForRespawn(PlayerController, (float)(UWorld::GetWorld()->GameState->GetServerWorldTimeSeconds() + RespawnTime));
    PlayerController->ClientSetDeathReport(PlayerController->PlayerDeathReport);

    StartRespawnTimerOG(PlayerController, RespawnTime, bNextRespawnInAir);
}

UFortWeaponItemDefinition* GetKillerWeapon(AActor* DamageCauser)
{
    AFortWeapon* Weapon = nullptr;

    if (DamageCauser && DamageCauser->IsA(AFortProjectileBase::StaticClass()))
    {
        auto Owner = DamageCauser->Owner;

        if (Owner && Owner->IsA(AFortWeapon::StaticClass()))
            Weapon = (AFortWeapon*)Owner;
        else if (Owner && Owner->IsA(AFortPlayerControllerAthena::StaticClass()) && ((AFortPlayerControllerAthena*)Owner)->Pawn)
            Weapon = ((AFortPlayerPawnAthena*)((AFortPlayerControllerAthena*)Owner)->Pawn)->CurrentWeapon;
        else if (Owner && Owner->IsA(AFortPlayerPawnAthena::StaticClass()))
            Weapon = ((AFortPlayerPawnAthena*)Owner)->CurrentWeapon;
    }
    else if (DamageCauser && DamageCauser->IsA(AFortWeapon::StaticClass()))
        Weapon = (AFortWeapon*)DamageCauser;

    return Weapon && Weapon->IsA(AFortWeapon::StaticClass()) ? Weapon->WeaponData : nullptr;
}

void ReportElimination(AFortGameModeAthena* GameMode, AFortPlayerStateAthena* PlayerState, AFortPlayerStateAthena* KillerPlayerState, AFortPlayerPawnAthena* KillerPawn, UFortWeaponItemDefinition* KillerWeapon, EDeathCause DeathCause)
{
    static auto StartEndGamePhaseTeam = (bool (*)(AFortGameModeAthena*, uint8, AFortPlayerStateAthena*, int32, APawn*, UFortWeaponItemDefinition*, EDeathCause))(ImageBase + 0x918EFF4);

    auto GameState = (AFortGameStateAthena*)GameMode->GameState;
    auto KillerPlayerController = GetPlayerControllerAthena(KillerPlayerState);
    auto Playlist = GameState->CurrentPlaylistInfo.BasePlaylist;
    auto TeamAliveIndex = Playlist ? PlayerState->TeamIndex - Playlist->DefaultFirstTeam : -1;
    auto TeamInfo = ((AFortTeamInfo* (*)(AFortGameStateAthena*, uint8))GameState->VTable[0x136])(GameState, PlayerState->TeamIndex);
    auto bTeamPlaced = TeamInfo && TeamInfo->IsA(AFortTeamInfoAthena::StaticClass()) && *(int32*)(uint64(TeamInfo) + 0x410) > 0;

    if (PlayerState->Place == 0 && !bTeamPlaced && GameMode->TeamAlivePlayers.IsValidIndex(TeamAliveIndex) && GameMode->TeamAlivePlayers[TeamAliveIndex] <= 0)
        StartEndGamePhaseTeam(GameMode, PlayerState->TeamIndex, nullptr, GameState->TeamsLeft + 1, KillerPawn, KillerWeapon, DeathCause);

    TournamentPlayerEliminated(GameMode, PlayerState, KillerPlayerState == PlayerState ? nullptr : KillerPlayerState);
    SendPlacementUpdate(KillerPlayerController);

    if (KillerPlayerState && KillerPlayerState != PlayerState && KillerPlayerState->Place == 1)
    {
        if (KillerPlayerController)
        {
            GrantVictoryCrownAwards(KillerPlayerController);
            KillerPlayerController->PlayWinEffects(KillerPawn, KillerWeapon, DeathCause, false, ERoundVictoryAnimation::Default);
            KillerPlayerController->ClientNotifyWon(KillerPawn, KillerWeapon, DeathCause);
            KillerPlayerController->ClientNotifyTeamWon(KillerPawn, KillerWeapon, DeathCause);
        }

        GameState->WinningTeam = KillerPlayerState->TeamIndex;
        GameState->OnRep_WinningTeam();
        GameState->WinningPlayerState = KillerPlayerState;
        GameState->OnRep_WinningPlayerState();
    }

    for (auto& PlacedPlayer : GameState->PlayerArray)
    {
        auto PlacedPlayerState = (AFortPlayerStateAthena*)PlacedPlayer;

        if (!PlacedPlayerState || !PlacedPlayerState->IsA(AFortPlayerStateAthena::StaticClass()) || PlacedPlayerState->Place <= 0)
            continue;

        if (auto PlacedPlayerController = GetPlayerControllerAthena(PlacedPlayerState))
            EndBattleRoyaleGame(PlacedPlayerController);
    }
}

static void ServerSetMultiProductCosmeticLoadout(UObject* Context, FFrame& Stack)
{
    FCosmeticLoadout LoadoutTemp{};
    auto& Loadout = Stack.StepCompiledInRef(&LoadoutTemp);
    TArray<FCosmeticLoadoutActiveArchetype> ArchetypesTemp{};
    auto& Archetypes = Stack.StepCompiledInRef(&ArchetypesTemp);

    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;
    auto CosmeticLoadoutComponent = PlayerController->CosmeticLoadoutComponent;

    if (!CosmeticLoadoutComponent)
        return;

    static auto SetCosmeticLoadout = (void (*)(UFortControllerComponent_CosmeticLoadout*, const FCosmeticLoadout*))(ImageBase + 0x9672F80);
    static auto BroadcastActiveArchetypesChanged = (void (*)(void*))(ImageBase + 0x4B6F354);
    static auto GetCosmeticLoadout = (FFortAthenaLoadout* (*)(AFortPlayerControllerAthena*, FFortAthenaLoadout*, bool, bool*))(ImageBase + 0xA22C2B8);
    static auto DestructAthenaLoadout = (void (*)(FFortAthenaLoadout*))(ImageBase + 0x25D5B28);
    static auto CachedLoadoutCharacterInfoName = FName(L"CachedLoadoutCharacterInfo");

    SetCosmeticLoadout(CosmeticLoadoutComponent, &Loadout);

    auto& ActiveArchetypes = CosmeticLoadoutComponent->ActiveArchetypes;

    if (ActiveArchetypes.Num() != Archetypes.Num() || (Archetypes.Num() != 0 && memcmp(ActiveArchetypes.Data, Archetypes.Data, Archetypes.Num() * sizeof(FCosmeticLoadoutActiveArchetype)) != 0))
    {
        ActiveArchetypes.Free();

        for (auto& Archetype : Archetypes)
            ActiveArchetypes.Add(Archetype);

        BroadcastActiveArchetypesChanged((uint8*)CosmeticLoadoutComponent + 0x360);
    }

    alignas(16) uint8 LoadoutData[0x400];
    auto AthenaLoadout = (FFortAthenaLoadout*)LoadoutData;
    bool bFromComponent = false;

    GetCosmeticLoadout(PlayerController, AthenaLoadout, false, &bFromComponent);

    if (bFromComponent)
    {
        auto CosmeticLoadoutInterface = (void**)(uint64(PlayerController) + 0x868);
        ((void (*)(void**, FFortAthenaLoadout*))(*(void***)CosmeticLoadoutInterface)[4])(CosmeticLoadoutInterface, AthenaLoadout);
    }

    DestructAthenaLoadout(AthenaLoadout);

    if (!bFromComponent)
        return;

    auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;

    PlayerState->CachedLoadoutCharacterInfo.CachedLoadoutCharacterUngated = PlayerController->CosmeticLoadoutPC.Character;
    UNetPushModelHelpers::MarkPropertyDirty(PlayerState, CachedLoadoutCharacterInfoName);

    auto MyFortPawn = PlayerController->MyFortPawn;
    auto WorldInventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;

    if (MyFortPawn)
        ApplyCosmeticLoadoutToPawn(PlayerController, MyFortPawn);

    auto Pickaxe = PlayerController->CosmeticLoadoutPC.Pickaxe;

    if (!WorldInventory || !Pickaxe || !Pickaxe->WeaponDefinition)
        return;

    std::vector<FGuid> OldPickaxes;
    bool bHasPickaxe = false;
    bool bWasEquipped = false;

    for (auto& Entry : WorldInventory->Inventory.ReplicatedEntries)
    {
        if (!Entry.ItemDefinition || ((UFortItemDefinition*)Entry.ItemDefinition)->ItemType != EFortItemType::WeaponHarvest)
            continue;

        if (Entry.ItemDefinition == Pickaxe->WeaponDefinition)
        {
            bHasPickaxe = true;
            continue;
        }

        if (MyFortPawn && MyFortPawn->CurrentWeapon && MyFortPawn->CurrentWeapon->ItemEntryGuid == Entry.ItemGuid)
            bWasEquipped = true;

        OldPickaxes.push_back(Entry.ItemGuid);
    }

    if (OldPickaxes.empty())
        return;

    for (auto& OldPickaxe : OldPickaxes)
        Remove(WorldInventory, OldPickaxe);

    auto NewPickaxe = bHasPickaxe ? FindItemInstance(WorldInventory, Pickaxe->WeaponDefinition) : GiveItem(WorldInventory, Pickaxe->WeaponDefinition);

    if (bWasEquipped && NewPickaxe && MyFortPawn)
        MyFortPawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)NewPickaxe->ItemEntry.ItemDefinition, NewPickaxe->ItemEntry.ItemGuid, NewPickaxe->ItemEntry.TrackerGuid, false);
}

static void ServerPlaySquadQuickChatMessage(UObject* Context, FFrame& Stack)
{
    FAthenaQuickChatActiveEntry ChatEntryTemp{};
    auto& ChatEntry = Stack.StepCompiledInRef(&ChatEntryTemp);
    FUniqueNetIdRepl SenderIDTemp{};
    auto& SenderID = Stack.StepCompiledInRef(&SenderIDTemp);

    Stack.IncrementCode();

    auto PlayerController = (AFortPlayerControllerAthena*)Context;
    auto Bank = ChatEntry.Bank.Get();

    if (!Bank || !Bank->ChatOptions.IsValidIndex(ChatEntry.Index))
        return;

    auto& ChatOption = Bank->ChatOptions[ChatEntry.Index];
    auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;

    PlayerState->TeamMemberState = ChatOption.TeamCommType;
    PlayerState->ReplicatedTeamMemberState = ChatOption.TeamCommType;

    PlayerController->ServerPlayEmoteItem(ChatOption.EmojiItemDefinition, 0);

    PlayerState->OnRep_ReplicatedTeamMemberState();
}

void PlaceLateGamePawn(AController* Controller, AFortPawn* Pawn)
{
    auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(Controller);
    auto Aircraft = GamePhaseLogic && GamePhaseLogic->Aircrafts_GameState.Num() > 0 ? GamePhaseLogic->Aircrafts_GameState.Data[0].Get() : nullptr;

    if (!Pawn || !Aircraft)
        return;

    auto Angle = UKismetMathLibrary::RandomFloatInRange(0., 360.);
    auto Distance = UKismetMathLibrary::RandomFloatInRange(0., 1000.);

    Pawn->SetShield(100.f);
    Pawn->K2_SetActorLocation(Aircraft->K2_GetActorLocation() + FVector(UKismetMathLibrary::DegCos(Angle) * Distance, UKismetMathLibrary::DegSin(Angle) * Distance, 0.), false, nullptr, true);
}

void AFortPlayerControllerAthena__PostLoad()
{
    auto PlayerControllerClass = AFortPlayerControllerAthena::StaticClass();
    auto DefaultPlayerControllerZone = AFortPlayerControllerZone::GetDefaultObj();

    Hook(ImageBase + 0x1C9AC68, GetPlayerViewPoint, GetPlayerViewPointOG);
    Hook<AFortPlayerControllerAthena>(uint32(0x12D), DefaultPlayerControllerZone->VTable[0x12D]);
    Hook<AFortPlayerControllerAthena>(uint32(0x28C), DefaultPlayerControllerZone->VTable[0x28C]);
    ExecHook(PlayerControllerClass->FindFunction("ServerAcknowledgePossession"), ServerAcknowledgePossession);
    Hook<AFortPlayerControllerAthena>(uint32(0x283), AFortPlayerController::GetDefaultObj()->VTable[0x283]);
    Hook(ImageBase + 0x921754C, OnPawnDied, OnPawnDiedOG);
    Rel32Hook(ImageBase + 0x925B829 + 1, StartRespawnTimer);
    ExecHook(PlayerControllerClass->FindFunction("ServerSetMultiProductCosmeticLoadout"), ServerSetMultiProductCosmeticLoadout);
    ExecHook(PlayerControllerClass->FindFunction("ServerPlaySquadQuickChatMessage"), ServerPlaySquadQuickChatMessage);
}
