#include "pch.h"
#include "Shared.h"
#include "SDK/SDK/DynamicPoiPointProviderRuntime_classes.hpp"

INIT_MODULE(AFortAthenaLivingWorldStaticPointProvider);

static void EnablePointProvider(void* PointProviderInterface)
{
    static auto EnablePointProvider = (void (*)(UFortAthenaLivingWorldManager*, const UObject*))(ImageBase + 0xB4BA5E4);

    auto PointProvider = (AFortAthenaLivingWorldStaticPointProvider*)(uint64(PointProviderInterface) - 0x2A8);

    if (PointProvider->CachedLivingWorldManager)
        EnablePointProvider(PointProvider->CachedLivingWorldManager, PointProvider);
}

static void DisablePointProvider(void* PointProviderInterface)
{
    static auto DisablePointProvider = (void (*)(UFortAthenaLivingWorldManager*, const UObject*))(ImageBase + 0xB4B9FB4);

    auto PointProvider = (AFortAthenaLivingWorldStaticPointProvider*)(uint64(PointProviderInterface) - 0x2A8);

    if (PointProvider->CachedLivingWorldManager)
        DisablePointProvider(PointProvider->CachedLivingWorldManager, PointProvider);
}

static void (*RegisterToLivingWorldManagerOG)(void*, UObject*);
static void RegisterToLivingWorldManager(void* PointProviderInterface, UObject* PointProvider)
{
    auto LivingWorldManager = PointProvider ? UFortAthenaLivingWorldManager::GetLivingWorldManager(PointProvider) : nullptr;

    if (!LivingWorldManager)
        return RegisterToLivingWorldManagerOG(PointProviderInterface, PointProvider);

    if (PointProvider->IsA(AFortAthenaLivingWorldStaticPointProvider::StaticClass()))
        ((AFortAthenaLivingWorldStaticPointProvider*)PointProvider)->CachedLivingWorldManager = LivingWorldManager;
    else if (PointProvider->IsA(AFortAthenaPatrolPathPointProvider::StaticClass()))
        ((AFortAthenaPatrolPathPointProvider*)PointProvider)->CachedLivingWorldManager = LivingWorldManager;
    else if (PointProvider->IsA(AFortAthenaLivingWorldVolume::StaticClass()))
        ((AFortAthenaLivingWorldVolume*)PointProvider)->CachedLivingWorldManager = LivingWorldManager;
    else if (PointProvider->IsA(UFortAthenaLivingWorldLightStaticPointProvider::StaticClass()))
        ((UFortAthenaLivingWorldLightStaticPointProvider*)PointProvider)->CachedLivingWorldManager = LivingWorldManager;
    else if (PointProvider->IsA(UFortAthenaWaterBodyPointProvider::StaticClass()))
        ((UFortAthenaWaterBodyPointProvider*)PointProvider)->CachedLivingWorldManager = LivingWorldManager;
    else if (PointProvider->IsA(UDynamicPoiPointProvider::StaticClass()))
        ((UDynamicPoiPointProvider*)PointProvider)->CachedLivingWorldManager = LivingWorldManager;

    LivingWorldManager->RegisteredPointProvidersCount++;
}

void AFortAthenaLivingWorldStaticPointProvider__Init()
{
    auto BaseInterfaceVft = *(void***)(uint64(AFortAthenaLivingWorldStaticPointProvider::GetDefaultObj()) + 0x2A8);
    auto StrippedEnable = BaseInterfaceVft[0xB];
    auto StrippedDisable = BaseInterfaceVft[0xC];

    for (int32 i = 0; i < UObject::GObjects->Num(); i++)
    {
        auto Object = UObject::GObjects->GetByIndex(i);

        if (!Object || !Object->IsDefaultObject() || !Object->IsA(AFortAthenaLivingWorldStaticPointProvider::StaticClass()))
            continue;

        auto InterfaceVft = *(void***)(uint64(Object) + 0x2A8);

        if (InterfaceVft[0xB] == StrippedEnable)
            Patch<void*>(uint64(&InterfaceVft[0xB]), (void*)EnablePointProvider);

        if (InterfaceVft[0xC] == StrippedDisable)
            Patch<void*>(uint64(&InterfaceVft[0xC]), (void*)DisablePointProvider);
    }

    Hook(ImageBase + 0xB4E282C, RegisterToLivingWorldManager, RegisterToLivingWorldManagerOG);
}
