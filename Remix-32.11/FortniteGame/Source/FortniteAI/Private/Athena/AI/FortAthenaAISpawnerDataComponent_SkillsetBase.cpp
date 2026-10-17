#include "pch.h"
#include "Shared.h"

INIT_MODULE(UFortAthenaAISpawnerDataComponent_SkillsetBase);

static UClass* GetDigestedSkillSetClass(UClass* SkillSetClass)
{
    auto SkillSet = SkillSetClass ? SkillSetClass->ClassDefaultObject : nullptr;
    UClass* DigestedSkillSetClass = nullptr;

    if (SkillSet)
        ((UClass** (*)(UObject*, UClass**))SkillSet->VTable[0x59])(SkillSet, &DigestedSkillSetClass);

    return DigestedSkillSetClass;
}

static void GetRuntimeParametersClasses(void* RuntimeParametersProvider, TArray<UClass*>* OutClasses)
{
    auto Component = (UFortAthenaAISpawnerDataComponent_SkillsetBase*)(uint64(RuntimeParametersProvider) - 0x28);

    if (!OutClasses || !Component->IsA(UFortAthenaAISpawnerDataComponent_SkillsetBase::StaticClass()))
        return;

    auto SkillSets = Component->GetSkillSets();

    for (int32 i = 0; i < SkillSets.Num(); i++)
        if (auto DigestedSkillSetClass = GetDigestedSkillSetClass(SkillSets.Data[i]))
            OutClasses->Add(DigestedSkillSetClass);

    SkillSets.Free();
}

static int32 FillRuntimeParameters(void* RuntimeParametersProvider, UObject* RuntimeParameters, int32 SkillLevel, int32 Result)
{
    auto Component = (UFortAthenaAISpawnerDataComponent_SkillsetBase*)(uint64(RuntimeParametersProvider) - 0x28);

    if (!RuntimeParameters || !Component->IsA(UFortAthenaAISpawnerDataComponent_SkillsetBase::StaticClass()))
        return Result;

    auto SkillSets = Component->GetSkillSets();

    for (int32 i = 0; i < SkillSets.Num(); i++)
    {
        auto DigestedSkillSetClass = GetDigestedSkillSetClass(SkillSets.Data[i]);

        if (!DigestedSkillSetClass || !RuntimeParameters->IsA(DigestedSkillSetClass))
            continue;

        ((void (*)(UObject*, int32, UObject*))RuntimeParameters->VTable[0x59])(RuntimeParameters, SkillLevel, ((UClass*)SkillSets.Data[i])->ClassDefaultObject);
        break;
    }

    SkillSets.Free();

    return Result;
}

void UFortAthenaAISpawnerDataComponent_SkillsetBase__Init()
{
    auto BaseInterfaceVft = *(void***)(uint64(UFortAthenaAISpawnerDataComponent::GetDefaultObj()) + 0x28);

    for (int32 i = 0; i < UObject::GObjects->Num(); i++)
    {
        auto Object = UObject::GObjects->GetByIndex(i);

        if (!Object || !Object->IsDefaultObject() || !Object->IsA(UFortAthenaAISpawnerDataComponent_SkillsetBase::StaticClass()))
            continue;

        auto InterfaceVft = *(void***)(uint64(Object) + 0x28);

        if (InterfaceVft == BaseInterfaceVft)
            continue;

        if (InterfaceVft[0x2] == BaseInterfaceVft[0x2])
            Patch<void*>(uint64(&InterfaceVft[0x2]), (void*)GetRuntimeParametersClasses);

        if (InterfaceVft[0x3] == BaseInterfaceVft[0x3])
            Patch<void*>(uint64(&InterfaceVft[0x3]), (void*)FillRuntimeParameters);
    }
}
