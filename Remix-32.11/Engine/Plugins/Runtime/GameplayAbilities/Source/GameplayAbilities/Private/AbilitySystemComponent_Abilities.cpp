#include "pch.h"
#include "Shared.h"

INIT_MODULE(UAbilitySystemComponent);

static void InternalServerTryActivateAbility(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAbilitySpecHandle Handle, bool InputPressed, FPredictionKey* PredictionKey, void* TriggerEventData)
{
    static auto InternalTryActivateAbility = (bool (*)(UAbilitySystemComponent*, FGameplayAbilitySpecHandle, FPredictionKey, UGameplayAbility**, void*, void*))(ImageBase + 0x7CBB438);

    FGameplayAbilitySpec* Spec = nullptr;

    for (auto& Item : AbilitySystemComponent->ActivatableAbilities.Items)
    {
        if (Item.Handle.Handle == Handle.Handle)
        {
            Spec = &Item;
            break;
        }
    }

    if (!Spec)
        return AbilitySystemComponent->ClientActivateAbilityFailed(Handle, PredictionKey->Current);

    Spec->InputPressed = true;

    UGameplayAbility* InstancedAbility = nullptr;

    if (!InternalTryActivateAbility(AbilitySystemComponent, Handle, *PredictionKey, &InstancedAbility, nullptr, TriggerEventData))
    {
        AbilitySystemComponent->ClientActivateAbilityFailed(Handle, PredictionKey->Current);
        Spec->InputPressed = false;
        AbilitySystemComponent->ActivatableAbilities.MarkItemDirty(*Spec);
    }
}

void UAbilitySystemComponent__Init()
{
    HookEvery<UAbilitySystemComponent>(0x11D, InternalServerTryActivateAbility);
}
