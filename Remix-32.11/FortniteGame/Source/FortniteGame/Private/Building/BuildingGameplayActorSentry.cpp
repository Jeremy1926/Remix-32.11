#include "pch.h"
#include "Shared.h"

INIT_MODULE(ABuildingGameplayActorSentry);

static void (*TickOG)(ABuildingGameplayActorSentry*, float);
static void (*BeginPlayOG)(ABuildingGameplayActorSentry*);
static void (*HandleDamagedOG)(ABuildingGameplayActorSentry*, float, const void*, const void*, const void*, AActor*, AActor*);

static std::unordered_map<ABuildingGameplayActorSentry*, float> AggroTimeRemaining;

template <class T>
static void SetReplicatedProperty(ABuildingGameplayActorSentry* Sentry, T& Property, const std::type_identity_t<T>& Value, const FName& PropertyName)
{
    Sentry->FlushNetDormancy();
    Property = Value;
    UNetPushModelHelpers::MarkPropertyDirty(Sentry, PropertyName);
}

static void SetSentryState(ABuildingGameplayActorSentry* Sentry, EBuildingGameplayActorSentry_State InSentryState)
{
    static auto CurrentSentryStateName = FName(L"CurrentSentryState");

    auto& ReturnToIdleTimerHandle = *(FTimerHandle*)(__int64(Sentry) + 0xC78);

    if (ReturnToIdleTimerHandle.Handle)
        UKismetSystemLibrary::K2_ClearAndInvalidateTimerHandle(Sentry, ReturnToIdleTimerHandle);

    ReturnToIdleTimerHandle.Handle = 0;

    auto PreviousSentryState = Sentry->CurrentSentryState;

    SetReplicatedProperty(Sentry, Sentry->CurrentSentryState, InSentryState, CurrentSentryStateName);

    Sentry->BPOnSentryStateChanged(PreviousSentryState);
}

static void SetActorToTrack(ABuildingGameplayActorSentry* Sentry, AActor* InActorToTrack)
{
    static auto ActorBeingTrackedName = FName(L"ActorBeingTracked");

    auto TrackedActorPrevious = Sentry->ActorBeingTracked;

    if (TrackedActorPrevious == InActorToTrack)
        return;

    SetReplicatedProperty(Sentry, Sentry->ActorBeingTracked, InActorToTrack, ActorBeingTrackedName);

    auto State = Sentry->CurrentSentryState;

    if (!InActorToTrack)
        AggroTimeRemaining.erase(Sentry);
    else if (State != EBuildingGameplayActorSentry_State::Aggro && State != EBuildingGameplayActorSentry_State::Dormant && State != EBuildingGameplayActorSentry_State::Deactivated)
    {
        auto Multiplier = Sentry->TrackedToAggroDelayMultiplier;
        auto Delay = Sentry->TrackedToAggroDelay.Evaluate(0.f) * (Multiplier > 0.f ? Multiplier : 0.f);

        if (State != EBuildingGameplayActorSentry_State::Tracking)
        {
            SetSentryState(Sentry, EBuildingGameplayActorSentry_State::Tracking);
            Sentry->K2_ExecuteGameplayCue(Sentry->TrackedGameplayCueTag, FGameplayEffectContextHandle());
            AggroTimeRemaining[Sentry] = Delay;
        }
        else if (!AggroTimeRemaining.contains(Sentry))
            AggroTimeRemaining[Sentry] = Delay;
    }

    Sentry->BPOnTrackedActorSet(TrackedActorPrevious);
}

static void SetFixedRotationGoalForReplication(ABuildingGameplayActorSentry* Sentry, const FRotator& InFixedRotationGoal)
{
    static auto FixedRotationGoalReplicatedName = FName(L"FixedRotationGoalReplicated");

    SetReplicatedProperty(Sentry, Sentry->FixedRotationGoalReplicated, InFixedRotationGoal, FixedRotationGoalReplicatedName);

    *(FRotator*)(__int64(Sentry) + 0xBE0) = InFixedRotationGoal;
}

static void SetStartingIdleRotation(ABuildingGameplayActorSentry* Sentry, const FRotator& InStartingIdleRotation)
{
    static auto StartingIdleRotationName = FName(L"StartingIdleRotation");

    SetReplicatedProperty(Sentry, Sentry->StartingIdleRotation, InStartingIdleRotation, StartingIdleRotationName);
}

static void SelectClosestIdleRotation(ABuildingGameplayActorSentry* Sentry)
{
    if (!Sentry->bDoesNotRotate)
        SetFixedRotationGoalForReplication(Sentry, Sentry->TargetRotationRight);
}

static void IdleRestart(ABuildingGameplayActorSentry* Sentry)
{
    Sentry->bIdleRotationStopped = false;

    SetFixedRotationGoalForReplication(Sentry, Sentry->bIdleRotatingRight ? Sentry->TargetRotationRight : Sentry->TargetRotationLeft);
}

static void ReturnToIdle(ABuildingGameplayActorSentry* Sentry)
{
    auto State = Sentry->CurrentSentryState;

    if (State != EBuildingGameplayActorSentry_State::ReturningToIdle && State != EBuildingGameplayActorSentry_State::LocatingDamager)
        return;

    SelectClosestIdleRotation(Sentry);

    if (Sentry->TargetActorsInRange.Num() > 0)
        SetSentryState(Sentry, EBuildingGameplayActorSentry_State::ActiveIdle);
    else if (Sentry->bGoDormantAfterPassiveIdle)
        SetSentryState(Sentry, EBuildingGameplayActorSentry_State::Dormant);
    else
        SetSentryState(Sentry, EBuildingGameplayActorSentry_State::PassiveIdle);

    Sentry->K2_ExecuteGameplayCue(Sentry->ReturnToIdleGameplayCueTag, FGameplayEffectContextHandle());
}

static void Tick(ABuildingGameplayActorSentry* Sentry, float DeltaSeconds)
{
    TickOG(Sentry, DeltaSeconds);

    auto AggroTimer = AggroTimeRemaining.find(Sentry);

    if (AggroTimer == AggroTimeRemaining.end())
        return;

    if (Sentry->Role != ENetRole::ROLE_Authority || Sentry->CurrentSentryState != EBuildingGameplayActorSentry_State::Tracking || !Sentry->ActorBeingTracked)
    {
        AggroTimeRemaining.erase(AggroTimer);
        return;
    }

    AggroTimer->second -= DeltaSeconds;

    if (AggroTimer->second > 0.f)
        return;

    AggroTimeRemaining.erase(AggroTimer);

    SetSentryState(Sentry, EBuildingGameplayActorSentry_State::Aggro);
    Sentry->K2_ExecuteGameplayCue(Sentry->AggroedGameplayCueTag, FGameplayEffectContextHandle());
}

static void BeginPlay(ABuildingGameplayActorSentry* Sentry)
{
    BeginPlayOG(Sentry);

    AggroTimeRemaining.erase(Sentry);

    if (Sentry->Role != ENetRole::ROLE_Authority)
        return;

    auto IdleRotationRenderGate = (uint8*)(ImageBase + 0x12C5E30C);

    if (*IdleRotationRenderGate)
        *IdleRotationRenderGate = 0;

    SetStartingIdleRotation(Sentry, Sentry->TargetRotation);
    IdleRestart(Sentry);
}

static void HandleDamaged(ABuildingGameplayActorSentry* Sentry, float Damage, const void* DamageTags, const void* EffectSpec, const void* EffectContext, AActor* EventInstigator, AActor* DamageCauser)
{
    HandleDamagedOG(Sentry, Damage, DamageTags, EffectSpec, EffectContext, EventInstigator, DamageCauser);

    if (!Sentry->bLocateDamagerIfHitWhileIdle || !DamageCauser || Sentry->Role != ENetRole::ROLE_Authority)
        return;

    auto State = Sentry->CurrentSentryState;

    if (State > EBuildingGameplayActorSentry_State::ActiveIdle && State != EBuildingGameplayActorSentry_State::ReturningToIdle)
        return;

    SetSentryState(Sentry, EBuildingGameplayActorSentry_State::LocatingDamager);

    auto Camera = Sentry->CameraStaticMeshComp;
    auto CameraLocation = Camera ? Camera->K2_GetComponentToWorld().Translation : FVector();
    auto Direction = DamageCauser->K2_GetActorLocation() - CameraLocation;
    auto Normal = Direction;
    auto SquareSum = Direction.X * Direction.X + Direction.Y * Direction.Y + Direction.Z * Direction.Z;

    if (SquareSum != 1.0)
    {
        if (SquareSum < double(1.e-4f))
            Normal = FVector();
        else
            Normal = Direction * (1.0 / sqrt(SquareSum));
    }

    FRotator LookRotation{};

    LookRotation.Yaw = atan2(Normal.Y, Normal.X) * 180.0 / 3.14159265358979323846;
    LookRotation.Pitch = atan2(Normal.Z, sqrt(Normal.X * Normal.X + Normal.Y * Normal.Y)) * 180.0 / 3.14159265358979323846;
    LookRotation.Roll = 0.0;

    double PitchClampMin = Sentry->SentryPitchClampMin;
    double PitchClampMax = Sentry->SentryPitchClampMax;
    double Pitch = LookRotation.Pitch;

    LookRotation.Pitch = Pitch >= PitchClampMin ? (PitchClampMax < Pitch ? PitchClampMax : Pitch) : PitchClampMin;

    SetFixedRotationGoalForReplication(Sentry, LookRotation);
}

static void SetSentryState(UObject* Context, FFrame& Stack)
{
    EBuildingGameplayActorSentry_State InSentryState{};

    Stack.StepCompiledIn(&InSentryState);
    Stack.IncrementCode();

    SetSentryState((ABuildingGameplayActorSentry*)Context, InSentryState);
}

static void SetActorToTrack(UObject* Context, FFrame& Stack)
{
    AActor* InActorToTrack = nullptr;

    Stack.StepCompiledIn(&InActorToTrack);
    Stack.IncrementCode();

    SetActorToTrack((ABuildingGameplayActorSentry*)Context, InActorToTrack);
}

static void SetIdleBehavior(UObject* Context, FFrame& Stack)
{
    static auto IdleBehaviorName = FName(L"IdleBehavior");

    EBuildingGameplayActorSentry_IdleBehavior NewIdleBehavior{};

    Stack.StepCompiledIn(&NewIdleBehavior);
    Stack.IncrementCode();

    auto Sentry = (ABuildingGameplayActorSentry*)Context;

    SetReplicatedProperty(Sentry, Sentry->IdleBehavior, NewIdleBehavior, IdleBehaviorName);
}

static void SelectClosestIdleRotation(UObject* Context, FFrame& Stack)
{
    Stack.IncrementCode();

    SelectClosestIdleRotation((ABuildingGameplayActorSentry*)Context);
}

static void SetFixedRotationGoalForReplication(UObject* Context, FFrame& Stack)
{
    FRotator InFixedRotationGoalTemp{};
    auto& InFixedRotationGoal = Stack.StepCompiledInRef(&InFixedRotationGoalTemp);

    Stack.IncrementCode();

    SetFixedRotationGoalForReplication((ABuildingGameplayActorSentry*)Context, InFixedRotationGoal);
}

static void SetStartingIdleRotation(UObject* Context, FFrame& Stack)
{
    FRotator InStartingIdleRotationTemp{};
    auto& InStartingIdleRotation = Stack.StepCompiledInRef(&InStartingIdleRotationTemp);

    Stack.IncrementCode();

    SetStartingIdleRotation((ABuildingGameplayActorSentry*)Context, InStartingIdleRotation);
}

static void SetEmpApplied(UObject* Context, FFrame& Stack)
{
    bool ApplyEMP = false;

    Stack.StepCompiledIn(&ApplyEMP);
    Stack.IncrementCode();

    auto Sentry = (ABuildingGameplayActorSentry*)Context;

    if (ApplyEMP)
        SetSentryState(Sentry, EBuildingGameplayActorSentry_State::Deactivated);
    else
        Sentry->TryReactivate();
}

void ABuildingGameplayActorSentry__Init()
{
    auto SentryClass = ABuildingGameplayActorSentry::StaticClass();

    Hook<ABuildingGameplayActorSentry>(uint32(0xA0), Tick, TickOG);
    Hook<ABuildingGameplayActorSentry>(uint32(0x74), BeginPlay, BeginPlayOG);
    Hook<ABuildingGameplayActorSentry>(uint32(0x139), HandleDamaged, HandleDamagedOG);

    Rel32Hook(ImageBase + 0x94F6FD4 + 3, ReturnToIdle);
    Rel32Hook(ImageBase + 0x94E8CA4 + 3, ReturnToIdle);
    Rel32Hook(ImageBase + 0x5ACEECA + 3, IdleRestart);

    ExecHook(SentryClass->FindFunction("SetSentryState"), (void (*)(UObject*, FFrame&))SetSentryState);
    ExecHook(SentryClass->FindFunction("SetActorToTrack"), (void (*)(UObject*, FFrame&))SetActorToTrack);
    ExecHook(SentryClass->FindFunction("SetIdleBehavior"), SetIdleBehavior);
    ExecHook(SentryClass->FindFunction("SelectClosestIdleRotation"), (void (*)(UObject*, FFrame&))SelectClosestIdleRotation);
    ExecHook(SentryClass->FindFunction("SetFixedRotationGoalForReplication"), (void (*)(UObject*, FFrame&))SetFixedRotationGoalForReplication);
    ExecHook(SentryClass->FindFunction("SetStartingIdleRotation"), (void (*)(UObject*, FFrame&))SetStartingIdleRotation);
    ExecHook(SentryClass->FindFunction("SetEmpApplied"), SetEmpApplied);
}
