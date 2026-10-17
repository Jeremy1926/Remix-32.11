#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteAI/Public/AI/Navigation/AthenaNavSystem.h"
#include <fstream>
#include <share.h>

INIT_MODULE(UAthenaNavSystem);
INIT_TICKER(UAthenaNavSystem);

static void (*RemoveNavigationLocksOG)(UAthenaNavSystem*);

static std::fstream CacheStream;
static int64 CacheSize = 0;
static uint32 FortniteMainVersion[10] = {};
static void* CustomVersions[2] = {};
static void* ArchiveVTable[0x2F] = {};
static uint64 Archive[8] = {};
std::vector<std::pair<std::wstring, int32>> CachedAreas;
static std::vector<uint64> CachedLinks;

static FILE* NavDiagFile = nullptr;

static void NavDiag(const char* Format, ...)
{
    if (!NavDiagFile)
    {
        wchar_t LocalAppData[MAX_PATH];

        if (!GetEnvironmentVariableW(L"LOCALAPPDATA", LocalAppData, MAX_PATH))
            return;

        NavDiagFile = _wfsopen((std::wstring(LocalAppData) + L"\\FortniteGame\\Saved\\RemixNavDiag.log").c_str(), L"a", _SH_DENYNO);

        if (!NavDiagFile)
            return;
    }

    SYSTEMTIME Now;
    va_list Args;

    GetLocalTime(&Now);
    fprintf(NavDiagFile, "[%02d:%02d:%02d.%03d pid %lu] ", Now.wHour, Now.wMinute, Now.wSecond, Now.wMilliseconds, GetCurrentProcessId());
    va_start(Args, Format);
    vfprintf(NavDiagFile, Format, Args);
    va_end(Args);
    fputc('\n', NavDiagFile);
    fflush(NavDiagFile);
}

static void ArchiveSerialize(void* Ar, void* Data, int64 Length)
{
    if (!(Archive[5] & 1))
        CacheStream.write((char*)Data, Length);
    else if (!CacheStream.read((char*)Data, Length))
        memset(Data, 0, Length);
}

static void* GetCustomVersions(void* Ar)
{
    return CustomVersions;
}

template <typename T>
static void Serialize(T& Value)
{
    ArchiveSerialize(Archive, &Value, sizeof(T));
}

static bool CanRead(uint64 Bytes)
{
    if (!(Archive[5] & 1) || (CacheStream && Bytes <= uint64(CacheSize - int64(CacheStream.tellg()))))
        return true;

    CacheStream.setstate(std::ios::failbit);

    return false;
}

static void Serialize(std::vector<uint64>& Values)
{
    auto Num = uint32(Values.size());

    Serialize(Num);

    if (!CanRead(uint64(Num) * sizeof(uint64)))
        Num = 0;

    Values.resize(Num);
    ArchiveSerialize(Archive, Values.data(), Num * sizeof(uint64));
}

static void Serialize(std::vector<std::pair<std::wstring, int32>>& Areas)
{
    auto Num = uint32(Areas.size());

    Serialize(Num);

    if (!CanRead(uint64(Num) * (sizeof(uint32) + sizeof(int32))))
        Num = 0;

    Areas.resize(Num);

    for (auto& [Name, Id] : Areas)
    {
        auto Length = uint32(Name.size());

        Serialize(Length);

        if (!CanRead(uint64(Length) * sizeof(wchar_t)))
            Length = 0;

        Name.resize(Length);
        ArchiveSerialize(Archive, Name.data(), Length * sizeof(wchar_t));
        Serialize(Id);
    }
}

static bool SerializeHeader(uint32 Stamp, uint8 (&Params)[0x7C], std::vector<std::pair<std::wstring, int32>>& Areas, std::vector<uint64>& Links, std::vector<uint64>& Columns, int32& Version, int64& PayloadSize)
{
    uint32 Magic = 0x31434E52;
    auto SavedStamp = Stamp;
    uint8 SavedParams[0x7C];

    memcpy(SavedParams, Params, sizeof(SavedParams));

    Serialize(Magic);
    Serialize(SavedStamp);
    Serialize(SavedParams);

    if (!CacheStream || Magic != 0x31434E52 || SavedStamp != Stamp || memcmp(SavedParams, Params, sizeof(SavedParams)))
        return false;

    Serialize(Areas);
    Serialize(Links);
    Serialize(Columns);
    Serialize(Version);
    Serialize(PayloadSize);

    return bool(CacheStream);
}

static bool OpenCache(const std::wstring& Path, bool bLoading)
{
    CacheStream.clear();
    CacheStream.open(Path, std::ios::binary | (bLoading ? std::ios::in | std::ios::ate : std::ios::out | std::ios::trunc));
    Archive[5] = bLoading ? 1 : 4;

    if (bLoading)
    {
        CacheSize = int64(CacheStream.tellg());
        CacheStream.seekg(0);
    }

    return CacheStream.is_open();
}

static std::wstring GetCachePath()
{
    auto World = UWorld::GetWorld();
    auto GameState = (AFortGameStateAthena*)World->GameState;
    auto Playlist = GameState ? GameState->CurrentPlaylistInfo.BasePlaylist : nullptr;
    auto NavSystem = (UNavigationSystemV1*)World->NavigationSystem;
    auto MapName = World->Name.GetRawString();
    wchar_t ModulePath[MAX_PATH];
    HMODULE Module = nullptr;

    if (!Playlist || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&GetCachePath, &Module) || !GetModuleFileNameW(Module, ModulePath, MAX_PATH))
        return std::wstring();

    TArray<ANavMeshBoundsVolume*> Volumes;
    GetAll<ANavMeshBoundsVolume>(Volumes);

    for (auto Volume : Volumes)
    {
        if (Volume && NavSystem && (*(uint32*)&Volume->SupportedAgents & *(uint32*)&NavSystem->SupportedAgentsMask) && Volume->Outer && Volume->Outer->Outer)
        {
            MapName = Volume->Outer->Outer->Name.GetRawString();
            break;
        }
    }

    Volumes.Free();

    auto Name = MapName + "_" + Playlist->PlaylistName.GetRawString() + ".bin";

    auto Folder = std::wstring(ModulePath);

    return Folder.substr(0, Folder.rfind(L'\\')) + L"\\RemixNavCache_" + std::wstring(Name.begin(), Name.end());
}

static void GetParams(ARecastNavMesh* NavMesh, uint8 (&Params)[0x7C])
{
    memcpy(Params, (void*)(uint64(NavMesh) + 0x514), 0x54);
    memcpy(Params + 0x54, (void*)(uint64(NavMesh) + 0x570), 0x28);
}

static uint32 GetImageStamp()
{
    return ((PIMAGE_NT_HEADERS)(ImageBase + ((PIMAGE_DOS_HEADER)ImageBase)->e_lfanew))->FileHeader.TimeDateStamp;
}

template <typename F>
static void ForEachSetElement(uint64 Set, F Callback)
{
    auto Data = *(uint8**)Set;
    auto Bits = *(uint32**)(Set + 0x20) ? *(uint32**)(Set + 0x20) : (uint32*)(Set + 0x10);

    for (int32 i = 0; i < *(int32*)(Set + 0x8); i++)
    {
        if (Bits[i / 32] & (1u << (i % 32)))
            Callback(Data + i * 0x20);
    }
}

static std::vector<std::pair<std::wstring, int32>> GetAreas(ARecastNavMesh* NavMesh)
{
    std::vector<std::pair<std::wstring, int32>> Areas;

    for (auto& Area : NavMesh->SupportedAreas)
        Areas.push_back({ Area.AreaClassName.ToWString(), Area.AreaID });

    return Areas;
}

int32 FindCachedAreaId(const std::wstring& Name)
{
    for (auto& [CachedName, AreaId] : CachedAreas)
    {
        if (CachedName == Name)
            return AreaId;
    }

    return -1;
}

bool IsAreaIdUsed(ARecastNavMesh* NavMesh, int32 AreaId)
{
    for (auto& Area : NavMesh->SupportedAreas)
    {
        if (Area.AreaID == AreaId)
            return true;
    }

    return false;
}

static bool HasCachedAreaNames(ARecastNavMesh* NavMesh)
{
    for (auto& Area : NavMesh->SupportedAreas)
    {
        if (FindCachedAreaId(Area.AreaClassName.ToWString()) < 0)
            return false;
    }

    return true;
}

static void RemapAreas(ARecastNavMesh* NavMesh)
{
    static auto OnNavAreaEvent = (void (*)(ARecastNavMesh*, const UClass*, int32))(ImageBase + 0x76CEB84);

    std::vector<const UClass*> Classes;

    for (auto& Area : NavMesh->SupportedAreas)
    {
        if (Area.AreaClass && FindCachedAreaId(Area.AreaClassName.ToWString()) != Area.AreaID)
            Classes.push_back(Area.AreaClass);
    }

    for (auto Class : Classes)
        OnNavAreaEvent(NavMesh, Class, 1);

    for (auto Class : Classes)
        OnNavAreaEvent(NavMesh, Class, 0);
}

static std::vector<uint64> GetLinks(UNavigationSystemV1* NavSystem)
{
    std::vector<uint64> Links;

    ForEachSetElement(uint64(NavSystem) + 0x470, [&](uint8* Element) { Links.push_back(*(uint64*)Element); });

    return Links;
}

template <typename T>
static bool IsSubset(const std::vector<T>& Runtime, const std::vector<T>& Saved, bool bExact)
{
    if (bExact && Runtime.size() != Saved.size())
        return false;

    for (auto& Entry : Runtime)
    {
        if (std::find(Saved.begin(), Saved.end(), Entry) == Saved.end())
            return false;
    }

    return true;
}

static void RebuildAll(ARecastNavMesh* NavMesh)
{
    auto Generator = *(void**)(uint64(NavMesh) + 0x368);

    ((void (*)(void*))(*(void***)Generator)[0x1])(Generator);
}

static bool AttachCache(UNavigationSystemV1* NavSystem, ARecastNavMesh* NavMesh)
{
    static auto SerializeRecastData = (void (*)(UObject*, void*, int32))(ImageBase + 0x771ACA0);
    static auto AttachTiles = (TArray<uint64>* (*)(UObject*, TArray<uint64>*, ARecastNavMesh*, bool, bool))(ImageBase + 0x7703F98);
    static auto MarkEmptyTileCacheLayers = (void (*)(ARecastNavMesh*, int32, int32))(ImageBase + 0x76F1BC8);

    auto Path = GetCachePath();

    auto bImpl = *(void**)(uint64(NavMesh) + 0x750) != nullptr;
    auto bOpen = !Path.empty() && bImpl && OpenCache(Path, true);

    NavDiag("ATTACH path %ls impl %d open %d size %lld", Path.c_str(), int(bImpl), int(bOpen), bOpen ? CacheSize : -1ll);

    if (!bOpen)
        return false;

    uint8 Params[0x7C];
    std::vector<uint64> Columns;
    int32 Version = 0;
    int64 PayloadSize = 0;
    UObject* Chunk = nullptr;

    GetParams(NavMesh, Params);

    auto bHeader = SerializeHeader(GetImageStamp(), Params, CachedAreas, CachedLinks, Columns, Version, PayloadSize);
    auto bPayload = bHeader && PayloadSize == CacheSize - int64(CacheStream.tellg()) && Version >= 20 && Version <= 26;
    auto RuntimeAreas = GetAreas(NavMesh);
    auto RuntimeLinks = GetLinks(NavSystem);
    auto bAreaNames = bPayload && HasCachedAreaNames(NavMesh);
    auto bAreaIds = bPayload && IsSubset(RuntimeAreas, CachedAreas, false);
    auto bLinks = bPayload && IsSubset(RuntimeLinks, CachedLinks, false);
    int32 Remapped = 0;

    NavDiag("HEADER ok %d payloadOk %d version %d stamp %08X cachedAreas %d cachedLinks %d columns %d runtimeAreas %d runtimeLinks %d areaNamesOk %d areaIdsOk %d linksOk %d",
        int(bHeader), int(bPayload), Version, GetImageStamp(), int(CachedAreas.size()), int(CachedLinks.size()), int(Columns.size()), int(RuntimeAreas.size()), int(RuntimeLinks.size()), int(bAreaNames), int(bAreaIds), int(bLinks));

    for (auto& [Name, AreaId] : RuntimeAreas)
    {
        auto CachedId = FindCachedAreaId(Name);

        if (CachedId != AreaId)
        {
            Remapped++;
            NavDiag("AREA %ls runtime %d cached %d", Name.c_str(), AreaId, CachedId);
        }
    }

    int32 MissingLinks = 0;

    for (auto Link : RuntimeLinks)
    {
        if (std::find(CachedLinks.begin(), CachedLinks.end(), Link) == CachedLinks.end() && MissingLinks++ < 40)
            NavDiag("LINK runtime id 0x%llX not in cache", Link);
    }

    NavDiag("LINKS runtime ids not in cache %d", MissingLinks);

    if (bPayload && bAreaNames && bLinks)
    {
        RemapAreas(NavMesh);
        Chunk = UGameplayStatics::SpawnObject(URecastNavMeshDataChunk::StaticClass(), NavMesh);
        *(FName*)(uint64(Chunk) + 0x28) = NavMesh->Name;
        SerializeRecastData(Chunk, Archive, Version);
    }

    auto bConsumed = CacheStream && int64(CacheStream.tellg()) == CacheSize;

    CacheStream.close();

    auto Tiles = Chunk && bConsumed ? *(int32*)(uint64(Chunk) + 0x38) : 0;

    if (!Tiles)
    {
        NavDiag("RESULT consumed %d tiles 0 attached 0", int(bConsumed));
        return false;
    }

    TArray<uint64> Refs;

    AttachTiles(Chunk, &Refs, NavMesh, false, false);

    auto bAttached = Refs.Num() == Tiles;

    NavDiag("RESULT consumed %d tiles %d refs %d attached %d remappedAreas %d", int(bConsumed), Tiles, Refs.Num(), int(bAttached), Remapped);

    if (!bAttached)
        RebuildAll(NavMesh);
    else
    {
        auto Generator = *(void**)(uint64(NavMesh) + 0x368);

        ((void (*)(void*))(*(void***)Generator)[0x3])(Generator);

        for (auto Column : Columns)
            MarkEmptyTileCacheLayers(NavMesh, int32(Column), int32(Column >> 32));

        ((void (*)(ARecastNavMesh*, TArray<uint64>*))NavMesh->VTable[0x119])(NavMesh, &Refs);
    }

    Refs.Free();

    return bAttached;
}

static void BakeCache(UNavigationSystemV1* NavSystem, ARecastNavMesh* NavMesh)
{
    static auto SerializeRecastData = (void (*)(UObject*, void*, int32))(ImageBase + 0x771ACA0);

    TArray<ANavMeshBoundsVolume*> Volumes;
    FBox Bounds{};

    GetAll<ANavMeshBoundsVolume>(Volumes);

    for (auto Volume : Volumes)
    {
        if (!Volume || !(*(uint32*)&Volume->SupportedAgents & *(uint32*)&NavSystem->SupportedAgentsMask))
            continue;

        FVector Origin, Extent;
        Volume->GetActorBounds(false, &Origin, &Extent, false);

        auto Min = Origin - Extent;
        auto Max = Origin + Extent;

        if (Bounds.IsValid)
        {
            Min = FVector((std::min)(Min.X, Bounds.Min.X), (std::min)(Min.Y, Bounds.Min.Y), (std::min)(Min.Z, Bounds.Min.Z));
            Max = FVector((std::max)(Max.X, Bounds.Max.X), (std::max)(Max.Y, Bounds.Max.Y), (std::max)(Max.Z, Bounds.Max.Z));
        }

        Bounds.Min = Min;
        Bounds.Max = Max;
        Bounds.IsValid = true;
    }

    Volumes.Free();

    auto Path = GetCachePath();
    auto ChunkActor = Bounds.IsValid && !Path.empty() ? SpawnActor<ANavigationDataChunkActor>(FVector{}, FRotator{}) : nullptr;

    if (!ChunkActor)
        return;

    FBox TilesBounds{};

    ((void (*)(ARecastNavMesh*, FBox*, ANavigationDataChunkActor*, FBox*))NavMesh->VTable[0xEC])(NavMesh, &Bounds, ChunkActor, &TilesBounds);

    auto Chunk = ChunkActor->NavDataChunks.Num() > 0 ? ChunkActor->NavDataChunks[0] : nullptr;
    auto TempPath = Path + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";

    NavDiag("BAKE path %ls bounds %.0f %.0f %.0f %.0f %.0f %.0f tiles %d", Path.c_str(), Bounds.Min.X, Bounds.Min.Y, Bounds.Min.Z, Bounds.Max.X, Bounds.Max.Y, Bounds.Max.Z, Chunk ? *(int32*)(uint64(Chunk) + 0x38) : -1);

    if (Chunk && OpenCache(TempPath, false))
    {
        uint8 Params[0x7C];
        auto Areas = GetAreas(NavMesh);
        auto Links = GetLinks(NavSystem);
        std::vector<uint64> Columns;
        int32 Version = 26;
        int64 PayloadSize = 0;

        GetParams(NavMesh, Params);

        ForEachSetElement(*(uint64*)(uint64(NavMesh) + 0x750) + 0x10, [&](uint8* Element)
        {
            if (!*(int32*)(Element + 0x10))
                Columns.push_back(*(uint64*)Element);
        });

        SerializeHeader(GetImageStamp(), Params, Areas, Links, Columns, Version, PayloadSize);

        auto PayloadStart = int64(CacheStream.tellp());

        SerializeRecastData(Chunk, Archive, Version);
        PayloadSize = int64(CacheStream.tellp()) - PayloadStart;
        CacheStream.seekp(PayloadStart - int64(sizeof(PayloadSize)));
        Serialize(PayloadSize);
        CacheStream.close();

        if (CacheStream.fail() || !MoveFileExW(TempPath.c_str(), Path.c_str(), MOVEFILE_REPLACE_EXISTING))
            DeleteFileW(TempPath.c_str());
    }

    ChunkActor->K2_DestroyActor();
}

static void RemoveNavigationLocks(UAthenaNavSystem* NavSystem)
{
    auto NavMesh = (ARecastNavMesh*)NavSystem->MainNavData;

    NavDiag("LOCKS lockBit %d navMesh %d athena %d generator %d", int(*(uint8*)(uint64(NavSystem) + 0x3C8) & 1), NavMesh != nullptr, NavMesh && NavMesh->IsA(AAthenaNavMesh::StaticClass()), NavMesh && *(void**)(uint64(NavMesh) + 0x368));

    if ((*(uint8*)(uint64(NavSystem) + 0x3C8) & 1) && NavMesh && NavMesh->IsA(AAthenaNavMesh::StaticClass()) && *(void**)(uint64(NavMesh) + 0x368))
        bNavCacheAttached = AttachCache(NavSystem, NavMesh);

    RemoveNavigationLocksOG(NavSystem);
}

void UAthenaNavSystem__Init()
{
    memcpy(FortniteMainVersion, (void*)(ImageBase + 0xF230510), 0x10);
    FortniteMainVersion[4] = 170;
    CustomVersions[0] = FortniteMainVersion;
    CustomVersions[1] = (void*)0x100000001;
    ArchiveVTable[0xB] = (void*)GetCustomVersions;
    ArchiveVTable[0x2E] = (void*)ArchiveSerialize;
    Archive[0] = uint64(ArchiveVTable);
    Archive[1] = uint64(&Archive[2]);

    Hook(ImageBase + 0xA8049B4, RemoveNavigationLocks, RemoveNavigationLocksOG);
}

void UAthenaNavSystem__Tick()
{
    static auto OnInitialPlaylistLoad = (void (*)(UNavigationSystemV1*))(ImageBase + 0xA7F6094);

    if (bNavCacheDone)
        return;

    auto World = UWorld::GetWorld();

    if (!World || !World->GameState || !World->AuthorityGameMode || !World->AuthorityGameMode->IsA(AFortGameModeAthena::StaticClass()))
        return;

    auto NavSystem = (UNavigationSystemV1*)World->NavigationSystem;

    if (!NavSystem && !((AFortGameModeAthena*)World->AuthorityGameMode)->bWorldIsReady)
        return;

    if (NavSystem && NavSystem->IsA(UAthenaNavSystem::StaticClass()) && *(uint64*)(uint64(NavSystem) + 0x18C8) && ((AFortGameStateAthena*)World->GameState)->bPlaylistDataIsLoaded)
    {
        NavDiag("PLAYLISTUNLOCK nav system born after the initial playlist load lockBit %d navMesh %d", int(*(uint8*)(uint64(NavSystem) + 0x3C8) & 1), NavSystem->MainNavData != nullptr);
        OnInitialPlaylistLoad(NavSystem);
    }

    auto bMatchStarted = World->GameState->HasMatchStarted();

    if (bNavCacheAttached ? !bMatchStarted : UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World))
        return;

    bNavCacheDone = true;

    auto NavMesh = NavSystem ? (ARecastNavMesh*)NavSystem->MainNavData : nullptr;

    if (!NavMesh || !NavMesh->IsA(AAthenaNavMesh::StaticClass()) || !*(void**)(uint64(NavMesh) + 0x368))
    {
        NavDiag("DONE without athena navmesh navSystem %d navMesh %d world %s", NavSystem != nullptr, NavMesh != nullptr, World->Name.GetRawString().c_str());
        return;
    }

    NavDiag("TICK attached %d matchStarted %d areasEqual %d linksEqual %d runtimeAreas %d runtimeLinks %d", int(bNavCacheAttached), int(bMatchStarted), int(IsSubset(GetAreas(NavMesh), CachedAreas, true)), int(IsSubset(GetLinks(NavSystem), CachedLinks, true)), int(GetAreas(NavMesh).size()), int(GetLinks(NavSystem).size()));

    if (bNavCacheAttached && (!IsSubset(GetAreas(NavMesh), CachedAreas, true) || !IsSubset(GetLinks(NavSystem), CachedLinks, true)))
    {
        NavDiag("LATE MISMATCH rebuild and delete");
        RebuildAll(NavMesh);
        DeleteFileW(GetCachePath().c_str());
        bNavCacheAttached = false;
    }
    else if (!bNavCacheAttached && !bMatchStarted)
    {
        NavDiag("BAKE start");
        BakeCache(NavSystem, NavMesh);
        NavDiag("BAKE done");
    }
}
