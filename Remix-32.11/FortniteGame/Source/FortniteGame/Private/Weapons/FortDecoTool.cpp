#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"

INIT_POSTLOAD(AFortDecoTool);

static UFortDecoItemDefinition* GetDecoItemDefinition(AFortDecoTool* DecoTool, EBuildingAttachmentType InBuildingAttachmentType)
{
    auto ItemDefinition = (UFortDecoItemDefinition*)DecoTool->ItemDefinition;

    if (DecoTool->IsA(AFortDecoTool_ContextTrap::StaticClass()))
    {
        auto ContextTrapItemDefinition = ((AFortDecoTool_ContextTrap*)DecoTool)->ContextTrapItemDefinition;

        switch ((int)InBuildingAttachmentType)
        {
        case 0:
        case 6:
            ItemDefinition = ContextTrapItemDefinition->FloorTrap;
            break;
        case 7:
        case 2:
            ItemDefinition = ContextTrapItemDefinition->CeilingTrap;
            break;
        case 1:
            ItemDefinition = ContextTrapItemDefinition->WallTrap;
            break;
        case 8:
            ItemDefinition = ContextTrapItemDefinition->StairTrap;
            break;
        }
    }

    return ItemDefinition;
}

static void ServerSpawnDeco(UObject* Context, FFrame& Stack, void* Ret)
{
    FVector Location;
    FRotator Rotation;
    ABuildingSMActor* AttachedActor;
    EBuildingAttachmentType InBuildingAttachmentType;
    Stack.StepCompiledIn(&Location);
    Stack.StepCompiledIn(&Rotation);
    Stack.StepCompiledIn(&AttachedActor);
    Stack.StepCompiledIn(&InBuildingAttachmentType);
    Stack.IncrementCode();

    auto DecoTool = (AFortDecoTool*)Context;
    auto Pawn = (AFortPlayerPawnAthena*)DecoTool->Owner;

    if (!Pawn)
        return;

    auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;

    if (!PlayerController)
        return;

    auto ItemDefinition = GetDecoItemDefinition(DecoTool, InBuildingAttachmentType);

    if (!ItemDefinition)
        return;

    auto ShouldAllowServerSpawnDeco = (bool (*)(AFortDecoTool*, FVector&, FRotator&, ABuildingSMActor*, EBuildingAttachmentType))DecoTool->VTable[0x1A3];

    if (!ShouldAllowServerSpawnDeco(DecoTool, Location, Rotation, AttachedActor, InBuildingAttachmentType))
        return;

    auto SpawnDeco = (ABuildingSMActor* (*)(AFortDecoTool*, TSubclassOf<ABuildingSMActor>&, FVector&, FRotator&, ABuildingSMActor*, EBuildingAttachmentType, int))DecoTool->VTable[0x198];

    TSubclassOf<ABuildingSMActor> DecoClass = ItemDefinition->BlueprintClass.Get();
    SpawnDeco(DecoTool, DecoClass, Location, Rotation, AttachedActor, InBuildingAttachmentType, 0);

    auto WorldInventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;
    auto DecoItemDefinition = (UFortItemDefinition*)DecoTool->ItemDefinition;
    auto Item = FindItemInstance(WorldInventory, DecoItemDefinition);
    auto ItemEntry = FindReplicatedEntry(WorldInventory, DecoItemDefinition);

    if (!ItemEntry)
        return;

    ItemEntry->Count--;
    UpdateOrRemoveEntry(WorldInventory, Item, *ItemEntry);
}

static void ServerCreateBuildingAndSpawnDeco(UObject* Context, FFrame& Stack, void* Ret)
{
    FVector BuildingLocation;
    EFortCardinalDirection BuildingDirection;
    FVector Location;
    FRotator Rotation;
    EBuildingAttachmentType InBuildingAttachmentType;
    bool bSpawnDecoOnExtraPiece;
    FVector BuildingExtraPieceLocation;
    Stack.StepCompiledIn(&BuildingLocation);
    Stack.StepCompiledIn(&BuildingDirection);
    Stack.StepCompiledIn(&Location);
    Stack.StepCompiledIn(&Rotation);
    Stack.StepCompiledIn(&InBuildingAttachmentType);
    Stack.StepCompiledIn(&bSpawnDecoOnExtraPiece);
    Stack.StepCompiledIn(&BuildingExtraPieceLocation);
    Stack.IncrementCode();

    FRotator BuildingRotation{};
    BuildingRotation.Yaw = (uint8)BuildingDirection == 0 ? -90.f : ((uint8)BuildingDirection == 2 ? 90.f : ((uint8)BuildingDirection == 3 ? 180.f : 0.f));

    auto DecoTool = (AFortDecoTool*)Context;
    auto Pawn = (AFortPlayerPawnAthena*)DecoTool->Owner;

    if (!Pawn)
        return;

    auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;

    if (!PlayerController)
        return;

    auto ItemDefinition = GetDecoItemDefinition(DecoTool, InBuildingAttachmentType);

    if (!ItemDefinition)
        return;

    auto GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
    UClass* BuildingClass = nullptr;
    EFortResourceType ResourceType = PlayerController->CurrentResourceType;

    if (ItemDefinition->AutoCreateAttachmentBuildingResourceType != EFortResourceType::None)
        ResourceType = ItemDefinition->AutoCreateAttachmentBuildingResourceType;

    auto BuildingType = (uint8)InBuildingAttachmentType <= 7 && ((0xC5 >> (uint8)InBuildingAttachmentType) & 1) ? 1 : ((uint8)InBuildingAttachmentType == 1 ? 0 : 12);

    static auto GetSparseClassData = (FBuildingSMActorClassData* (*)(UClass*, uint8))(ImageBase + 0x1BBBDC8);

    for (auto& ShapePtr : ItemDefinition->AutoCreateAttachmentBuildingShapes)
    {
        auto Shape = ShapePtr.Get();

        for (auto& Class : GameState->AllPlayerBuildableClasses)
        {
            auto Default = (ABuildingSMActor*)((UClass*)Class)->ClassDefaultObject;
            auto EditModePatternData = GetSparseClassData(Default->Class, 1)->EditModePatternData;

            if (Default->ResourceType == ResourceType && (uint8)Default->BuildingType == BuildingType && EditModePatternData == Shape)
            {
                BuildingClass = Class;
                break;
            }
        }

        if (BuildingClass)
            break;
    }

    if (!BuildingClass)
        return;

    FBuildingClassData BuildingClassData{};
    BuildingClassData.BuildingClass = BuildingClass;
    BuildingClassData.PreviousBuildingLevel = -1;
    BuildingClassData.UpgradeLevel = 0;

    static auto CanAffordToPlaceBuildableClass = (bool (*)(AFortPlayerControllerAthena*, FBuildingClassData))(ImageBase + 0xA21CED8);

    if (!CanAffordToPlaceBuildableClass(PlayerController, BuildingClassData))
        return;

    static auto CantBuild = (__int64 (*)(UWorld*, TSubclassOf<ABuildingActor>&, FVector, FRotator, bool, TArray<ABuildingSMActor*>*, char*))(ImageBase + 0x9A95ED4);

    TArray<ABuildingSMActor*> RemoveBuildings;
    char CantBuildReason;

    if (CantBuild(UWorld::GetWorld(), BuildingClassData.BuildingClass, BuildingLocation, BuildingRotation, false, &RemoveBuildings, &CantBuildReason))
        return;

    for (auto& RemoveBuilding : RemoveBuildings)
        RemoveBuilding->K2_DestroyActor();

    RemoveBuildings.Free();

    auto Building = SpawnActorUnfinished<ABuildingSMActor>(BuildingClass, BuildingLocation, BuildingRotation, PlayerController);

    if (!Building)
        return;

    Building->InitializeKismetSpawnedBuildingActor(Building, PlayerController, true, nullptr, false);
    FinishSpawnActor(Building, BuildingLocation, BuildingRotation);

    Building->bPlayerPlaced = true;
    Building->Team = (EFortTeam)((AFortPlayerStateAthena*)PlayerController->PlayerState)->TeamIndex;
    Building->TeamIndex = (uint8)Building->Team;

    if (!PlayerController->bBuildFree)
    {
        static auto PayBuildableClassPlacementCost = (int (*)(AFortPlayerControllerAthena*, FBuildingClassData))(ImageBase + 0xA24BD50);

        PayBuildableClassPlacementCost(PlayerController, BuildingClassData);
    }

    DecoTool->ServerSpawnDeco(Location, Rotation, Building, InBuildingAttachmentType);
}

void AFortDecoTool__PostLoad()
{
    ExecHook(AFortDecoTool::StaticClass()->FindFunction("ServerSpawnDeco"), ServerSpawnDeco);
    ExecHook(AFortDecoTool::StaticClass()->FindFunction("ServerCreateBuildingAndSpawnDeco"), ServerCreateBuildingAndSpawnDeco);
}
