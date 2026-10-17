#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Plugins/GameFeatures/VictoryCrownsGameplay/Source/VictoryCrownsRuntime/Public/FortControllerComponent_VictoryCrowns.h"
#include "Engine/Source/Runtime/Engine/Public/TimerManager.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"
#include "FortniteGame/Source/FortniteGame/Public/Quests/FortQuestManager.h"
#include "SDK/SDK/VictoryCrownsAmbienceRuntime_structs.hpp"

INIT_POSTLOAD(UFortControllerComponent_VictoryCrowns);

static UFortControllerComponent_VictoryCrowns* GetVictoryCrownsComponent(AActor* PlayerController)
{
    return (UFortControllerComponent_VictoryCrowns*)PlayerController->GetComponentByClass(UFortControllerComponent_VictoryCrowns::StaticClass());
}

static bool CurrentPlaylistSupportsVictoryCrowns(UFortControllerComponent_VictoryCrowns* Component)
{
    static auto CurrentPlaylistSupportsVictoryCrownsInternal = (bool (*)(void*))(ImageBase + 0xC818DD4);

    return CurrentPlaylistSupportsVictoryCrownsInternal((uint8*)Component + 0xA8);
}

static void GrantCrown(AFortPlayerController* PlayerController)
{
    auto Component = GetVictoryCrownsComponent(PlayerController);
    auto WorldInventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;
    auto Crown = Component ? Component->CrownInventoryItemClass.Get() : nullptr;

    if (!Crown || !WorldInventory || Component->IsCrownInPlayerInventory())
        return;

    GiveItem(WorldInventory, Crown, 1);
}

static void (*BeginPlayOG)(UFortControllerComponent_VictoryCrowns*);
static void BeginPlay(UFortControllerComponent_VictoryCrowns* Component)
{
    BeginPlayOG(Component);

    auto Owner = Component->GetOwner();

    if (!Owner || !Owner->IsA(AFortPlayerControllerAthena::StaticClass()))
        return;

    auto PlayerController = (AFortPlayerControllerAthena*)Owner;

    PlayerController->OnPossessedPawnChanged.Bind(Component, FName(L"HandlePawnChanged"));

    if (auto AircraftComponent = PlayerController->GetAircraftComponent())
    {
        AircraftComponent->OnEnterAircraftEvent.Bind(Component, FName(L"OnEnterAircraft"));
        AircraftComponent->OnExitAircraftEvent.Bind(Component, FName(L"HandleExitAircraft"));
    }

    if (auto GameStateComponent = UWorld::GetWorld()->GameState->GetComponentByClass(UFortGameStateComponent_VictoryCrowns::StaticClass()))
        Component->OnVictoryCrownStatusChanged.Bind(GameStateComponent, FName(L"HandleCrownStatusChanged"));

    if (PlayerController->AthenaProfile)
        Component->OnAthenaProfileInitialized();
    else
        PlayerController->OnMcpProfilesInitializedEvent.Bind(Component, FName(L"OnAthenaProfileInitialized"));
}

static void (*CacheAccountItemDataOG)(UObject*, FFrame&, void*);
static void CacheAccountItemData(UObject* Context, FFrame& Stack, void* Ret)
{
    static auto GetSingleItemInstanceByTemplate = (UFortVictoryCrownAccountItem* (*)(UFortMcpProfile*, const void*))(ImageBase + 0xAD8AE1C);
    static auto GetTemplateId = UMcpItemDefinitionBase::StaticClass()->FindFunction("GetTemplateId");

    CacheAccountItemDataOG(Context, Stack, Ret);

    auto Component = (UFortControllerComponent_VictoryCrowns*)Context;
    auto Owner = Component->GetOwner();
    auto Profile = Owner && Owner->IsA(AFortPlayerControllerAthena::StaticClass()) ? ((AFortPlayerControllerAthena*)Owner)->AthenaProfile : nullptr;

    if (!*(bool*)(ImageBase + 0x12C67B50) || !Profile || !Component->VictoryCrownAccountItemDefinition || !GetTemplateId)
        return;

    FString TemplateId;
    Component->VictoryCrownAccountItemDefinition->ProcessEvent(GetTemplateId, &TemplateId);

    alignas(16) struct { const wchar_t* Data; int32 Len; } TemplateIdView{ TemplateId.CStr(), TemplateId.Num() > 0 ? TemplateId.Num() - 1 : 0 };
    auto Item = TemplateIdView.Len > 0 ? GetSingleItemInstanceByTemplate(Profile, &TemplateIdView) : nullptr;

    TemplateId.Free();

    if (!Item)
        return;

    Component->VictoryCrownAccountData = Item->victory_crown_account_data;
    Component->VictoryCrownAccountData.data_is_valid_for_mcp = true;
    Component->OnRep_VictoryCrownAccountData();
}

static void (*OnAthenaProfileInitializedOG)(UObject*, FFrame&, void*);
static void OnAthenaProfileInitialized(UObject* Context, FFrame& Stack, void* Ret)
{
    OnAthenaProfileInitializedOG(Context, Stack, Ret);

    auto Component = (UFortControllerComponent_VictoryCrowns*)Context;
    auto Owner = Component->GetOwner();

    if (!Owner || !Owner->IsA(AFortPlayerControllerAthena::StaticClass()))
        return;

    Component->CacheAccountItemData();

    if (!Component->VictoryCrownAccountData.has_victory_crown || !*(bool*)(ImageBase + 0x12C67B52) || !CurrentPlaylistSupportsVictoryCrowns(Component))
        return;

    SetActorTimer(Owner, (void*)GrantCrown, *(uint64*)(uint64(Component) + 0x1C8), UFortScalableFloatUtils::GetValueAtLevelWithContext(Component, Component->InitialGrantDelay, 0.f));
}

static void (*HandleExitAircraftOG)(UObject*, FFrame&, void*);
static void HandleExitAircraft(UObject* Context, FFrame& Stack, void* Ret)
{
    auto Component = (UFortControllerComponent_VictoryCrowns*)Context;
    auto ExitingController = *(AController**)Stack.Locals;
    auto bGrantCrown = *(bool*)(uint64(Component) + 0x132) && ExitingController && ExitingController->IsA(AFortPlayerController::StaticClass());

    HandleExitAircraftOG(Context, Stack, Ret);

    if (bGrantCrown)
        GrantCrown((AFortPlayerController*)ExitingController);
}

void GrantVictoryCrownAwards(AFortPlayerControllerAthena* PlayerController)
{
    static auto GetCurrentPlaylistData = (UFortPlaylistAthena* (*)(AFortGameStateAthena*))(ImageBase + 0x4B6DB14);
    static auto FindRow = (FVictoryCrownPlaylistData* (*)(UDataTable*, FName, const wchar_t*, bool))(ImageBase + 0xC16B0BC);

    auto Component = GetVictoryCrownsComponent(PlayerController);
    auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;
    auto Playlist = GetCurrentPlaylistData((AFortGameStateAthena*)UWorld::GetWorld()->GameState);

    if (!*(bool*)(ImageBase + 0x12C67B51) || !Component || Component->bWonCrownInMatch || !PlayerState || !Playlist || !CurrentPlaylistSupportsVictoryCrowns(Component))
        return;

    auto Row = FindRow(Component->VictoryCrownPlaylistData, Playlist->PlaylistName, L"UFortControllerComponent_VictoryCrowns::WasOwnerQualifyingWinner", false);
    auto& AccountData = Component->VictoryCrownAccountData;

    AccountData.has_victory_crown = Row && PlayerState->Place > 0 && PlayerState->Place <= Row->MaxRankingForCrownAward;

    if (AccountData.has_victory_crown)
    {
        AccountData.total_victory_crowns_bestowed_count++;
        Component->bWonCrownInMatch = true;

        if (Component->IsCrownInPlayerInventory())
        {
            AccountData.total_royal_royales_achieved_count++;
            Component->bWonRoyalRoyale = true;

            if (auto PlayerStateComponent = (UFortPlayerStateComponent_VictoryCrowns*)PlayerState->GetComponentByClass(UFortPlayerStateComponent_VictoryCrowns::StaticClass()))
                PlayerStateComponent->bHasWonRoyalRoyale = true;

            SendStatEvent(PlayerController->GetQuestManager(ESubGame::Athena), PlayerController, EFortQuestObjectiveStatEvent::ComplexCustom, 1, true, nullptr, FGameplayTagContainer(), Component->SourceTagsForRoyalRoyale);

            auto ExternalEmote = (UFortControllerComponent_ExternalEmote*)PlayerController->GetComponentByClass(UFortControllerComponent_ExternalEmote::StaticClass());

            if (*(bool*)(ImageBase + 0x12C67B53) && ExternalEmote)
            {
                FEmoteActionBinding Emote{};
                Emote.EmoteId = UKismetSystemLibrary::GetPrimaryAssetIdFromSoftObjectReference(*(TSoftObjectPtr<UObject>*)&Component->RoyalRoyaleEmoteItemDefinition);

                if (!Emote.EmoteId.PrimaryAssetName.IsNone())
                    ExternalEmote->UncategorizedEmotes.Add(Emote);
            }
        }
    }

    Component->OnRep_VictoryCrownAccountData();
}

void UFortControllerComponent_VictoryCrowns__PostLoad()
{
    Hook<UFortControllerComponent_VictoryCrowns>(uint32(0x78), BeginPlay, BeginPlayOG);
    ExecHook(UFortControllerComponent_VictoryCrowns::StaticClass()->FindFunction("CacheAccountItemData"), CacheAccountItemData, CacheAccountItemDataOG);
    ExecHook(UFortControllerComponent_VictoryCrowns::StaticClass()->FindFunction("OnAthenaProfileInitialized"), OnAthenaProfileInitialized, OnAthenaProfileInitializedOG);
    ExecHook(UFortControllerComponent_VictoryCrowns::StaticClass()->FindFunction("HandleExitAircraft"), HandleExitAircraft, HandleExitAircraftOG);
}
