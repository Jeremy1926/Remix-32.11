#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"

INIT_MODULE(AFortAthenaVehicle);
INIT_POSTLOAD(AFortAthenaVehicle);

static void ApplyVehicleMod(void* VehicleInterface, const FGameplayTag* ModTag, AActor* EventInstigator)
{
    static auto GetModConfig = (UFortVehicleModConfig* (*)(void*, const FGameplayTag*))(ImageBase + 0x9BCADB8);
    static auto ApplyToVehicleInternal = (void (*)(UFortVehicleModConfig*, void*))(ImageBase + 0xA60996C);

    auto Vehicle = (AFortAthenaVehicle*)(__int64(VehicleInterface) - 0x520);

    if (!ModTag || Vehicle->Role != ENetRole::ROLE_Authority)
        return;

    auto CanApplyMod = (bool (*)(void*, const FGameplayTag*))(*(void***)VehicleInterface)[0x39];

    if (!CanApplyMod(VehicleInterface, ModTag))
        return;

    auto ModConfig = GetModConfig(VehicleInterface, ModTag);

    if (!ModConfig)
        return;

    static auto AppliedVehicleModTagsName = FName(L"AppliedVehicleModTags");

    UBlueprintGameplayTagLibrary::AddGameplayTag(Vehicle->AppliedVehicleModTags, *ModTag);
    UNetPushModelHelpers::MarkPropertyDirty(Vehicle, AppliedVehicleModTagsName);

    ApplyToVehicleInternal(ModConfig, VehicleInterface);
}

static void RemoveVehicleMod(void* VehicleInterface, const FGameplayTag* ModTag)
{
    static auto GetModConfig = (UFortVehicleModConfig* (*)(void*, const FGameplayTag*))(ImageBase + 0x9BCADB8);
    static auto RemoveFromVehicleInternal = (void (*)(UFortVehicleModConfig*, void*))(ImageBase + 0xA61B2A0);

    auto Vehicle = (AFortAthenaVehicle*)(__int64(VehicleInterface) - 0x520);

    if (!ModTag || Vehicle->Role != ENetRole::ROLE_Authority)
        return;

    if (auto ModConfig = GetModConfig(VehicleInterface, ModTag))
        RemoveFromVehicleInternal(ModConfig, VehicleInterface);

    static auto AppliedVehicleModTagsName = FName(L"AppliedVehicleModTags");

    UBlueprintGameplayTagLibrary::RemoveGameplayTag(Vehicle->AppliedVehicleModTags, *ModTag);
    UNetPushModelHelpers::MarkPropertyDirty(Vehicle, AppliedVehicleModTagsName);
}

static void (*PostInitializeComponentsOG)(AFortAthenaVehicle*);
static void PostInitializeComponents(AFortAthenaVehicle* Vehicle)
{
    static auto DamageablePartsName = FName(L"DamageableParts");

    PostInitializeComponentsOG(Vehicle);

    auto VehicleConfigs = Vehicle->FortPhysicsVehicleConfigs;

    if (Vehicle->Role != ENetRole::ROLE_Authority || !VehicleConfigs)
        return;

    Vehicle->ServerSetDestructionInfo(VehicleConfigs->SelfDestructTime, VehicleConfigs->MaxTimeZeroHealth, VehicleConfigs->MaxTimeSelfDestructCleanup, VehicleConfigs->MaxTimeExplosion);

    if (Vehicle->DamageableParts.Num() > 0)
        return;

    for (int32 i = 0; i < VehicleConfigs->DamageableParts.Num(); i++)
    {
        auto& PartConfig = VehicleConfigs->DamageableParts[i];

        FVehicleDamageablePart Part{};
        Part.ConfigIndex = i;
        Part.BoneName = PartConfig.BoneName;
        Part.ShapeName = PartConfig.ShapeName;
        Part.Health = PartConfig.bStartsDisabled ? 0.f : PartConfig.MaxHealth.Evaluate();
        Part.bConsideredCriticalHit = PartConfig.bConsideredCriticalHit;
        Part.MaxHealthOverride = -1.f;

        Vehicle->DamageableParts.Add(Part);
    }

    UNetPushModelHelpers::MarkPropertyDirty(Vehicle, DamageablePartsName);
    Vehicle->RefreshDamageableParts();
}

template <typename Fn>
static void UpdateDamageableParts(AFortAthenaVehicle* Vehicle, Fn&& Update)
{
    static auto RefreshDamageablePartsInternal = (void (*)(AFortAthenaVehicle*, const TArray<FVehicleDamageablePart>&))(ImageBase + 0x2BAF940);
    static auto DamageablePartsName = FName(L"DamageableParts");

    TArray<FVehicleDamageablePart> PrevDamageableParts;

    for (auto& PrevPart : Vehicle->DamageableParts)
        PrevDamageableParts.Add(PrevPart);

    Update();

    UNetPushModelHelpers::MarkPropertyDirty(Vehicle, DamageablePartsName);
    RefreshDamageablePartsInternal(Vehicle, PrevDamageableParts);
    PrevDamageableParts.Free();
}

static void RepairDamageablePart(AFortAthenaVehicle* Vehicle, const FName& ShapeName, float NewHealthPercent)
{
    auto VehicleConfigs = Vehicle->FortPhysicsVehicleConfigs;

    if (Vehicle->Role != ENetRole::ROLE_Authority || !VehicleConfigs)
        return;

    for (auto& Part : Vehicle->DamageableParts)
    {
        if (Part.ShapeName != ShapeName || !VehicleConfigs->DamageableParts.IsValidIndex(Part.ConfigIndex))
            continue;

        UpdateDamageableParts(Vehicle, [&]()
        {
            Part.Health = VehicleConfigs->DamageableParts[Part.ConfigIndex].MaxHealth.Evaluate() * NewHealthPercent;
        });
        break;
    }
}

static void BreakDamageablePart(AFortAthenaVehicle* Vehicle, const FName& ShapeName)
{
    RepairDamageablePart(Vehicle, ShapeName, 0.f);
}

static void (*RepairAllDamageablePartsOG)(AFortAthenaVehicle*, float);
static void RepairAllDamageableParts(AFortAthenaVehicle* Vehicle, float NewHealthPercent)
{
    auto VehicleConfigs = Vehicle->FortPhysicsVehicleConfigs;

    if (Vehicle->Role == ENetRole::ROLE_Authority && VehicleConfigs)
    {
        UpdateDamageableParts(Vehicle, [&]()
        {
            for (auto& Part : Vehicle->DamageableParts)
            {
                if (VehicleConfigs->DamageableParts.IsValidIndex(Part.ConfigIndex))
                    Part.Health = VehicleConfigs->DamageableParts[Part.ConfigIndex].MaxHealth.Evaluate() * NewHealthPercent;
            }
        });
    }

    if (Vehicle->IsA(AFortDagwoodVehicle::StaticClass()))
        RepairAllDamageablePartsOG(Vehicle, NewHealthPercent);
}

static void DestroyAllDamageableParts(UObject* Context, FFrame& Stack, void* Ret)
{
    Stack.IncrementCode();

    auto Vehicle = (AFortAthenaVehicle*)Context;

    for (auto& Part : Vehicle->DamageableParts)
        BreakDamageablePart(Vehicle, Part.ShapeName);
}

static void (*AbsorbDamageIntoDamagablePartOG)(AFortAthenaVehicle*, float&, const FGameplayTagContainer&, const FGameplayEffectContextHandle&, AController*, AActor*, AFortPlayerPawn*);
static void AbsorbDamageIntoDamagablePart(AFortAthenaVehicle* Vehicle, float& Damage, const FGameplayTagContainer& DamageTags, const FGameplayEffectContextHandle& EffectContext, AController* EventInstigator, AActor* DamageCauser, AFortPlayerPawn* InstigatorPawn)
{
    static auto GetVehicleBulletCollisionComponent = (UPrimitiveComponent* (*)(AFortAthenaVehicle*))(ImageBase + 0x5B9B1EC);
    static auto GetBodySetup = (UBodySetup* (*)(FBodyInstance*))(ImageBase + 0x180D474);
    static auto GetElement = (FKShapeElem* (*)(FKAggregateGeom*, int32))(ImageBase + 0x19D3BEC);
    static auto NeverBreakVehiclePartTag = FGameplayTag{ FName(L"Gameplay.Damage.NeverBreakVehiclePart") };
    static auto AlwaysBreakVehiclePartTag = FGameplayTag{ FName(L"Gameplay.Damage.AlwaysBreakVehiclePart") };

    auto Context = *(FFortGameplayEffectContext**)(__int64(&EffectContext) + 0x8);
    auto BulletCollisionComponent = GetVehicleBulletCollisionComponent(Vehicle);

    if (Vehicle->Role == ENetRole::ROLE_Authority && Damage > 0.f && Context && BulletCollisionComponent && !DamageTags.HasTag(NeverBreakVehiclePartTag))
    {
        FHitResult HitResult{};
        FVector HitDirection{};

        ((void (*)(FFortGameplayEffectContext*, AActor*, AActor*, FHitResult*, FVector*))(*(void***)Context)[0x19])(Context, Vehicle, EventInstigator, &HitResult, &HitDirection);

        auto BodyInstance = HitResult.Component.Get() == BulletCollisionComponent ? ((FBodyInstance* (*)(UPrimitiveComponent*, FName, bool, int32))BulletCollisionComponent->VTable[0xFA])(BulletCollisionComponent, FName(), true, -1) : nullptr;
        auto BodySetup = BodyInstance ? GetBodySetup(BodyInstance) : nullptr;
        auto ShapeElem = BodySetup ? GetElement(&BodySetup->AggGeom, HitResult.ElementIndex) : nullptr;

        for (int32 i = 0; ShapeElem && i < Vehicle->DamageableParts.Num(); i++)
        {
            auto& Part = Vehicle->DamageableParts[i];

            if (Part.ShapeName != ShapeElem->Name || Part.Health <= 0.f)
                continue;

            UpdateDamageableParts(Vehicle, [&]()
            {
                auto AbsorbedDamage = (std::min)(Damage, Part.Health);

                Damage = (std::max)(Damage - Part.Health, 0.f);
                Part.Health = DamageTags.HasTag(AlwaysBreakVehiclePartTag) ? 0.f : Part.Health - AbsorbedDamage;

                if (AbsorbedDamage > 0.f && InstigatorPawn && InstigatorPawn->Controller && InstigatorPawn->Controller->IsA(AFortPlayerController::StaticClass()))
                    ((AFortPlayerController*)InstigatorPawn->Controller)->ClientAddDamageNumber(AbsorbedDamage, Vehicle, InstigatorPawn, EffectContext, DamageTags, Context->bIsCriticalHit, HitResult.ImpactPoint, HitResult.ImpactNormal);
            });
            break;
        }
    }

    if (Vehicle->IsA(AFortDagwoodVehicle::StaticClass()))
        AbsorbDamageIntoDamagablePartOG(Vehicle, Damage, DamageTags, EffectContext, EventInstigator, DamageCauser, InstigatorPawn);
}

static void PatchIfReturnStub(void** Slot, void* Detour)
{
    if (*(uint8*)*Slot == 0xC3 || *(uint8*)*Slot == 0xC2)
        Patch<void*>(uint64(Slot), Detour);
}

static void PatchVehicleVtables()
{
    const std::pair<uint32, void*> DamageablePartVirtuals[] = {
        { 0x231, (void*)BreakDamageablePart },
        { 0x232, (void*)RepairDamageablePart },
        { 0x233, (void*)RepairAllDamageableParts },
        { 0x23A, (void*)AbsorbDamageIntoDamagablePart }
    };

    for (int32 i = 0; i < UObject::GObjects->Num(); i++)
    {
        auto Object = UObject::GObjects->GetByIndex(i);

        if (!Object || !Object->IsDefaultObject() || !Object->IsA(AFortAthenaVehicle::StaticClass()))
            continue;

        for (auto& [Index, Detour] : DamageablePartVirtuals)
            PatchIfReturnStub(&Object->VTable[Index], Detour);

        auto VehicleInterface = (uint8*)Object->GetInterface(IFortVehicleInterface::StaticClass());

        if (VehicleInterface != (uint8*)Object + 0x520)
            continue;

        auto InterfaceVft = *(void***)VehicleInterface;

        PatchIfReturnStub(&InterfaceVft[0x3B], (void*)RemoveVehicleMod);
        PatchIfReturnStub(&InterfaceVft[0x3A], (void*)ApplyVehicleMod);
    }
}

void AFortAthenaVehicle__Init()
{
    ExecHook(AFortAthenaVehicle::StaticClass()->FindFunction("DestroyAllDamageableParts"), DestroyAllDamageableParts);

    PatchVehicleVtables();

    Hook(ImageBase + 0x14E2328, PostInitializeComponents, PostInitializeComponentsOG);
    Hook(ImageBase + 0xB525578, AbsorbDamageIntoDamagablePart, AbsorbDamageIntoDamagablePartOG);
    Hook(ImageBase + 0x2BAF42C, RepairAllDamageableParts, RepairAllDamageablePartsOG);
}

void AFortAthenaVehicle__PostLoad()
{
    PatchVehicleVtables();
}
