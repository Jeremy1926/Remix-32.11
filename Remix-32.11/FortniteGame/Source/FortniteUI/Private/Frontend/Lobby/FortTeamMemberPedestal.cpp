#include "pch.h"
#include "Shared.h"

INIT_MODULE(AFortTeamMemberPedestal);

void AFortTeamMemberPedestal__Init()
{
    Hook<AFortTeamMemberPedestal>(uint32(0x74), AActor::GetDefaultObj()->VTable[0x74]);
}
