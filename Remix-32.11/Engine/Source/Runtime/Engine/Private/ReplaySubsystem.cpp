#include "pch.h"
#include "Shared.h"
#include "Config.h"
#include "Engine/Source/Runtime/Engine/Public/ReplaySubsystem.h"
#include <shlobj.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

INIT_MODULE(UReplaySubsystem);
INIT_TICKER(UReplaySubsystem);

static HINTERNET Session = nullptr;
static bool bStreamerSet = false;
static int32 MostTeams = 0;

struct FMatchResult
{
    std::wstring AccountId;
    int32 Kills = 0;
    int32 Place = 0;
    int32 Players = 0;
};

static std::mutex ResultMutex;
static std::vector<FMatchResult> Results;
static std::unordered_set<std::wstring> Recorded;

static std::string Utf8(const std::wstring& Text)
{
    auto Size = WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), (int32)Text.size(), nullptr, 0, nullptr, nullptr);
    std::string Result(Size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), (int32)Text.size(), Result.data(), Size, nullptr, nullptr);
    return Result;
}

static std::wstring Base64(const std::string& Text)
{
    DWORD Size = 0;
    CryptBinaryToStringW((const BYTE*)Text.data(), (DWORD)Text.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &Size);

    std::wstring Result(Size, L'\0');
    CryptBinaryToStringW((const BYTE*)Text.data(), (DWORD)Text.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, Result.data(), &Size);
    Result.resize(Size);
    return Result;
}

static DWORD Send(const wchar_t* Verb, const std::wstring& Path, const std::wstring& Headers, const void* Body, DWORD BodySize, std::string& Response)
{
    wchar_t Host[256]{};
    URL_COMPONENTS Parts{};
    Parts.dwStructSize = sizeof(Parts);
    Parts.lpszHostName = Host;
    Parts.dwHostNameLength = _countof(Host);

    if (!WinHttpCrackUrl(BackendUrl.c_str(), 0, 0, &Parts))
        return 0;

    auto Connection = WinHttpConnect(Session, Host, Parts.nPort, 0);
    auto Request = Connection ? WinHttpOpenRequest(Connection, Verb, Path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, Parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    DWORD Status = 0;

    if (Request && WinHttpSendRequest(Request, Headers.c_str(), (DWORD)-1, (LPVOID)Body, BodySize, BodySize, 0) && WinHttpReceiveResponse(Request, nullptr))
    {
        DWORD Size = sizeof(Status);
        WinHttpQueryHeaders(Request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &Status, &Size, WINHTTP_NO_HEADER_INDEX);

        Response.clear();
        DWORD Available = 0;
        while (WinHttpQueryDataAvailable(Request, &Available) && Available > 0)
        {
            auto Offset = Response.size();
            Response.resize(Offset + Available);

            DWORD Read = 0;
            WinHttpReadData(Request, Response.data() + Offset, Available, &Read);
            Response.resize(Offset + Read);
        }
    }

    if (Request)
        WinHttpCloseHandle(Request);

    if (Connection)
        WinHttpCloseHandle(Connection);

    return Status;
}

static std::wstring AccessToken()
{
    std::string Response;
    auto Body = std::string("grant_type=client_credentials");
    auto Headers = L"Content-Type: application/x-www-form-urlencoded\r\nAuthorization: basic " + Base64(Utf8(ClientId + L":" + ClientSecret));

    if (Send(L"POST", L"/account/api/oauth/token", Headers, Body.data(), (DWORD)Body.size(), Response) != 200)
        return L"";

    auto Pattern = std::string("\"access_token\":\"");
    auto Start = Response.find(Pattern);
    auto End = Start == std::string::npos ? std::string::npos : Response.find('"', Start + Pattern.size());

    if (End == std::string::npos)
        return L"";

    auto Token = Response.substr(Start + Pattern.size(), End - Start - Pattern.size());
    return std::wstring(Token.begin(), Token.end());
}

static std::wstring DemosDirectory()
{
    PWSTR LocalAppData = nullptr;
    std::wstring Directory;

    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &LocalAppData)))
        Directory = std::wstring(LocalAppData) + L"\\FortniteGame\\Saved\\Demos";

    CoTaskMemFree(LocalAppData);
    return Directory;
}

static bool SessionName(std::wstring Name, std::wstring& SessionId)
{
    if (Name.size() != 32 + 7 || _wcsicmp(Name.c_str() + 32, L".replay") != 0)
        return false;

    Name.resize(32);
    for (auto& Char : Name)
    {
        Char = towlower(Char);
        if (!iswxdigit(Char))
            return false;
    }

    SessionId = Name;
    return true;
}

static bool UploadReplay(const std::wstring& File, const std::wstring& SessionId, const std::wstring& Token)
{
    auto Handle = CreateFileW(File.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

    if (Handle == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER FileSize{};
    BCRYPT_ALG_HANDLE Algorithm = nullptr;
    BCRYPT_HASH_HANDLE Hash = nullptr;
    std::vector<uint8> Buffer(32 * 1024 * 1024);
    auto Base = L"/fortnite/api/game/v2/replays/" + SessionId;
    auto Headers = L"Authorization: bearer " + Token + L"\r\nContent-Type: application/octet-stream";
    std::string Response;
    bool bDone = false;
    bool bFailed = !GetFileSizeEx(Handle, &FileSize) || FileSize.QuadPart <= 0 || BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0 || BCryptCreateHash(Algorithm, &Hash, nullptr, 0, nullptr, 0, 0) != 0;

    for (int64 Offset = 0; !bFailed && !bDone && Offset < FileSize.QuadPart;)
    {
        DWORD Read = 0;

        if (!ReadFile(Handle, Buffer.data(), (DWORD)std::min<int64>(32 * 1024 * 1024, FileSize.QuadPart - Offset), &Read, nullptr) || Read == 0 || BCryptHashData(Hash, Buffer.data(), Read, 0) != 0)
        {
            bFailed = true;
            break;
        }

        auto Status = Send(L"PUT", Base + L"?offset=" + std::to_wstring(Offset), Headers, Buffer.data(), Read, Response);

        if (Status == 409 && Response.find("replays.exists") != std::string::npos)
            bDone = true;
        else if (Status == 200)
            Offset += Read;
        else
            bFailed = true;
    }

    if (!bFailed && !bDone)
    {
        uint8 Digest[32]{};
        std::string Hex;

        if (BCryptFinishHash(Hash, Digest, sizeof(Digest), 0) == 0)
        {
            for (auto Byte : Digest)
                Hex += std::format("{:02x}", Byte);

            auto Body = std::format("{{\"size\":{},\"sha256\":\"{}\"}}", FileSize.QuadPart, Hex);
            auto Status = Send(L"POST", Base + L"/complete", L"Authorization: bearer " + Token + L"\r\nContent-Type: application/json", Body.data(), (DWORD)Body.size(), Response);
            bDone = Status == 200 || (Status == 409 && Response.find("replays.exists") != std::string::npos);
        }
    }

    if (Hash)
        BCryptDestroyHash(Hash);

    if (Algorithm)
        BCryptCloseAlgorithmProvider(Algorithm, 0);

    CloseHandle(Handle);

    if (bDone)
        DeleteFileW(File.c_str());

    return bDone;
}

static void UploadWorker()
{
    auto Directory = DemosDirectory();

    while (!Directory.empty())
    {
        std::vector<std::pair<std::wstring, std::wstring>> Finished;
        WIN32_FIND_DATAW Entry{};
        auto Find = FindFirstFileW((Directory + L"\\*.replay").c_str(), &Entry);

        if (Find != INVALID_HANDLE_VALUE)
        {
            FILETIME Now{};
            GetSystemTimeAsFileTime(&Now);
            auto NowTicks = ULARGE_INTEGER{ Now.dwLowDateTime, Now.dwHighDateTime }.QuadPart;

            do
            {
                std::wstring SessionId;
                auto Written = ULARGE_INTEGER{ Entry.ftLastWriteTime.dwLowDateTime, Entry.ftLastWriteTime.dwHighDateTime }.QuadPart;

                if (!(Entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && SessionName(Entry.cFileName, SessionId) && NowTicks > Written + 120ull * 10000000ull)
                    Finished.push_back({ Directory + L"\\" + Entry.cFileName, SessionId });
            } while (FindNextFileW(Find, &Entry));

            FindClose(Find);
        }

        std::vector<FMatchResult> Ready;

        {
            std::lock_guard Lock(ResultMutex);
            Ready.swap(Results);
        }

        auto Token = Finished.empty() && Ready.empty() ? L"" : AccessToken();

        for (auto& [File, SessionId] : Finished)
        {
            if (!Token.empty())
                UploadReplay(File, SessionId, Token);
        }

        for (auto& Result : Ready)
        {
            std::string Response;
            auto Body = std::format("{{\"kills\":{},\"place\":{},\"players\":{}}}", Result.Kills, Result.Place, Result.Players);
            auto Status = Token.empty() ? 0 : Send(L"PUT", L"/fortnite/api/game/v2/matches/players/" + Result.AccountId + L"/result", L"Authorization: bearer " + Token + L"\r\nContent-Type: application/json", Body.data(), (DWORD)Body.size(), Response);

            if (Status == 0 || Status >= 500)
            {
                std::lock_guard Lock(ResultMutex);
                Results.push_back(Result);
            }
        }

        Sleep(30000);
    }
}

void RecordMatchResult(AFortPlayerControllerAthena* PlayerController)
{
    static auto ToString = (FString* (*)(const FUniqueNetIdRepl*, FString*))(ImageBase + 0x35E21FC);

    auto PlayerState = PlayerController ? (AFortPlayerStateAthena*)PlayerController->PlayerState : nullptr;

    if (!Session || !PlayerState || PlayerState->Place <= 0)
        return;

    FString AccountId;
    ToString(&PlayerState->UniqueID, &AccountId);

    FMatchResult Result{ AccountId.ToWString(), PlayerState->KillScore, PlayerState->Place, (std::max)(MostTeams, PlayerState->Place) };
    AccountId.Free();

    for (auto& Char : Result.AccountId)
        Char = towlower(Char);

    if (Result.AccountId.size() != 32 || !Recorded.insert(Result.AccountId).second)
        return;

    std::lock_guard Lock(ResultMutex);
    Results.push_back(Result);
}

void UReplaySubsystem__Init()
{
    if (!bRecordReplays)
        return;

    UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"Fort.AutomaticReplayRecordingBR 1", nullptr);
    Session = WinHttpOpen(L"Remix-GS", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if (Session)
        std::thread(UploadWorker).detach();
}

void UReplaySubsystem__Tick()
{
    if (!Session)
        return;

    if (auto GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState)
        MostTeams = (std::max)(MostTeams, GameState->TeamsLeft);

    if (bStreamerSet)
        return;

    auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;

    if (GameMode)
    {
        GameMode->ReplayStreamerOverride = FString(L"LocalFileNetworkReplayStreaming");
        bStreamerSet = true;
    }
}
