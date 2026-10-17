#include "pch.h"
#include "Shared.h"

INIT_MODULE(UAthenaSpecialActorComponent);

struct FSpecialActorData
{
    AActor* SpecialActor;
    FName SpecialActorID;
    FGameplayTag CategoryTag;
    APlayerState* PlayerState;
    float ReplicationInterval;
    float ReplicationIntervalDeviation;
    FSlateBrush MinimapIconBrush;
    FVector2D MinimapScale;
    FSlateBrush FullScreenMapIconBrush;
    FVector2D FullScreenMapScale;
    FSlateBrush CompassIconBrush;
    FVector2D CompassScale;
    bool bDrawCompassIcon;
    bool bDrawCompassDistanceText;
    bool bDrawRotated;
    float YawOffset;
    uint8 RenderOnMapDuringPhases;
    bool bReplicateHealth;
    bool bReplicateShield;
    FText DisplayName;
    float RenderDistance;
    EFortMinimapLayer DrawLayer;
    FGameplayTagContainer RenderTags;
    FElevationArrowData ElevationArrowData;
    TArray<FSpecialActorSingleStatData> TrackedStats;
    int32 Priority;
};

static void RegisterSpecialActor(UAthenaSpecialActorComponent* Component)
{
    static auto ConstructSpecialActorData = (FSpecialActorData* (*)(FSpecialActorData*))(ImageBase + 0x9B8FEF0);
    static auto DestroySpecialActorData = (void (*)(FSpecialActorData*))(ImageBase + 0x90BC848);
    static auto CopySlateBrush = (FSlateBrush* (*)(FSlateBrush*, const FSlateBrush*))(ImageBase + 0x19794CC);
    static auto CopyGameplayTagContainer = (FGameplayTagContainer* (*)(FGameplayTagContainer*, const FGameplayTagContainer*))(ImageBase + 0x1895304);
    static auto AddActorToReplicationList = (int32 (*)(AFortSpecialActorReplicationInfo*, FSpecialActorData*))(ImageBase + 0x9B926EC);

    auto Owner = Component->GetOwner();
    auto World = UWorld::GetWorld();
    auto GameState = World && World->GameState && World->GameState->IsA(AFortGameStateAthena::StaticClass()) ? (AFortGameStateAthena*)World->GameState : nullptr;

    if (!Owner || Owner->Role != ENetRole::ROLE_Authority || Component->SpecialActorTag.TagName.IsNone() || !GameState || !GameState->SpecialActorData)
        return;

    auto TagName = Component->SpecialActorTag.TagName.GetRawString();
    auto OwnerName = Owner->Name.GetRawString();
    auto SpecialActorID = std::wstring(TagName.begin(), TagName.end()) + L"_" + std::wstring(OwnerName.begin(), OwnerName.end());

    FSpecialActorData Data;
    ConstructSpecialActorData(&Data);

    Data.SpecialActor = Owner;
    Data.SpecialActorID = FName(SpecialActorID.c_str());
    Data.CategoryTag = Component->SpecialActorTag;
    CopySlateBrush(&Data.MinimapIconBrush, &Component->SpecialActorMinimapIconBrush);
    Data.MinimapScale = Component->SpecialActorMinimapIconScale;
    CopySlateBrush(&Data.FullScreenMapIconBrush, &Component->SpecialActorFullScreenMapIconBrush);
    Data.FullScreenMapScale = Component->SpecialActorFullScreenMapIconScale;
    CopySlateBrush(&Data.CompassIconBrush, &Component->SpecialActorCompassIconBrush);
    Data.CompassScale = Component->SpecialActorCompassIconScale;
    Data.bDrawCompassIcon = Component->bUseCompassIcon;
    Data.bDrawCompassDistanceText = Component->bDrawCompassDistanceText;
    Data.bDrawRotated = Component->bDrawRotated;
    Data.YawOffset = Component->YawOffset;
    Data.RenderOnMapDuringPhases = Component->RenderOnMapDuringPhases;
    Data.RenderDistance = Component->RenderDistance.Evaluate();
    Data.DrawLayer = Component->DrawLayer;
    CopyGameplayTagContainer(&Data.RenderTags, &Component->RenderTags);
    Data.ElevationArrowData = Component->ElevationArrowData;
    Data.Priority = Component->Priority;

    if (AddActorToReplicationList(GameState->SpecialActorData, &Data) != -1)
        Component->SpecialActorID = Data.SpecialActorID;

    DestroySpecialActorData(&Data);
}

static void UnregisterSpecialActor(UAthenaSpecialActorComponent* Component)
{
    static auto RemoveActorFromReplicationList = (void (*)(AFortSpecialActorReplicationInfo*, FName))(ImageBase + 0x9BA8168);

    if (Component->SpecialActorID.IsNone())
        return;

    auto World = UWorld::GetWorld();
    auto GameState = World && World->GameState && World->GameState->IsA(AFortGameStateAthena::StaticClass()) ? (AFortGameStateAthena*)World->GameState : nullptr;

    if (GameState && GameState->SpecialActorData)
        RemoveActorFromReplicationList(GameState->SpecialActorData, Component->SpecialActorID);

    Component->SpecialActorID = FName();
}

static void (*ActivateOG)(UAthenaSpecialActorComponent*, bool);
static void Activate(UAthenaSpecialActorComponent* Component, bool bReset)
{
    ActivateOG(Component, bReset);

    if (Component->bIsActive)
        RegisterSpecialActor(Component);
}

static void (*DeactivateOG)(UAthenaSpecialActorComponent*);
static void Deactivate(UAthenaSpecialActorComponent* Component)
{
    DeactivateOG(Component);

    if (!Component->bIsActive)
        UnregisterSpecialActor(Component);
}

static void (*OnUnregisterOG)(UAthenaSpecialActorComponent*);
static void OnUnregister(UAthenaSpecialActorComponent* Component)
{
    UnregisterSpecialActor(Component);
    OnUnregisterOG(Component);
}

void UAthenaSpecialActorComponent__Init()
{
    Hook<UAthenaSpecialActorComponent>(uint32(0x5A), Activate, ActivateOG);
    Hook<UAthenaSpecialActorComponent>(uint32(0x5B), Deactivate, DeactivateOG);
    Hook<UAthenaSpecialActorComponent>(uint32(0x69), OnUnregister, OnUnregisterOG);
}
