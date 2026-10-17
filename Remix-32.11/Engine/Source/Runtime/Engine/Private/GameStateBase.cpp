#include "pch.h"
#include "Shared.h"

INIT_MODULE(AGameStateBase);

static double GetServerWorldTimeSeconds(AGameStateBase* GameState)
{
    static auto GetTimeSeconds = (double (*)(UObject*))(ImageBase + 0x19C3A38);

    return GetTimeSeconds(GameState);
}

void AGameStateBase__Init()
{
    Hook(ImageBase + 0x1E35794, GetServerWorldTimeSeconds);
}
