#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Components/FortControllerComponent_IndicatedActorManagement.h"

INIT_POSTLOAD(UFortIndicatedActorManagementLibrary);

static void AddActorsToIndicatedList(UObject* Context, FFrame& Stack)
{
    AController* InstigatingController;
    TArray<AActor*> IndicatedActors;
    FIndicatedActorData IndicatedActorData{};
    bool bAddAsUnique;
    bool bAllowOwningPlayer;
    bool bReplaceExistingEntry;
    bool bRefreshExistingEntry;

    Stack.StepCompiledIn(&InstigatingController);
    Stack.StepCompiledIn(&IndicatedActors);
    Stack.StepCompiledIn(&IndicatedActorData);
    Stack.StepCompiledIn(&bAddAsUnique);
    Stack.StepCompiledIn(&bAllowOwningPlayer);
    Stack.StepCompiledIn(&bReplaceExistingEntry);
    Stack.StepCompiledIn(&bRefreshExistingEntry);
    Stack.IncrementCode();

    if (auto Component = InstigatingController ? GetIndicatedActorManagement(InstigatingController) : nullptr)
        AddIndicatedActors(Component, IndicatedActors, IndicatedActorData, bAddAsUnique, bReplaceExistingEntry, bRefreshExistingEntry);

    FreeData(IndicatedActorData);
    IndicatedActors.Free();
}

static void AddActorsToStenciledList(UObject* Context, FFrame& Stack)
{
    AController* InstigatingController;
    TArray<AActor*> StenciledActors;
    FStenciledActorData StenciledActorData{};
    bool bAddAsUnique;
    bool bReplaceExistingEntry;
    bool bRefreshExistingEntry;

    Stack.StepCompiledIn(&InstigatingController);
    Stack.StepCompiledIn(&StenciledActors);
    Stack.StepCompiledIn(&StenciledActorData);
    Stack.StepCompiledIn(&bAddAsUnique);
    Stack.StepCompiledIn(&bReplaceExistingEntry);
    Stack.StepCompiledIn(&bRefreshExistingEntry);
    Stack.IncrementCode();

    if (auto Component = InstigatingController ? GetIndicatedActorManagement(InstigatingController) : nullptr)
        AddStenciledActors(Component, StenciledActors, StenciledActorData, bAddAsUnique, bReplaceExistingEntry, bRefreshExistingEntry);

    FreeData(StenciledActorData);
    StenciledActors.Free();
}

static void AddActorsInRadiusToIndicatedList(UObject* Context, FFrame& Stack)
{
    AddActorsInRadius(Stack, false);
}

static void AddActorsInRadiusToStenciledList(UObject* Context, FFrame& Stack)
{
    AddActorsInRadius(Stack, true);
}

void UFortIndicatedActorManagementLibrary__PostLoad()
{
    auto LibraryClass = UFortIndicatedActorManagementLibrary::StaticClass();

    ExecHook(LibraryClass->FindFunction("AddActorsToIndicatedList"), AddActorsToIndicatedList);
    ExecHook(LibraryClass->FindFunction("AddActorsToStenciledList"), AddActorsToStenciledList);
    ExecHook(LibraryClass->FindFunction("AddActorsInRadiusToIndicatedList"), AddActorsInRadiusToIndicatedList);
    ExecHook(LibraryClass->FindFunction("AddActorsInRadiusToStenciledList"), AddActorsInRadiusToStenciledList);
}
