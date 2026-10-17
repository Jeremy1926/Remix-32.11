#pragma once
#include "pch.h"

inline bool bNavCacheAttached = false;
inline bool bNavCacheDone = false;

extern std::vector<std::pair<std::wstring, int32>> CachedAreas;

int32 FindCachedAreaId(const std::wstring& Name);
bool IsAreaIdUsed(ARecastNavMesh* NavMesh, int32 AreaId);
