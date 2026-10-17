#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Quests/FortQuestManager.h"

INIT_POSTLOAD(AFortPawn);

static void (*EmoteStoppedOG)(UObject*, FFrame&, void*);

static void MovingEmoteStopped(UObject* Context, FFrame& Stack)
{
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;
    auto OldEmote = Pawn->LastReplicatedEmoteExecuted;

    Pawn->bMovingEmote = false;
    Pawn->bMovingEmoteForwardOnly = false;
    Pawn->bMovingEmoteFollowingOnly = false;
    Pawn->LastReplicatedEmoteExecuted = nullptr;
    Pawn->OnRep_LastReplicatedEmoteExecuted(OldEmote);
}

static void EmoteStopped(UObject* Context, FFrame& Stack)
{
    UFortItemDefinition* MontageItemDef;

    Stack.StepCompiledIn(&MontageItemDef);
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (Pawn->LastReplicatedEmoteExecuted == MontageItemDef)
    {
        Pawn->LastReplicatedEmoteExecuted = nullptr;
        Pawn->OnRep_LastReplicatedEmoteExecuted(MontageItemDef);
    }

    callOG(Pawn, Stack.CurrentNativeFunction, EmoteStopped, MontageItemDef);
}

static void (*OnDamageServerOG)(AFortPawn*, float, const FGameplayTagContainer&, const FVector&, const FHitResult&, AController*, AActor*, const FGameplayEffectContextHandle&);
static void OnDamageServer(AFortPawn* Pawn, float Damage, const FGameplayTagContainer& DamageTags, const FVector& Momentum, const FHitResult& HitInfo, AController* InstigatedBy, AActor* DamageCauser,
                           const FGameplayEffectContextHandle& EffectContext)
{
    auto Context = *(FFortGameplayEffectContext**)(__int64(&EffectContext) + 0x8);
    bool bCriticalHit = Context && Context->bIsCriticalHit;

    FPendingDamageTaken Pending{};

    if (PendingDamageTaken.Victim.Get() == Pawn)
        std::swap(Pending, PendingDamageTaken);

    OnDamageServerOG(Pawn, Damage, DamageTags, Momentum, HitInfo, InstigatedBy, DamageCauser, EffectContext);

    auto PlayerController = ResolvePlayerController(InstigatedBy);

    if (Pending.Victim.Get() == Pawn && PlayerController && ResolvePlayerController(Pending.DamageSource.Get()) == PlayerController)
        SendDamageVerbs(PlayerController, Pawn, Pending, Damage, bCriticalHit);

    FreeGameplayTags(Pending.DamageSourceTags);
    FreeGameplayTags(Pending.DamageTypeTags);
}

void AFortPawn__PostLoad()
{
    auto PawnClass = AFortPlayerPawnAthena::StaticClass();

    ExecHook(PawnClass->FindFunction("MovingEmoteStopped"), MovingEmoteStopped);
    ExecHook(PawnClass->FindFunction("EmoteStopped"), EmoteStopped, EmoteStoppedOG);

    Hook(ImageBase + 0xA150DFC, OnDamageServer, OnDamageServerOG);
}
