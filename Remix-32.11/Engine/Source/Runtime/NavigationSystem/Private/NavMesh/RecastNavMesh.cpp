#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteAI/Public/AI/Navigation/AthenaNavSystem.h"

INIT_MODULE(ARecastNavMesh);

static int32 (*GetNewAreaIDOG)(ARecastNavMesh*, const UClass*);
static int32 GetNewAreaID(ARecastNavMesh* NavMesh, const UClass* AreaClass)
{
    auto AreaId = GetNewAreaIDOG(NavMesh, AreaClass);

    if (CachedAreas.empty() || AreaId == 0 || AreaId >= 62)
        return AreaId;

    auto Name = AreaClass->GetName();
    auto CachedId = FindCachedAreaId(std::wstring(Name.begin(), Name.end()));

    if (CachedId > 0 && CachedId < 62 && !IsAreaIdUsed(NavMesh, CachedId))
        return CachedId;

    while (AreaId < 61 && (IsAreaIdUsed(NavMesh, AreaId) || std::any_of(CachedAreas.begin(), CachedAreas.end(), [&](auto& Area) { return Area.second == AreaId; })))
        AreaId++;

    return AreaId;
}

void ARecastNavMesh__Init()
{
    Hook(ImageBase + 0x76ED438, GetNewAreaID, GetNewAreaIDOG);
}
