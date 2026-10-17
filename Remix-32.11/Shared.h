#pragma once
#include "pch.h"
#include "Core.hpp"

template <typename T = UObject>
inline T* GetFirstInstance(const UClass* Class = T::StaticClass())
{
    for (int32 i = 0; i < UObject::GObjects->Num(); i++)
    {
        auto Obj = UObject::GObjects->GetByIndex(i);

        if (Obj && !Obj->IsDefaultObject() && Obj->IsA(Class))
            return (T*)Obj;
    }

    return nullptr;
}

template <typename T>
inline T* FindCachedObject(std::optional<T*>& Cache, const wchar_t* Path)
{
    if (!Cache)
        Cache = FindObject<T>(Path);

    return *Cache;
}

template <typename T = AActor>
inline void GetAll(const UClass* Class, TArray<T*>& Out)
{
    UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), (UClass*)Class, (TArray<AActor*>*)&Out);
}

template <typename T = AActor>
inline void GetAll(TArray<T*>& Out)
{
    GetAll<T>(T::StaticClass(), Out);
}

inline AActor* SpawnActor(const UClass* Class, const FTransform& Transform, AActor* Owner = nullptr, ESpawnActorCollisionHandlingMethod CollisionOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn)
{
    auto Actor = UGameplayStatics::BeginDeferredActorSpawnFromClass(UWorld::GetWorld(), (UClass*)Class, Transform, CollisionOverride, Owner, ESpawnActorScaleMethod::MultiplyWithRoot);

    return UGameplayStatics::FinishSpawningActor(Actor, Transform, ESpawnActorScaleMethod::MultiplyWithRoot);
}

inline AActor* SpawnActor(const UClass* Class, FVector Loc, FRotator Rot = {}, AActor* Owner = nullptr)
{
    return SpawnActor(Class, UKismetMathLibrary::MakeTransform(Loc, Rot, FVector(1, 1, 1)), Owner);
}

template <typename T>
inline T* SpawnActor(const UClass* Class, FVector Loc, FRotator Rot = {}, AActor* Owner = nullptr)
{
    return (T*)SpawnActor(Class, Loc, Rot, Owner);
}

template <typename T>
inline T* SpawnActor(const UClass* Class, const FTransform& Transform, AActor* Owner = nullptr)
{
    return (T*)SpawnActor(Class, Transform, Owner);
}

template <typename T>
inline T* SpawnActor(FVector Loc, FRotator Rot = {}, AActor* Owner = nullptr)
{
    return (T*)SpawnActor(T::StaticClass(), Loc, Rot, Owner);
}

template <typename T = AActor>
inline T* SpawnActorUnfinished(const UClass* Class, FVector Loc, FRotator Rot = {}, AActor* Owner = nullptr)
{
    return (T*)UGameplayStatics::BeginDeferredActorSpawnFromClass(UWorld::GetWorld(), (UClass*)Class, UKismetMathLibrary::MakeTransform(Loc, Rot, FVector(1, 1, 1)), ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn, Owner, ESpawnActorScaleMethod::MultiplyWithRoot);
}

template <typename T = AActor>
inline T* FinishSpawnActor(AActor* Actor, FVector Loc, FRotator Rot)
{
    return (T*)UGameplayStatics::FinishSpawningActor(Actor, UKismetMathLibrary::MakeTransform(Loc, Rot, FVector(1, 1, 1)), ESpawnActorScaleMethod::MultiplyWithRoot);
}

inline void* _NpFH = nullptr;

template <class _Ot = void*>
void Hook(uint64_t _Ptr, void* _Detour, _Ot& _Orig = _NpFH)
{
    MH_CreateHook((LPVOID)_Ptr, _Detour, (LPVOID*)(std::is_same_v<_Ot, void*> ? nullptr : &_Orig));
}

inline void VirtualSwap(void** VTable, uint32_t Index, void* Detour)
{
    DWORD Protect;
    VirtualProtect(&VTable[Index], 8, PAGE_EXECUTE_READWRITE, &Protect);
    VTable[Index] = Detour;
    VirtualProtect(&VTable[Index], 8, Protect, &Protect);
}

template <class _Ct, class _Ot = void*>
void Hook(uint32_t _Ind, void* _Detour, _Ot& _Orig = _NpFH)
{
    auto _Vt = _Ct::GetDefaultObj()->VTable;

    if constexpr (!std::is_same_v<_Ot, void*>)
        _Orig = (_Ot)_Vt[_Ind];

    VirtualSwap(_Vt, _Ind, _Detour);
}

template <class _Ct>
void HookEvery(uint32_t _Ind, void* _Detour)
{
    for (int32 i = 0; i < UObject::GObjects->Num(); i++)
    {
        auto Obj = UObject::GObjects->GetByIndex(i);

        if (Obj && Obj->IsDefaultObject() && Obj->IsA(_Ct::StaticClass()))
            VirtualSwap(Obj->VTable, _Ind, _Detour);
    }
}

template <class _Ot = void*>
void ExecHook(UFunction* _Fn, void* _Detour, _Ot& _Orig = _NpFH)
{
    if (!_Fn)
        return;

    if constexpr (!std::is_same_v<_Ot, void*>)
        _Orig = (_Ot)_Fn->ExecFunction;

    _Fn->ExecFunction = (UFunction::FNativeFuncPtr)_Detour;
}

template <class _Pt>
void Patch(uint64_t _Ptr, _Pt _Val)
{
    DWORD Protect;
    VirtualProtect((LPVOID)_Ptr, sizeof(_Pt), PAGE_EXECUTE_READWRITE, &Protect);
    *(_Pt*)_Ptr = _Val;
    VirtualProtect((LPVOID)_Ptr, sizeof(_Pt), Protect, &Protect);
}

inline void* AllocateNear(uint64_t _Ptr)
{
    SYSTEM_INFO _Info;
    GetSystemInfo(&_Info);

    const uint64_t _Granularity = _Info.dwAllocationGranularity;
    const uint64_t _Lowest = uint64_t(_Info.lpMinimumApplicationAddress);
    const uint64_t _Highest = uint64_t(_Info.lpMaximumApplicationAddress);
    const uint64_t _Min = _Ptr > _Lowest + 0x7FFF0000 ? _Ptr - 0x7FFF0000 : _Lowest;
    const uint64_t _Max = _Ptr + 0x7FFF0000 < _Highest ? _Ptr + 0x7FFF0000 : _Highest;

    for (auto _Address = (_Ptr & ~(_Granularity - 1)) - _Granularity; _Address >= _Min && _Address < _Ptr; _Address -= _Granularity)
    {
        if (auto _Page = VirtualAlloc((LPVOID)_Address, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))
            return _Page;
    }

    for (auto _Address = (_Ptr & ~(_Granularity - 1)) + _Granularity; _Address + 0x1000 <= _Max; _Address += _Granularity)
    {
        if (auto _Page = VirtualAlloc((LPVOID)_Address, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))
            return _Page;
    }

    return nullptr;
}

inline void Rel32Hook(uint64_t _Ptr, void* _Detour)
{
    uint8_t _Jmp[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    *(void**)(_Jmp + 6) = _Detour;

    auto _Page = AllocateNear(_Ptr);

    if (!_Page)
        return;

    memcpy(_Page, _Jmp, sizeof(_Jmp));
    Patch<int32_t>(_Ptr, int32_t(uint64_t(_Page) - (_Ptr + 4)));
}

inline std::vector<void (*)()> Initters;
inline std::vector<void (*)()> PostLoaders;
inline std::vector<void (*)()> Tickers;

#define INIT_MODULE(Name)                                                                          \
    void Name##__Init();                                                                           \
    auto Initter__##Name = ([]() { Initters.push_back(Name##__Init); return 1; })();

#define INIT_POSTLOAD(Name)                                                                        \
    void Name##__PostLoad();                                                                       \
    auto PostLoader__##Name = ([]() { PostLoaders.push_back(Name##__PostLoad); return 1; })();

#define INIT_TICKER(Name)                                                                          \
    void Name##__Tick();                                                                           \
    auto Ticker__##Name = ([]() { Tickers.push_back(Name##__Tick); return 1; })();

#define callOG(_Tr, _Fn, _Th, ...)                                 \
    ([&]()                                                         \
    {                                                              \
        _Fn->ExecFunction = (UFunction::FNativeFuncPtr)_Th##OG;    \
        _Tr->_Th(__VA_ARGS__);                                     \
        _Fn->ExecFunction = (UFunction::FNativeFuncPtr)_Th;        \
    })()

#define callOGWithRet(_Tr, _Fn, _Th, ...)                          \
    ([&]()                                                         \
    {                                                              \
        _Fn->ExecFunction = (UFunction::FNativeFuncPtr)_Th##OG;    \
        auto _Rt = _Tr->_Th(__VA_ARGS__);                          \
        _Fn->ExecFunction = (UFunction::FNativeFuncPtr)_Th;        \
        return _Rt;                                                \
    })()

inline TMap<FName, FRealCurve*>& GetRowMap(const UCurveTable* CurveTable)
{
    return *(TMap<FName, FRealCurve*>*)(__int64(CurveTable) + 0x30);
}

inline void GetOwnedGameplayTags(const UObject* Object, FGameplayTagContainer& OwnedTags)
{
    if (auto Interface = Object ? Object->GetInterface(IGameplayTagAssetInterface::StaticClass()) : nullptr)
        ((void (*)(void*, FGameplayTagContainer*))(*(void***)Interface)[0x2])(Interface, &OwnedTags);
}

inline void FreeGameplayTags(FGameplayTagContainer& Tags)
{
    Tags.GameplayTags.Free();
    Tags.ParentTags.Free();
}

struct FNativeDelegate
{
    uint8* Instance;
    int32 Size;
};

struct FSharedRef
{
    void* Object;
    void* SharedReferenceCount;
};

struct FJsonKey
{
    const wchar_t* Data;
    int32 Len;
};

enum class EHttpRequestStatus : int32
{
    NotStarted,
    Processing,
    Failed,
    Failed_ConnectionError,
    Succeeded
};
