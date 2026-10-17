#pragma once
#include "pch.h"

void ForceAllExitAircraft(UAthenaAIServicePlayerBots* Service);
void FinishPendingRespawns(UAthenaAIServicePlayerBots* Service);
void UnspawnBotFromWarmup(UAthenaAIServicePlayerBots* Service);
void PlacePlayerBotsInAircraft(UAthenaAIServicePlayerBots* Service, UFortGameStateComponent_BattleRoyaleGamePhaseLogic* GamePhaseLogic);
