#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortPlayerPawnAthena.h"

INIT_MODULE(AFortTeamInfo);

static bool (*AddToTeamOG)(AFortTeamInfo*, AController*);
static bool AddToTeam(AFortTeamInfo* TeamInfo, AController* Controller)
{
    auto bAdded = AddToTeamOG(TeamInfo, Controller);

    if (bAdded)
        UpdateTeamGroup(Controller->Pawn, nullptr, Controller->PlayerState);

    return bAdded;
}

void AFortTeamInfo__Init()
{
    Hook(ImageBase + 0x9B939E4, AddToTeam, AddToTeamOG);
}
