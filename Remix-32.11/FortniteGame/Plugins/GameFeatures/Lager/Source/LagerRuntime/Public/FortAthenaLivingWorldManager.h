#pragma once
#include "pch.h"

template <class T>
static void NotifyWhenPlaylistLoaded(AFortGameStateAthena* GameState, T* Listener)
{
    if (GameState->bPlaylistDataIsLoaded)
    {
        auto Playlist = GameState->CurrentPlaylistInfo.BasePlaylist;
        auto PlaylistContextTags = GameState->GetAthenaPlaylistContextTags();

        Listener->OnCurrentPlaylistLoaded(Playlist ? Playlist->PlaylistName : FName(), PlaylistContextTags);
    }
    else
        GameState->OnCurrentPlaylistLoaded.Bind(Listener, FName(L"OnCurrentPlaylistLoaded"));
}
