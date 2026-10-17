#include "pch.h"
#include "Shared.h"

INIT_POSTLOAD(AFortSafeZoneIndicator);

static void GetPhaseInfo(UObject* Context, FFrame& Stack, bool* Ret)
{
    FFortSafeZonePhaseInfo OutSafeZonePhaseTemp{};
    auto& OutSafeZonePhase = Stack.StepCompiledInRef(&OutSafeZonePhaseTemp);
    int32 InPhaseToGet;
    Stack.StepCompiledIn(&InPhaseToGet);
    Stack.IncrementCode();

    auto SafeZoneIndicator = (AFortSafeZoneIndicator*)Context;
    auto& SafeZonePhases = SafeZoneIndicator->SafeZonePhases;

    if (SafeZonePhases.IsValidIndex(InPhaseToGet))
    {
        memcpy((PBYTE)&OutSafeZonePhase, (PBYTE)&SafeZonePhases[InPhaseToGet], sizeof(FFortSafeZonePhaseInfo));

        *Ret = true;
        return;
    }

    *Ret = false;
}

void AFortSafeZoneIndicator__PostLoad()
{
    ExecHook(AFortSafeZoneIndicator::StaticClass()->FindFunction("GetPhaseInfo"), GetPhaseInfo);
}
