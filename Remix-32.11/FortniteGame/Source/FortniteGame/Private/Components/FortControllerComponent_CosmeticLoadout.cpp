#include "pch.h"
#include "Shared.h"

INIT_POSTLOAD(UFortControllerComponent_CosmeticLoadout);

static void RefreshCosmeticLoadoutFromProfile(UFortControllerComponent_CosmeticLoadout* Component)
{
}

void UFortControllerComponent_CosmeticLoadout__PostLoad()
{
    Hook(ImageBase + 0x96774B4, RefreshCosmeticLoadoutFromProfile);
}
