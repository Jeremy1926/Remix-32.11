#pragma once

inline bool bReload = false;
inline std::wstring ReloadPlaylist = L"/BlastBerry/Playlists/Playlist_PunchBerrySolo.Playlist_PunchBerrySolo";
inline std::wstring Playlist = bReload ? ReloadPlaylist : L"/BRPlaylists/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo";
inline bool bInit = false;
inline bool bReady = false;
inline bool bEnableZones = true;
inline bool bForceRespawns = false;
inline bool bStormRespawns = false;
inline bool bLateGame = false;
inline int LateGameZone = 3;
inline bool bLateGameLongZone = true;
inline bool bLateGameMovingBus = true;
inline std::vector<std::pair<std::wstring, int>> LateGameLoadout;
inline bool bRecordReplays = true;
inline bool bPlayerBots = true;
inline std::wstring BackendUrl;
inline std::wstring ClientId;
inline std::wstring ClientSecret;
