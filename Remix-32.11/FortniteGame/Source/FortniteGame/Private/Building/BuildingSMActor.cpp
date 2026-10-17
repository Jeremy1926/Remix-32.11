#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortPickup.h"

INIT_POSTLOAD(ABuildingSMActor);

static std::optional<UCurveTable*> ResourceRates;

static void (*OnDamageServerOG)(ABuildingSMActor*, float, const FGameplayTagContainer&, const FVector&, const FHitResult&, AController*, AActor*, const FGameplayEffectContextHandle&);
static void OnDamageServer(ABuildingSMActor* Actor, float Damage, const FGameplayTagContainer& DamageTags, const FVector& Momentum, const FHitResult& HitInfo, AController* InstigatedBy, AActor* DamageCauser,
                           const FGameplayEffectContextHandle& EffectContext)
{
    auto GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
    auto Controller = InstigatedBy && InstigatedBy->IsA(AFortPlayerControllerAthena::StaticClass()) ? (AFortPlayerControllerAthena*)InstigatedBy : nullptr;
    auto BotController = InstigatedBy && InstigatedBy->IsA(AFortAthenaAIBotController::StaticClass()) ? (AFortAthenaAIBotController*)InstigatedBy : nullptr;

    if ((!Controller && !BotController) || !Actor->IsA(ABuildingSMActor::StaticClass()) || Actor->bPlayerPlaced || Actor->GetHealth() == 1)
        return OnDamageServerOG(Actor, Damage, DamageTags, Momentum, HitInfo, InstigatedBy, DamageCauser, EffectContext);

    auto Inventory = (AFortInventory*)(Controller ? Controller->WorldInventory.ObjectPointer : BotController->Inventory.ObjectPointer);
    auto InstigatorPawn = Controller ? Controller->MyFortPawn : (AFortPawn*)BotController->Pawn;

    if (!Inventory || !DamageCauser || !DamageCauser->IsA(AFortWeapon::StaticClass()) || !((AFortWeapon*)DamageCauser)->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass()))
        return OnDamageServerOG(Actor, Damage, DamageTags, Momentum, HitInfo, InstigatedBy, DamageCauser, EffectContext);

    auto Resource = UFortKismetLibrary::K2_GetResourceItemDefinition(Actor->ResourceType);

    if (!Resource)
        return OnDamageServerOG(Actor, Damage, DamageTags, Momentum, HitInfo, InstigatedBy, DamageCauser, EffectContext);

    auto MaxMat = Resource->GetMaxStackSize(nullptr);

    if (!ResourceRates)
    {
        auto Playlist = GameState->CurrentPlaylistInfo.BasePlaylist;
        ResourceRates = Playlist ? Playlist->ResourceRates.Get() : nullptr;
    }

    if (!*ResourceRates)
        ResourceRates = FindObject<UCurveTable>(L"/Game/Athena/Balance/DataTables/AthenaResourceRates.AthenaResourceRates");

    static auto GetSparseClassData = (FBuildingSMActorClassData* (*)(UClass*, uint8))(ImageBase + 0x1BBBDC8);

    int ResCount = 0;
    auto& BuildingResourceAmountOverride = GetSparseClassData(Actor->Class, 1)->BuildingResourceAmountOverride;

    if (!BuildingResourceAmountOverride.RowName.IsNone())
    {
        float Out = 0.f;
        UDataTableFunctionLibrary::EvaluateCurveTableRow(*ResourceRates, BuildingResourceAmountOverride.RowName, 0.f, nullptr, &Out, FString());

        float RC = Out / (Actor->GetMaxHealth() / Damage);

        ResCount = (int)round(RC);
    }

    if (ResCount > 0)
    {
        auto Item = FindItemInstance(Inventory, Resource);
        auto ItemEntry = FindReplicatedEntry(Inventory, Resource);

        if (Item)
        {
            ItemEntry->Count += ResCount;
            if (ItemEntry->Count > MaxMat)
            {
                SpawnPickup(InstigatedBy->Pawn->K2_GetActorLocation(), (UFortItemDefinition*)Item->ItemEntry.ItemDefinition, ItemEntry->Count - MaxMat, 0, EFortPickupSourceTypeFlag::Tossed,
                            EFortPickupSpawnSource::Unset, InstigatorPawn);
                ItemEntry->Count = MaxMat;
            }

            for (auto& StateValue : ItemEntry->StateValues)
            {
                if (StateValue.StateType != EFortItemEntryState::ShouldShowItemToast)
                    continue;

                StateValue.IntValue = 0;
                break;
            }

            Item->ItemEntry.Count = ItemEntry->Count;
            UpdateEntry(Inventory, *ItemEntry);
            Item->ItemEntry.bIsDirty = true;
        }
        else
        {
            if (ResCount > MaxMat)
            {
                SpawnPickup(InstigatedBy->Pawn->K2_GetActorLocation(), Resource, ResCount - MaxMat, 0, EFortPickupSourceTypeFlag::Tossed, EFortPickupSpawnSource::Unset, InstigatorPawn);
                ResCount = MaxMat;
            }

            GiveItem(Inventory, Resource, ResCount, 0, 0, false);
        }
    }

    if (ResCount > 0 && Controller)
        Controller->ClientReportDamagedResourceBuilding(Actor, Actor->ResourceType, ResCount, Actor->GetHealth() - Damage <= 0, Damage == 100.f);

    Actor->ForceNetUpdate();
    return OnDamageServerOG(Actor, Damage, DamageTags, Momentum, HitInfo, InstigatedBy, DamageCauser, EffectContext);
}

void ABuildingSMActor__PostLoad()
{
    Hook(ImageBase + 0x94C7EE0, OnDamageServer, OnDamageServerOG);
}
