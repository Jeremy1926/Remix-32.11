#pragma once
#include "pch.h"

void TournamentPlayerDivision(const wchar_t* AccountId, const wchar_t* Division);
void TournamentPlayerJoined(AFortGameModeAthena* GameMode, AFortPlayerStateAthena* PlayerState);
void TournamentMatchStarted(AFortGameModeAthena* GameMode);
void TournamentPlayerEliminated(AFortGameModeAthena* GameMode, AFortPlayerStateAthena* PlayerState, AFortPlayerStateAthena* KillerPlayerState);
