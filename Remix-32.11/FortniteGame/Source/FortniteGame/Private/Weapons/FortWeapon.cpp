#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Weapons/FortWeapon.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"

INIT_POSTLOAD(AFortWeapon);

static void (*K2_OnUnEquipOG)(AFortWeapon*);
static void K2_OnUnEquip(AFortWeapon* Weapon)
{
    RemoveWeaponAbilities(Weapon);

    K2_OnUnEquipOG(Weapon);
}

UFortWeaponModSetData* GetRandomWeaponModSetData(const UObject* WorldContext, const UFortWeaponItemDefinition* WeaponItemDefinition)
{
    if (!WorldContext || !WeaponItemDefinition)
        return nullptr;

    auto WeaponModManager = (UFortWeaponModManager*)USubsystemBlueprintLibrary::GetWorldSubsystem((UObject*)WorldContext, UFortWeaponModManager::StaticClass());
    auto AutoApplyModData = WeaponModManager ? WeaponModManager->AutoApplyModDataAsset : nullptr;

    if (!AutoApplyModData || !AutoApplyModData->ModSetDataTable)
        return nullptr;

    auto Interface = WeaponItemDefinition->GetInterface(IGameplayTagAssetInterface::StaticClass());

    if (!Interface)
        return nullptr;

    FGameplayTagContainer OwnedTags{};
    ((void (*)(void*, FGameplayTagContainer*))(*(void***)Interface)[0x2])(Interface, &OwnedTags);

    std::vector<FFortWeaponModSetRow*> BucketEntries;
    float TotalWeight = 0.f;

    for (auto& Pair : AutoApplyModData->ModSetDataTable->RowMap)
    {
        auto ModSetRow = (FFortWeaponModSetRow*)Pair.Value();

        if (!ModSetRow || !ModSetRow->Set || ModSetRow->RandomWeight <= 0.f)
            continue;

        for (auto& Tag : OwnedTags.GameplayTags)
        {
            if (Tag.TagName.ComparisonIndex == ModSetRow->BucketTag.TagName.ComparisonIndex)
            {
                BucketEntries.push_back(ModSetRow);
                TotalWeight += ModSetRow->RandomWeight;
                break;
            }
        }
    }

    FreeGameplayTags(OwnedTags);

    if (BucketEntries.empty())
        return nullptr;

    auto RandomNumber = ((float)rand() / 32767.f) * TotalWeight;

    for (auto& BucketEntry : BucketEntries)
    {
        if (RandomNumber <= BucketEntry->RandomWeight)
            return BucketEntry->Set;

        RandomNumber -= BucketEntry->RandomWeight;
    }

    return BucketEntries.back()->Set;
}

void ApplyModSetToPickup(const UFortWeaponModSetData* ModSet, AFortPickup* WeaponPickup)
{
    if (!ModSet || !WeaponPickup)
        return;

    auto& PickupEntry = WeaponPickup->MultiItemPickupEntries.Num() > 0 ? WeaponPickup->MultiItemPickupEntries[0] : WeaponPickup->PrimaryPickupItemEntry;
    auto WeaponDefinition = PickupEntry.ItemDefinition && PickupEntry.ItemDefinition->IsA(UFortWeaponItemDefinition::StaticClass()) ? (UFortWeaponItemDefinition*)PickupEntry.ItemDefinition : nullptr;

    if (!WeaponDefinition)
        return;

    const TArray<FFortModSetModEntry>* Mods = nullptr;

    switch (WeaponDefinition->Rarity)
    {
    case EFortRarity::Common:
        Mods = &ModSet->CommonRarityMods;
        break;
    case EFortRarity::Uncommon:
        Mods = &ModSet->UncommonRarityMods;
        break;
    case EFortRarity::Rare:
        Mods = &ModSet->RareRarityMods;
        break;
    case EFortRarity::Epic:
        Mods = &ModSet->EpicRarityMods;
        break;
    case EFortRarity::Legendary:
        Mods = &ModSet->LegendaryRarityMods;
        break;
    }

    if (!Mods)
        return;

    bool bApplied = false;

    for (auto& ModEntry : *Mods)
    {
        if (ModEntry.ModDefinition && UFortWeaponModFunctionLibrary::ApplyWeaponModToPickup(WeaponPickup, ModEntry.ModDefinition))
            bApplied = true;
    }

    if (bApplied)
    {
        static auto ModSetStateType = FGameplayTag{ FName(L"ItemStateValue.ConstObject.ModSet") };
        SetStateValueConstObject(PickupEntry, ModSetStateType, ModSet);
    }
}

void AFortWeapon__PostLoad()
{
    Hook(ImageBase + 0xA684F84, K2_OnUnEquip, K2_OnUnEquipOG);
}
