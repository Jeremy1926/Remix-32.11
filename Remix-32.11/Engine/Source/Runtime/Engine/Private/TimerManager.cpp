#include "pch.h"
#include "Shared.h"
#include "Engine/Source/Runtime/Engine/Public/TimerManager.h"

void ClearTimer(AActor* Actor, uint64& Handle)
{
    static auto GetWorldTimerManager = (void* (*)(AActor*))(ImageBase + 0x16D2DC0);
    static auto FindTimer = (void* (*)(void*, uint64*))(ImageBase + 0x16CF6F4);
    static auto InternalClearTimer = (void (*)(void*, uint64))(ImageBase + 0x177914C);

    auto TimerManager = GetWorldTimerManager(Actor);

    if (FindTimer(TimerManager, &Handle))
        InternalClearTimer(TimerManager, Handle);

    Handle = 0;
}

void SetActorTimer(AActor* Actor, void* Callback, uint64& Handle, float Delay)
{
    static auto BindTimerDelegate = (void (*)(FNativeDelegate*, UObject*, void*))(ImageBase + 0x9A2EE20);
    static auto GetWorldTimerManager = (void* (*)(AActor*))(ImageBase + 0x16D2DC0);
    static auto MakeUnifiedTimerDelegate = (void* (*)(uint8*, FNativeDelegate*))(ImageBase + 0x16D118C);
    static auto InternalSetTimer = (void (*)(void*, uint64*, uint8*, float, const void*))(ImageBase + 0x16D1304);
    static auto DestroyDelegate = (void (*)(FNativeDelegate*))(ImageBase + 0x16E0B28);

    FNativeDelegate TimerDelegate{};
    uint8 UnifiedTimerDelegate[0x48];
    struct { uint8 bLoop; uint8 bMaxOncePerFrame; float FirstDelay; } TimerParameters{ 0, 0, -1.f };

    BindTimerDelegate(&TimerDelegate, Actor, Callback);
    MakeUnifiedTimerDelegate(UnifiedTimerDelegate, &TimerDelegate);
    InternalSetTimer(GetWorldTimerManager(Actor), &Handle, UnifiedTimerDelegate, Delay, &TimerParameters);
    DestroyDelegate((FNativeDelegate*)UnifiedTimerDelegate);
    DestroyDelegate(&TimerDelegate);
}
