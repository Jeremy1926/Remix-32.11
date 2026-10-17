#include "pch.h"
#include "Shared.h"

INIT_MODULE(UFortEngine);

static float GetMaxTickRate(UEngine* Engine, float DeltaTime, bool bAllowFrameRateSmoothing)
{
    return 30.f;
}

static uint32_t CheckCheckpointHeartBeat()
{
    return -1;
}

void UFortEngine__Init()
{
    Hook(ImageBase + 0x189FE98, GetMaxTickRate);
    Hook(ImageBase + 0x521AFC0, CheckCheckpointHeartBeat);

    *(float*)(ImageBase + 0x130DCC64) = 0.0f;
}
