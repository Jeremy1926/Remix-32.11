#include "pch.h"
#include "Shared.h"
#include "Config.h"
#pragma comment(lib, "minhook.lib")

void Main()
{
    AllocConsole();
    FILE* Stream;
    freopen_s(&Stream, "CONOUT$", "w", stdout);
    freopen_s(&Stream, "CONOUT$", "w+", stderr);
    freopen_s(&Stream, "CONIN$", "r", stdin);

    static auto GetGameCommandLine = (wchar_t* (*)())(ImageBase + 0x1AE85B0);
    static auto Strncat = (wchar_t* (*)(wchar_t*, const wchar_t*, int32))(ImageBase + 0x29866B8);

    Strncat(GetGameCommandLine(), L" -RequiredPlayers=0", 0x4000);

    Sleep(5000);

    for (auto Command : { L"log LogFortUIDirector None", L"log LogFortUIManager None", L"net.AllowEncryption 0", L"log LogIris None", L"log LogIrisRpc None", L"log LogIrisBridge None" })
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), Command, nullptr);

    *(int32_t*)(ImageBase + 0x12C4E6D0) = 2;
    *(bool*)(ImageBase + 0x12DDB468) = false;
    *(uint32_t*)(ImageBase + 0x12F2F6F0) = 1;

    auto FortInventoryName = FName(L"/Script/FortniteGame.FortInventory");

    for (auto& FilterConfig : UObjectReplicationBridgeConfig::GetDefaultObj()->FilterConfigs)
    {
        if (FilterConfig.ClassName == FortInventoryName)
        {
            FilterConfig.DynamicFilterName.ComparisonIndex = 0;
            break;
        }
    }

    SetConsoleTitleA("Remix 32.11 | Setting up");

    Patch<uint8_t>(ImageBase + 0xA0E8124, 0xC3);
    Patch<uint8_t>(ImageBase + 0x403B258, 0xC3);
    Patch<uint8_t>(ImageBase + 0x4336B38, 0xC3);
    Patch<uint8_t>(ImageBase + 0x4336BAC, 0xC3);
    Patch<uint8_t>(ImageBase + 0xC6A3B28, 0xC3);
    Patch<uint8_t>(ImageBase + 0xA0A47F0, 0xC3);
    Patch<uint32_t>(ImageBase + 0x9EFA710, 0xC0FFC031);
    Patch<uint8_t>(ImageBase + 0x9EFA714, 0xC3);
    Patch<uint32_t>(ImageBase + 0x9F10000, 0xC0FFC031);
    Patch<uint8_t>(ImageBase + 0x9F10004, 0xC3);
    Patch<uint32_t>(ImageBase + 0x9210FC8, 0xC0FFC031);
    Patch<uint8_t>(ImageBase + 0x9210FCC, 0xC3);
    Patch<uint8_t>(ImageBase + 0x1E349CB, 0x85);
    Patch<uint8_t>(ImageBase + 0x3425033, 0xEB);
    Patch<uint16_t>(ImageBase + 0x1B555CB, 0x01B0);
    Patch<uint32_t>(ImageBase + 0x1B555CD, 0x90909090);

    MH_Initialize();

    for (auto& Initter : Initters)
        Initter();

    MH_EnableHook(MH_ALL_HOOKS);

    *(bool*)(ImageBase + 0x12D4E1CA) = false;
    *(bool*)(ImageBase + 0x12D4E166) = true;

    srand((uint32_t)time(0));

    UWorld::GetWorld()->OwningGameInstance->LocalPlayers.Remove(0);

    auto PlaylistAsset = FindObject<UFortPlaylistAthena>(Playlist.c_str());
    auto PersistentLevel = PlaylistAsset ? PlaylistAsset->PreloadPersistentLevel.ObjectID.AssetPath.PackageName : FName();
    std::wstring Open = L"open Apollo_Terrain_Retro";

    if (!PersistentLevel.IsNone())
    {
        auto Level = PersistentLevel.GetRawString();
        Open = L"open " + std::wstring(Level.begin(), Level.end());
    }

    UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), Open.c_str(), nullptr);

    Patch<uint8_t>(ImageBase + 0x727C6C7, 0x84);

    for (auto& PostLoader : PostLoaders)
        PostLoader();

    MH_EnableHook(MH_ALL_HOOKS);

    bInit = true;
}

BOOL APIENTRY DllMain(HMODULE Module, DWORD Reason, LPVOID Reserved)
{
    if (Reason == DLL_PROCESS_ATTACH)
        std::thread(Main).detach();

    return TRUE;
}
