#include "pch.h"
#include "Shared.h"

INIT_POSTLOAD(AFortWeaponRanged);

static UGameplayAbility* GetFirstValidInstance(TArray<UGameplayAbility*>& Instances)
{
    for (auto& Instance : Instances)
    {
        if (Instance)
            return Instance;
    }

    return nullptr;
}

static void ServerNotifyPawnHit(UObject* Context, FFrame& Stack, void* Ret)
{
    FHitResult Hit{};
    FVector ProjectileOriginPosition;
    float ProjectileStartTimestamp;
    TArray<uint8> ProjectileContext;
    FVector LocalSpaceImpactPoint;
    FVector LocalSpaceImpactNormal;
    bool bWasTargetingWhenProjectileFired;
    UObject* HitEntity;
    Stack.StepCompiledIn(&Hit);
    Stack.StepCompiledIn(&ProjectileOriginPosition);
    Stack.StepCompiledIn(&ProjectileStartTimestamp);
    Stack.StepCompiledIn(&ProjectileContext);
    Stack.StepCompiledIn(&LocalSpaceImpactPoint);
    Stack.StepCompiledIn(&LocalSpaceImpactNormal);
    Stack.StepCompiledIn(&bWasTargetingWhenProjectileFired);
    Stack.StepCompiledIn(&HitEntity);
    Stack.IncrementCode();

    ProjectileContext.Free();

    auto Weapon = (AFortWeaponRanged*)Context;

    if (!Hit.HitObjectHandle.ReferenceObject.Get())
        return;

    auto Pawn = Weapon->Owner && Weapon->Owner->IsA(AFortPlayerPawnAthena::StaticClass()) ? (AFortPlayerPawnAthena*)Weapon->Owner : nullptr;

    if (!Pawn || !Pawn->PlayerState)
        return;

    auto PlayerState = Pawn->PlayerState->IsA(AFortPlayerStateAthena::StaticClass()) ? (AFortPlayerStateAthena*)Pawn->PlayerState : nullptr;

    if (!PlayerState || !PlayerState->AbilitySystemComponent)
        return;

    int32 Level = 1;
    UGameplayAbility* Ability = nullptr;

    for (auto& Spec : PlayerState->AbilitySystemComponent->ActivatableAbilities.Items)
    {
        if (Spec.Handle.Handle != Weapon->PrimaryAbilitySpecHandle.Handle)
            continue;

        Level = Spec.Level;
        Ability = GetFirstValidInstance(Spec.NonReplicatedInstances);

        if (!Ability)
            Ability = GetFirstValidInstance(Spec.ReplicatedInstances);

        if (!Ability)
            Ability = Spec.ability;

        break;
    }

    if (Ability)
    {
        static auto HitApplicationTag = FGameplayTag{ FName(L"Gameplay.Application.Hit") };
        auto TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(Hit);
        ((UFortGameplayAbility*)Ability)->ApplyGameplayEffectContainer(TargetData, HitApplicationTag, Level);
    }
}

static void ServerSetScopedThirdPersonTargeting(UObject* Context, FFrame& Stack, void* Ret)
{
    uint8 bNewUseScopedThirdPersonTargeting;
    Stack.StepCompiledIn(&bNewUseScopedThirdPersonTargeting);
    Stack.IncrementCode();

    auto Weapon = (AFortWeaponRanged*)Context;

    if (!Weapon->OpticDefinition || !Weapon->OpticDefinition->bUsesFirstPersonAiming)
        return;

    static auto bUseScopedThirdPersonTargetingName = FName(L"bUseScopedThirdPersonTargeting");
    Weapon->bUseScopedThirdPersonTargeting = bNewUseScopedThirdPersonTargeting != 0;
    UNetPushModelHelpers::MarkPropertyDirty(Weapon, bUseScopedThirdPersonTargetingName);
    Weapon->SetFirstPersonTargeting(Weapon->CanUseScopedCameraViaOptic());
}

void AFortWeaponRanged__PostLoad()
{
    ExecHook(AFortWeaponRanged::StaticClass()->FindFunction("ServerNotifyPawnHit"), ServerNotifyPawnHit);
    ExecHook(AFortWeaponRanged::StaticClass()->FindFunction("ServerSetScopedThirdPersonTargeting"), ServerSetScopedThirdPersonTargeting);
}
