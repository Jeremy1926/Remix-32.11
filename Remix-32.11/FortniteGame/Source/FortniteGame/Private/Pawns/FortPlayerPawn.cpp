#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Pawns/FortPlayerPawn.h"
#include "Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/GameplayCueInterface.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"
#include "FortniteGame/Source/FortniteGame/Public/Quests/FortQuestManager.h"

INIT_POSTLOAD(AFortPlayerPawn);
INIT_TICKER(AFortPlayerPawn);

static void (*OnCapsuleBeginOverlapOG)(UObject*, FFrame&, void*);
static void (*ServerOnExitVehicleOG)(UObject*, FFrame&, void*);

static std::vector<std::pair<TWeakObjectPtr<AFortPlayerPawnAthena>, TWeakObjectPtr<AActor>>> DBNORevivePawns;
static std::optional<UClass*> CachedZiplineClass;
static std::optional<UClass*> CachedEmoteAbilityClass;

static AFortPlayerStateAthena* GetPlayerStateAthena(APawn* Pawn)
{
    return Pawn->PlayerState && Pawn->PlayerState->IsA(AFortPlayerStateAthena::StaticClass()) ? (AFortPlayerStateAthena*)Pawn->PlayerState : nullptr;
}

static void ServerHandlePickupInfo(UObject* Context, FFrame& Stack)
{
    AFortPickupAthena* Pickup;
    FFortPickupRequestInfo Params;

    Stack.StepCompiledIn(&Pickup);
    Stack.StepCompiledIn(&Params);
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (!Pawn || !Pickup || Pickup->bPickedUp)
        return;

    if (Params.bUseRequestedSwap && Pawn->Controller && Pawn->Controller->IsA(AFortPlayerControllerAthena::StaticClass()) && Pawn->CurrentWeapon && IsPrimaryQuickbar(Pawn->CurrentWeapon->WeaponData) &&
        IsPrimaryQuickbar((UFortItemDefinition*)Pickup->PrimaryPickupItemEntry.ItemDefinition))
        ((AFortPlayerControllerAthena*)Pawn->Controller)->bTryPickupSwap = true;

    static auto SetPickupTarget = (void (*)(AFortPickupAthena*, AFortPlayerPawnAthena*, float, FVector&, bool))(ImageBase + 0x9CDB4C8);
    SetPickupTarget(Pickup, Pawn, Params.FlyTime / Pawn->PickupSpeedMultiplier, Params.Direction, Params.bPlayPickupSound);
}

static void ServerSendZiplineState(UObject* Context, FFrame& Stack)
{
    FZiplinePawnState State;

    Stack.StepCompiledIn(&State);
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (!Pawn)
        return;

    auto Zipline = Pawn->GetActiveZipline();
    auto PreviousState = Pawn->ZiplineState;

    Pawn->ZiplineState = State;

    static auto OnRep_ZiplineState = (void (*)(AFortPlayerPawnAthena*))(ImageBase + 0xA190360);
    OnRep_ZiplineState(Pawn);

    auto NewZipline = Pawn->GetActiveZipline();

    if (State.bIsZiplining && (!PreviousState.bIsZiplining || NewZipline != Zipline))
        Pawn->ZiplinePostBegin.Process();

    if (State.bJumped)
    {
        auto VelocityX = Pawn->CharacterMovement->Velocity.X * -0.5f;
        auto VelocityY = Pawn->CharacterMovement->Velocity.Y * -0.5f;

        Pawn->LaunchCharacterJump(FVector{ (std::clamp)(VelocityX, -750., 750.), (std::clamp)(VelocityY, -750., 750.), 1200 }, false, false, true, true);
    }

    auto ZiplineClass = FindCachedObject(CachedZiplineClass, L"/Ascender/Gameplay/Ascender/B_Athena_Zipline_Ascender.B_Athena_Zipline_Ascender_C");

    if (Zipline && Zipline->IsA(ZiplineClass))
    {
        auto Ascender = (AFortAscenderZipline*)Zipline;
        Ascender->PawnUsingHandle = nullptr;
        Ascender->PreviousPawnUsingHandle = Pawn;
        Ascender->OnRep_PawnUsingHandle();
    }
    else if (NewZipline && NewZipline->IsA(ZiplineClass))
    {
        auto Ascender = (AFortAscenderZipline*)NewZipline;
        Ascender->PawnUsingHandle = Pawn;
        Ascender->PreviousPawnUsingHandle = nullptr;
        Ascender->OnRep_PawnUsingHandle();
    }
}

static void OnCapsuleBeginOverlap(UObject* Context, FFrame& Stack)
{
    UPrimitiveComponent* OverlappedComp;
    AActor* OtherActor;
    UPrimitiveComponent* OtherComp;
    int32 OtherBodyIndex;
    bool bFromSweep;
    FHitResult SweepResult;

    Stack.StepCompiledIn(&OverlappedComp);
    Stack.StepCompiledIn(&OtherActor);
    Stack.StepCompiledIn(&OtherComp);
    Stack.StepCompiledIn(&OtherBodyIndex);
    Stack.StepCompiledIn(&bFromSweep);
    Stack.StepCompiledIn(&SweepResult);
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;
    auto PlayerController = Pawn && Pawn->Controller && Pawn->Controller->IsA(AFortPlayerController::StaticClass()) ? (AFortPlayerControllerAthena*)Pawn->Controller : nullptr;
    auto Pickup = PlayerController && OtherActor && OtherActor->IsA(AFortPickupAthena::StaticClass()) ? (AFortPickupAthena*)OtherActor : nullptr;
    auto ItemDefinition = Pickup ? (UFortItemDefinition*)Pickup->PrimaryPickupItemEntry.ItemDefinition : nullptr;
    auto WorldInventory = PlayerController ? (AFortInventory*)PlayerController->WorldInventory.ObjectPointer : nullptr;

    if (ItemDefinition && WorldInventory && Pickup->PawnWhoDroppedPickup.Get() != Pawn)
    {
        auto MaxStack = ItemDefinition->GetMaxStackSize(nullptr);
        FFortItemEntry* ItemEntry = nullptr;

        for (auto& Entry : WorldInventory->Inventory.ReplicatedEntries)
        {
            if (Entry.ItemDefinition == ItemDefinition && Entry.Count <= MaxStack)
            {
                ItemEntry = &Entry;
                break;
            }
        }

        if (ItemEntry ? ItemEntry->Count < MaxStack : !IsPrimaryQuickbar(ItemDefinition))
            Pawn->ServerHandlePickup(Pickup, Pickup->PickupLocationData.FlyTime, FVector(), true);
    }

    callOG(Pawn, Stack.CurrentNativeFunction, OnCapsuleBeginOverlap, OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}

static void ServerOnExitVehicle(UObject* Context, FFrame& Stack, AFortAthenaVehicle** Ret)
{
    FVehicleExitData VehicleExitData;

    Stack.StepCompiledIn(&VehicleExitData);
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;
    auto Vehicle = Pawn->GetVehicleActor();

    if (!Vehicle && Pawn->IsA(AFortCharacterVehicle::StaticClass()))
        Vehicle = Pawn;

    auto SeatWeaponComponent = Vehicle ? (UFortVehicleSeatWeaponComponent*)Vehicle->GetComponentByClass(UFortVehicleSeatWeaponComponent::StaticClass()) : nullptr;

    if (!SeatWeaponComponent)
        return callOG(Pawn, Stack.CurrentNativeFunction, ServerOnExitVehicle, VehicleExitData);

    auto SeatComponent = (UFortVehicleSeatComponent*)Vehicle->GetComponentByClass(UFortVehicleSeatComponent::StaticClass());
    auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;
    auto WorldInventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;
    auto& ReplicatedEntries = WorldInventory->Inventory.ReplicatedEntries;
    int32 SeatIndex = -1;
    UFortWeaponItemDefinition* Weapon = nullptr;

    for (int i = 0; i < SeatComponent->PlayerSlots.Num(); i++)
    {
        if (SeatComponent->PlayerSlots[i].Player == Pawn)
        {
            SeatIndex = i;
            break;
        }
    }

    for (auto& WeaponDefinition : SeatWeaponComponent->WeaponSeatDefinitions)
    {
        if (WeaponDefinition.SeatIndex == SeatIndex)
        {
            Weapon = WeaponDefinition.VehicleWeapon;
            break;
        }
    }

    if (Weapon)
    {
        for (int i = 0; i < ReplicatedEntries.Num(); i++)
        {
            if (ReplicatedEntries[i].ItemDefinition == Weapon)
            {
                Remove(WorldInventory, ReplicatedEntries[i].ItemGuid);
                break;
            }
        }
    }

    *Ret = callOGWithRet(Pawn, Stack.CurrentNativeFunction, ServerOnExitVehicle, VehicleExitData);

    if (!Weapon)
        return;

    if (auto LastItem = Pawn->PreviousWeapon)
    {
        PlayerController->ServerExecuteInventoryItem(LastItem->ItemEntryGuid);
        PlayerController->ClientEquipItem(LastItem->ItemEntryGuid, true);
        return;
    }

    for (int i = 0; i < ReplicatedEntries.Num(); i++)
    {
        if (ReplicatedEntries[i].ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass()))
        {
            PlayerController->ServerExecuteInventoryItem(ReplicatedEntries[i].ItemGuid);
            PlayerController->ClientEquipItem(ReplicatedEntries[i].ItemGuid, true);
            break;
        }
    }
}

static void SetDBNOBeingRevivedTag(AActor* AbilitySystemActor, bool bBeingRevived)
{
    static auto DBNOBeingRevivedTag = FGameplayTag{ FName(L"Gameplay.Status.DBNOBeingRevived") };

    FGameplayTagContainer DBNOBeingRevivedTags{};
    DBNOBeingRevivedTags.GameplayTags.Add(DBNOBeingRevivedTag);

    if (bBeingRevived)
        UAbilitySystemBlueprintLibrary::AddLooseGameplayTags(AbilitySystemActor, DBNOBeingRevivedTags, true);
    else
        UAbilitySystemBlueprintLibrary::RemoveLooseGameplayTags(AbilitySystemActor, DBNOBeingRevivedTags, true);

    DBNOBeingRevivedTags.GameplayTags.Free();
}

static void SetDBNOBeingRevivedCue(AActor* AbilitySystemActor, bool bBeingRevived, float ReviveTime = 0.f, AActor* RevivingPawn = nullptr)
{
    static auto BeingRevivedCueTag = FGameplayTag{ FName(L"GameplayCue.Athena.Player.BeingRevivedFromDBNO") };

    FGameplayCueParameters CueParameters{};
    CueParameters.RawMagnitude = ReviveTime;
    CueParameters.Instigator = RevivingPawn;
    CueParameters.EffectCauser = RevivingPawn;
    CueParameters.GameplayEffectLevel = 1;
    CueParameters.AbilityLevel = 1;

    if (bBeingRevived)
        UGameplayCueFunctionLibrary::AddGameplayCueOnActor(AbilitySystemActor, BeingRevivedCueTag, CueParameters);
    else
        UGameplayCueFunctionLibrary::RemoveGameplayCueOnActor(AbilitySystemActor, BeingRevivedCueTag, CueParameters);
}

static void EndDBNOBeingRevived(AActor* AbilitySystemActor)
{
    SetDBNOBeingRevivedCue(AbilitySystemActor, false);
    SetDBNOBeingRevivedTag(AbilitySystemActor, false);
}

static decltype(DBNORevivePawns)::iterator FindDBNORevivePawn(AFortPlayerPawnAthena* Pawn)
{
    return std::find_if(DBNORevivePawns.begin(), DBNORevivePawns.end(), [&](auto& RevivePawn) { return RevivePawn.first.Get() == Pawn; });
}

static void BeginDBNOBeingRevived(AFortPlayerPawnAthena* Pawn, AActor* RevivingPawn)
{
    if (FindDBNORevivePawn(Pawn) != DBNORevivePawns.end() || !Pawn->PlayerState)
        return;

    SetDBNOBeingRevivedTag(Pawn->PlayerState, true);
    SetDBNOBeingRevivedCue(Pawn->PlayerState, true, Pawn->GetReviveFromDBNOTime(), RevivingPawn);
    DBNORevivePawns.push_back({ Pawn, Pawn->PlayerState });
}

void UpdateDBNORevivingActors(AFortPlayerPawnAthena* Pawn)
{
    std::vector<AFortPlayerPawnAthena*> RevivingPawns;

    if (Pawn->IsDBNO())
    {
        TArray<AFortPlayerControllerAthena*> PlayerControllers;
        GetAll<AFortPlayerControllerAthena>(PlayerControllers);

        for (auto PlayerController : PlayerControllers)
        {
            auto InteractionComponent = PlayerController ? (UFortControllerComponent_Interaction*)PlayerController->GetComponentByClass(UFortControllerComponent_Interaction::StaticClass()) : nullptr;
            auto RevivingPawn = InteractionComponent && InteractionComponent->LongInteractValidateInfo.Target == Pawn ? (AFortPlayerPawnAthena*)PlayerController->MyFortPawn : nullptr;

            if (RevivingPawn && RevivingPawn != Pawn && !RevivingPawn->IsDBNO())
                RevivingPawns.push_back(RevivingPawn);
        }

        PlayerControllers.Free();
    }

    if (Pawn->DBNORevivingActorsCount != (uint8)RevivingPawns.size())
    {
        Pawn->DBNORevivingActorsCount = (uint8)RevivingPawns.size();
        Pawn->ForceNetUpdate();
    }

    if (auto MultiInteractComponent = (UFortActorComponent_MultiInteract*)Pawn->GetComponentByClass(UFortActorComponent_MultiInteract::StaticClass()))
    {
        auto& InteractingPlayers = MultiInteractComponent->InteractingPlayers;

        for (int i = InteractingPlayers.Num() - 1; i >= 0; i--)
        {
            auto InteractingPlayer = InteractingPlayers[i];

            if (std::none_of(RevivingPawns.begin(), RevivingPawns.end(), [&](AFortPlayerPawnAthena* RevivingPawn) { return RevivingPawn->PlayerState == InteractingPlayer; }))
                MultiInteractComponent->RemoveInteractingActor(InteractingPlayer);
        }

        for (auto RevivingPawn : RevivingPawns)
            MultiInteractComponent->AddInteractingActor(RevivingPawn);
    }

    if (Pawn->ServerWorldTimeRevivalTime >= 0.f)
        return;

    if (!RevivingPawns.empty())
    {
        BeginDBNOBeingRevived(Pawn, RevivingPawns[0]);
        return;
    }

    for (size_t i = 0; i < DBNORevivePawns.size(); i++)
    {
        if (DBNORevivePawns[i].first.Get() != Pawn)
            continue;

        if (auto AbilitySystemActor = DBNORevivePawns[i].second.Get())
            EndDBNOBeingRevived(AbilitySystemActor);

        DBNORevivePawns.erase(DBNORevivePawns.begin() + i);
        return;
    }
}

static void FinishDBNORevive(AFortPlayerPawnAthena* Pawn)
{
    Pawn->ServerWorldTimeRevivalTime = -1.f;
    Pawn->ForceNetUpdate();

    auto PlayerState = GetPlayerStateAthena(Pawn);

    if (!Pawn->IsDBNO() && Pawn->Controller && PlayerState && PlayerState->AbilitySystemComponent)
    {
        static auto DBNOTag = FGameplayTag{ FName(L"Gameplay.Action.Player.DBNO") };
        static auto DBNOAthenaTag = FGameplayTag{ FName(L"Gameplay.Action.Player.DBNOAthena") };

        FGameplayTagContainer DBNOTags{};
        DBNOTags.GameplayTags.Add(DBNOTag);
        DBNOTags.GameplayTags.Add(DBNOAthenaTag);

        PlayerState->AbilitySystemComponent->BP_CancelAbilitiesWithTags(DBNOTags);

        DBNOTags.GameplayTags.Free();
    }

    UpdateDBNORevivingActors(Pawn);
}

static void SendReviveStatEvent(AFortPlayerControllerAthena* PlayerController, EFortQuestObjectiveStatEvent StatEvent, AActor* Target)
{
    FGameplayTagContainer TargetTags{};
    GetOwnedGameplayTags(Target, TargetTags);
    SendStatEvent(PlayerController->GetQuestManager(ESubGame::Athena), PlayerController, StatEvent, 1, true, Target, TargetTags);
    FreeGameplayTags(TargetTags);
}

void ReviveFromDBNO(AFortPlayerPawnAthena* Pawn, AActor* EventInstigator, bool bForceRevive)
{
    auto PlayerState = GetPlayerStateAthena(Pawn);
    auto AbilitySystemComponent = PlayerState ? PlayerState->AbilitySystemComponent : nullptr;

    if (!Pawn->IsDBNO() || !AbilitySystemComponent)
        return;

    auto ReviverController = EventInstigator && EventInstigator->IsA(AFortPlayerControllerAthena::StaticClass()) ? (AFortPlayerControllerAthena*)EventInstigator : nullptr;
    auto ReviverBotController = EventInstigator && EventInstigator->IsA(AFortAthenaAIBotController::StaticClass()) ? (AFortAthenaAIBotController*)EventInstigator : nullptr;
    auto ReviverPawn = ReviverController ? ReviverController->MyFortPawn : ReviverBotController ? ReviverBotController->Pawn : nullptr;
    auto PlayerController = Pawn->Controller && Pawn->Controller->IsA(AFortPlayerControllerAthena::StaticClass()) ? (AFortPlayerControllerAthena*)Pawn->Controller : nullptr;
    auto GameMode = (AFortGameModeZone*)UWorld::GetWorld()->AuthorityGameMode;
    auto ReviveGameplayEffect = GameMode ? GameMode->GetTeammateReviveGameplayEffectOverride() : TSubclassOf<UGameplayEffect>();

    if (!ReviveGameplayEffect)
        ReviveGameplayEffect = Pawn->TeammateReviveGameplayEffect;

    if (ReviveGameplayEffect)
    {
        auto SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(ReviveGameplayEffect, 1.f, FGameplayEffectContextHandle{});
        auto ReviveSpecHandle = UAbilitySystemBlueprintLibrary::AssignSetByCallerMagnitude(SpecHandle, FName(L"ReviveHeal"), Pawn->SetByCallerReviveHealth.Evaluate() - Pawn->GetHealth());

        AbilitySystemComponent->BP_ApplyGameplayEffectSpecToSelf(ReviveSpecHandle);

        ReviveSpecHandle.Release();
        SpecHandle.Release();
    }

    ((void (*)(AFortPlayerPawnAthena*, bool))Pawn->VTable[0x199])(Pawn, false);

    static auto UpdateDBNOInteractCollision = (void (*)(AFortPlayerPawnAthena*))(ImageBase + 0x3215E94);
    UpdateDBNOInteractCollision(Pawn);

    memset((PBYTE)&PlayerState->DeathInfo, 0, sizeof(FDeathInfo));
    PlayerState->OnRep_DeathInfo();
    PlayerState->ForceNetUpdate();

    if (bForceRevive)
    {
        FinishDBNORevive(Pawn);

        if (PlayerController)
            PlayerController->ClientOnPawnRevived((AController*)EventInstigator);

        return;
    }

    static auto RevivedTag = FGameplayTag{ FName(L"Event.RevivedAthena") };

    FGameplayEventData EventData{};
    EventData.EventTag = RevivedTag;
    EventData.Instigator = ReviverPawn;
    EventData.Target = Pawn;

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Pawn, RevivedTag, EventData);

    BeginDBNOBeingRevived(Pawn, ReviverPawn);

    auto RevivePawn = FindDBNORevivePawn(Pawn);

    if (RevivePawn != DBNORevivePawns.end())
    {
        if (auto AbilitySystemActor = RevivePawn->second.Get())
            SetDBNOBeingRevivedCue(AbilitySystemActor, false);
    }

    Pawn->ServerWorldTimeRevivalTime = (float)UGameplayStatics::GetTimeSeconds(Pawn) + Pawn->ConvertFromDBNOTime;
    Pawn->ForceNetUpdate();

    if (PlayerController)
        PlayerController->ClientOnPawnRevived((AController*)EventInstigator);

    if (ReviverController)
        SendReviveStatEvent(ReviverController, EFortQuestObjectiveStatEvent::RevivePlayer, Pawn);

    if (PlayerController)
        SendReviveStatEvent(PlayerController, EFortQuestObjectiveStatEvent::RevivedFromDBNO, ReviverPawn);
}

static void ServerReviveFromDBNO(AFortPlayerPawnAthena* Pawn, AActor* EventInstigator)
{
    ReviveFromDBNO(Pawn, EventInstigator, false);
}

static void ServerReviveFromDBNO(UObject* Context, FFrame& Stack)
{
    AController* EventInstigator;

    Stack.StepCompiledIn(&EventInstigator);
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;

    if (Pawn->Controller && (!Pawn->Controller->IsA(AFortPlayerControllerAthena::StaticClass()) || (EventInstigator && EventInstigator->IsA(AFortAthenaAIBotController::StaticClass()) && ((AFortAthenaAIBotController*)EventInstigator)->ReviveTarget == Pawn)))
        ReviveFromDBNO(Pawn, EventInstigator, false);
}

static void PlayGroupEmote(UObject* Context, FFrame& Stack)
{
    UObject* EmoteAsset;

    Stack.StepCompiledIn(&EmoteAsset);
    Stack.IncrementCode();

    auto Pawn = (AFortPlayerPawnAthena*)Context;
    auto DanceAsset = EmoteAsset && EmoteAsset->IsA(UAthenaDanceItemDefinition::StaticClass()) ? (UAthenaDanceItemDefinition*)EmoteAsset : nullptr;
    auto PlayerState = GetPlayerStateAthena(Pawn);
    auto AbilitySystemComponent = PlayerState ? PlayerState->AbilitySystemComponent : nullptr;

    if (!DanceAsset || !AbilitySystemComponent)
        return;

    auto EmoteAbilityClass = FindCachedObject(CachedEmoteAbilityClass, L"/Game/Abilities/Emotes/GAB_Emote_Generic.GAB_Emote_Generic_C");
    auto CustomAbility = DanceAsset->CustomDanceAbility.Get();
    auto AbilityClass = CustomAbility ? CustomAbility : EmoteAbilityClass;

    if (!AbilityClass)
        return;

    Pawn->bMovingEmote = DanceAsset->bMovingEmote;
    Pawn->EmoteWalkSpeed = DanceAsset->WalkForwardSpeed;
    Pawn->bMovingEmoteForwardOnly = DanceAsset->bMoveForwardOnly;
    Pawn->bMovingEmoteFollowingOnly = DanceAsset->bMoveFollowingOnly;

    static auto ConstructAbilitySpec = (void (*)(FGameplayAbilitySpec*, const UObject*, int, int, UObject*))(ImageBase + 0x7CF6DD0);
    static auto GiveAbilityAndActivateOnce = (void (*)(UAbilitySystemComponent*, FGameplayAbilitySpecHandle*, FGameplayAbilitySpec*, void*))(ImageBase + 0x7CBA17C);

    auto Spec = (FGameplayAbilitySpec*)calloc(1, sizeof(FGameplayAbilitySpec));
    FGameplayAbilitySpecHandle Handle;

    ConstructAbilitySpec(Spec, AbilityClass->ClassDefaultObject, 1, -1, EmoteAsset);
    GiveAbilityAndActivateOnce(AbilitySystemComponent, &Handle, Spec, nullptr);

    free(Spec);

    auto OldEmote = Pawn->LastReplicatedEmoteExecuted;
    Pawn->LastReplicatedEmoteExecuted = (UFortItemDefinition*)EmoteAsset;
    Pawn->OnRep_LastReplicatedEmoteExecuted(OldEmote);
}

static void StartCarryingPlayer(AFortPlayerPawnAthena* Pawn, AFortPlayerPawnAthena* InDBNOHoistee, EFortDBNOCarryEvent CarryEvent)
{
    Pawn->DBNOHoistee = InDBNOHoistee;
    Pawn->OnRep_DBNOHoistee();

    auto PreviousHoisterData = InDBNOHoistee->DBNOHoisterData;

    InDBNOHoistee->DBNOHoisterData.DBNOHoister = Pawn;
    InDBNOHoistee->DBNOHoisterData.DBNOCarryEvent = CarryEvent;
    InDBNOHoistee->OnRep_DBNOHoisterData(PreviousHoisterData);

    InDBNOHoistee->ForceNetUpdate();
    Pawn->ForceNetUpdate();
}

static void ServerHoistDBNOPlayer(AFortPlayerPawnAthena* Pawn, AFortPlayerPawnAthena* InDBNOHoistee)
{
    if (InDBNOHoistee && ((bool (*)(AFortPlayerPawnAthena*, AActor*))InDBNOHoistee->VTable[0x2B9])(InDBNOHoistee, Pawn->Controller))
        StartCarryingPlayer(Pawn, InDBNOHoistee, EFortDBNOCarryEvent::PickedUp);
}

static void ServerInterrogateDBNOPlayer(AFortPlayerPawnAthena* Pawn, AFortPlayerPawnAthena* InDBNOHoistee)
{
    if (InDBNOHoistee && InDBNOHoistee->CanInterrogateFromDBNO(Pawn->Controller))
        StartCarryingPlayer(Pawn, InDBNOHoistee, EFortDBNOCarryEvent::Interrogating);
}

void AFortPlayerPawn__PostLoad()
{
    auto PawnClass = AFortPlayerPawnAthena::StaticClass();

    ExecHook(PawnClass->FindFunction("ServerHandlePickupInfo"), ServerHandlePickupInfo);
    ExecHook(PawnClass->FindFunction("OnCapsuleBeginOverlap"), OnCapsuleBeginOverlap, OnCapsuleBeginOverlapOG);
    ExecHook(PawnClass->FindFunction("ServerSendZiplineState"), ServerSendZiplineState);
    ExecHook(PawnClass->FindFunction("ServerOnExitVehicle"), ServerOnExitVehicle, ServerOnExitVehicleOG);
    Hook<AFortPlayerPawnAthena>(uint32(0x23A), (void (*)(AFortPlayerPawnAthena*, AActor*))ServerReviveFromDBNO);
    ExecHook(PawnClass->FindFunction("PlayGroupEmote"), PlayGroupEmote);
    Hook<AFortPlayerPawnAthena>(uint32(0x23E), ServerHoistDBNOPlayer);
    Hook<AFortPlayerPawnAthena>(uint32(0x23D), ServerInterrogateDBNOPlayer);

    ExecHook(PawnClass->FindFunction("ServerReviveFromDBNO"), (void (*)(UObject*, FFrame&))ServerReviveFromDBNO);
}

void AFortPlayerPawn__Tick()
{
    if (!DBNORevivePawns.empty())
    {
        auto Time = (float)UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
        auto RevivePawns = DBNORevivePawns;

        for (auto& RevivePawn : RevivePawns)
        {
            auto Pawn = RevivePawn.first.Get();

            if (!Pawn)
            {
                if (auto AbilitySystemActor = RevivePawn.second.Get())
                    EndDBNOBeingRevived(AbilitySystemActor);
            }
            else if (Pawn->ServerWorldTimeRevivalTime < 0.f)
                UpdateDBNORevivingActors(Pawn);
            else if (Time >= Pawn->ServerWorldTimeRevivalTime)
                FinishDBNORevive(Pawn);
        }

        std::erase_if(DBNORevivePawns, [](auto& RevivePawn) { return !RevivePawn.first.Get(); });
    }

    static auto SimulatedProxyActiveGameplayCuesName = FName(L"SimulatedProxyActiveGameplayCues");
    static auto SimulatedProxyMinimalReplicationGameplayCuesName = FName(L"SimulatedProxyMinimalReplicationGameplayCues");

    TArray<AFortPlayerPawnAthena*> Pawns;
    GetAll<AFortPlayerPawnAthena>(Pawns);

    for (auto Pawn : Pawns)
    {
        auto PlayerState = Pawn ? GetPlayerStateAthena(Pawn) : nullptr;
        auto AbilitySystemComponent = PlayerState ? PlayerState->AbilitySystemComponent : nullptr;

        if (!AbilitySystemComponent)
            continue;

        if (PreReplication(&Pawn->SimulatedProxyActiveGameplayCues, AbilitySystemComponent->ActiveGameplayCues))
            UNetPushModelHelpers::MarkPropertyDirty(Pawn, SimulatedProxyActiveGameplayCuesName);

        if (PreReplication(&Pawn->SimulatedProxyMinimalReplicationGameplayCues, AbilitySystemComponent->MinimalReplicationGameplayCues))
            UNetPushModelHelpers::MarkPropertyDirty(Pawn, SimulatedProxyMinimalReplicationGameplayCuesName);
    }

    Pawns.Free();
}
