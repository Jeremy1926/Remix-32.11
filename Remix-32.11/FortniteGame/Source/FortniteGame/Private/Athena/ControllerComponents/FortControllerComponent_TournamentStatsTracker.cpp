#include "pch.h"
#include "Shared.h"
#include "Config.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/ControllerComponents/FortControllerComponent_TournamentStatsTracker.h"
#include <objbase.h>
#include <wincrypt.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")

INIT_MODULE(UFortControllerComponent_TournamentStatsTracker);

struct FTournamentPlayer
{
    TWeakObjectPtr<AFortPlayerStateAthena> PlayerState;
    uint8 TeamIndex = 0;
    float EliminatedTime = -1.f;
    bool bReported = false;
};

struct FTournamentRequest
{
    std::wstring Path;
    std::string Body;
};

struct FTournamentWindow
{
    std::wstring EventId;
    std::wstring WindowId;
};

static std::string AccessToken;
static HINTERNET Session = nullptr;

static std::mutex RequestMutex;
static std::condition_variable RequestSignal;
static std::deque<FTournamentRequest> Requests;

static std::unordered_map<std::string, FTournamentPlayer> TournamentPlayers;
static std::unordered_map<std::string, FTournamentWindow> PlayerDivisions;
static std::unordered_map<uint8, int32> TeamEliminations;
static std::wstring MatchId;
static float MatchStartTime = 0.f;
static int32 StartingTeams = 0;
static int32 PlacementReported = 0;

static std::string Utf8(const std::wstring& Text)
{
    auto Size = WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), (int32)Text.size(), nullptr, 0, nullptr, nullptr);
    std::string Result(Size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, Text.c_str(), (int32)Text.size(), Result.data(), Size, nullptr, nullptr);
    return Result;
}

static std::wstring Wide(const std::string& Text)
{
    auto Size = MultiByteToWideChar(CP_UTF8, 0, Text.c_str(), (int32)Text.size(), nullptr, 0);
    std::wstring Result(Size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, Text.c_str(), (int32)Text.size(), Result.data(), Size);
    return Result;
}

static std::string Base64(const std::string& Text)
{
    DWORD Size = 0;
    CryptBinaryToStringA((const BYTE*)Text.data(), (DWORD)Text.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &Size);

    std::string Result(Size, '\0');
    CryptBinaryToStringA((const BYTE*)Text.data(), (DWORD)Text.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, Result.data(), &Size);
    Result.resize(Size);
    return Result;
}

static std::string JsonString(const std::string& Json, const std::string& Key, size_t& Cursor)
{
    auto Pattern = "\"" + Key + "\":\"";
    auto Start = Json.find(Pattern, Cursor);

    if (Start == std::string::npos)
    {
        Cursor = std::string::npos;
        return "";
    }

    Start += Pattern.size();
    auto End = Json.find('"', Start);
    Cursor = End;
    return End == std::string::npos ? "" : Json.substr(Start, End - Start);
}

static DWORD Send(const std::wstring& Path, const std::string& Body, const std::wstring& Headers, std::string& Response)
{
    wchar_t Host[256]{};
    URL_COMPONENTS Url{};
    Url.dwStructSize = sizeof(Url);
    Url.lpszHostName = Host;
    Url.dwHostNameLength = _countof(Host);

    if (!WinHttpCrackUrl(BackendUrl.c_str(), 0, 0, &Url))
        return 0;

    auto Connection = WinHttpConnect(Session, Host, Url.nPort, 0);
    auto Request = Connection ? WinHttpOpenRequest(Connection, L"POST", Path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, Url.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    DWORD Status = 0;

    if (Request && WinHttpSendRequest(Request, Headers.c_str(), (DWORD)-1, (LPVOID)Body.data(), (DWORD)Body.size(), (DWORD)Body.size(), 0) && WinHttpReceiveResponse(Request, nullptr))
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

static bool Login()
{
    auto Body = std::string("grant_type=client_credentials");
    auto Headers = L"Content-Type: application/x-www-form-urlencoded\r\nAuthorization: basic " + Wide(Base64(Utf8(ClientId + L":" + ClientSecret)));

    std::string Response;
    size_t Cursor = 0;
    AccessToken = Send(L"/account/api/oauth/token", Body, Headers, Response) == 200 ? JsonString(Response, "access_token", Cursor) : "";
    return !AccessToken.empty();
}

static void TournamentWorker()
{
    while (true)
    {
        FTournamentRequest Request;

        {
            std::unique_lock Lock(RequestMutex);
            RequestSignal.wait(Lock, [] { return !Requests.empty(); });
            Request = std::move(Requests.front());
            Requests.pop_front();
        }

        std::string Response;
        DWORD Status = 0;

        for (int32 Attempt = 0; Attempt < 3 && Status != 200; Attempt++)
        {
            if (AccessToken.empty() && !Login())
            {
                Sleep(2000);
                continue;
            }

            Status = Send(Request.Path, Request.Body, L"Content-Type: application/json\r\nAuthorization: bearer " + Wide(AccessToken), Response);

            if (Status == 401)
                AccessToken.clear();
            else if (Status == 0 || Status >= 500)
                Sleep(2000);
            else if (Status != 200)
                break;
        }
    }
}

static void Queue(const std::wstring& Route, std::string Body)
{
    if (!Session)
        return;

    {
        std::lock_guard Lock(RequestMutex);
        Requests.push_back({ std::wstring(L"/api/v1/events/Fortnite/gameserver/matches/") + MatchId + Route, std::move(Body) });
    }

    RequestSignal.notify_one();
}

static std::string GetAccountId(AFortPlayerStateAthena* PlayerState)
{
    auto DebugString = UFortKismetLibrary::GetDebugStringForUniqueId(PlayerState->UniqueID);
    auto Text = DebugString.ToString();
    DebugString.Free();

    if (Text.size() < 32)
        return "";

    auto AccountId = Text.substr(Text.size() - 32);

    for (auto& Char : AccountId)
    {
        if (!isxdigit((uint8)Char))
            return "";

        Char = (char)tolower((uint8)Char);
    }

    return AccountId;
}

static float GetMatchTime()
{
    return (float)UGameplayStatics::GetTimeSeconds(UWorld::GetWorld()) - MatchStartTime;
}

static FEventTournamentIds& GetEventTournamentIds(AFortGameModeAthena* GameMode)
{
    return *(FEventTournamentIds*)(__int64(GameMode) + 0xE78);
}

static bool IsTournamentMatch(AFortGameModeAthena* GameMode)
{
    auto& EventTournamentIds = GetEventTournamentIds(GameMode);

    return !PlayerDivisions.empty() || (EventTournamentIds.EventId.Num() > 1 && EventTournamentIds.WindowId.Num() > 1);
}

static bool GetTournamentWindow(AFortGameModeAthena* GameMode, AFortPlayerStateAthena* PlayerState, FTournamentWindow& Window)
{
    auto Found = PlayerDivisions.find(GetAccountId(PlayerState));

    if (Found != PlayerDivisions.end())
    {
        Window = Found->second;
        return true;
    }

    auto& EventTournamentIds = GetEventTournamentIds(GameMode);

    if (EventTournamentIds.EventId.Num() <= 1 || EventTournamentIds.WindowId.Num() <= 1)
        return false;

    Window = { EventTournamentIds.EventId.Data, EventTournamentIds.WindowId.Data };
    return true;
}

static void UpdateMatchAbandonState(AFortPlayerStateAthena* PlayerState)
{
    auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(UWorld::GetWorld());

    if (GamePhaseLogic)
        ((void (*)(UFortGameStateComponent_BattleRoyaleGamePhaseLogic*, AFortPlayerStateAthena*))GamePhaseLogic->VTable[0xC6])(GamePhaseLogic, PlayerState);
}

static void ReportPlacementPoints(AFortGameModeAthena* GameMode, const std::set<uint8>& AliveTeams)
{
    static auto FindTournamentSession = (FSharedRef* (*)(UFortTournamentManager*, FSharedRef*, const FString*, const FString*))(ImageBase + 0xA108D5C);
    static auto GetEventTemplate = (FSharedRef* (*)(void*, FSharedRef*))(ImageBase + 0xA10F858);
    static auto GetPlacementPointsChange = (int32 (*)(void*, void*, int32, int32))(ImageBase + 0xA111848);
    static auto TSharedRef_Destructor = (void (*)(FSharedRef*))(ImageBase + 0x14EAAD8);

    auto GameState = (AFortGameStateAthena*)GameMode->GameState;
    auto Placement = GameState->TeamsLeft;

    if (Placement < 1 || Placement >= PlacementReported)
        return;

    auto GameInstance = (UFortGameInstance*)UWorld::GetWorld()->OwningGameInstance;

    for (auto& Player : GameState->PlayerArray)
    {
        auto PlayerState = (AFortPlayerStateAthena*)Player;
        FTournamentWindow Window;

        if (!PlayerState || !PlayerState->IsA(AFortPlayerStateAthena::StaticClass()) || !AliveTeams.contains(PlayerState->TeamIndex) || !PlayerState->Owner || !PlayerState->Owner->IsA(AFortPlayerControllerAthena::StaticClass()) || !GetTournamentWindow(GameMode, PlayerState, Window))
            continue;

        FString EventId(Window.EventId.c_str());
        FString WindowId(Window.WindowId.c_str());
        FSharedRef TournamentSession{};
        FSharedRef Template{};

        FindTournamentSession(GameInstance->TournamentManager, &TournamentSession, &EventId, &WindowId);

        if (TournamentSession.Object)
            GetEventTemplate(TournamentSession.Object, &Template);

        for (auto Place = PlacementReported - 1; Template.Object && Place >= Placement; Place--)
        {
            auto PointsEarned = GetPlacementPointsChange(Template.Object, nullptr, Place + 1, Place);

            if (PointsEarned > 0)
                ((AFortPlayerControllerAthena*)PlayerState->Owner)->ClientReportTournamentPlacementPointsScored(Place, PointsEarned);
        }

        TSharedRef_Destructor(&Template);
        TSharedRef_Destructor(&TournamentSession);
        EventId.Free();
        WindowId.Free();
    }

    PlacementReported = Placement;
}

static void BuildAndSendTournamentStats(uint8 TeamIndex, int32 Placement)
{
    std::string Results;

    for (auto& [AccountId, Player] : TournamentPlayers)
    {
        if (Player.TeamIndex != TeamIndex || Player.bReported)
            continue;

        Player.bReported = true;
        auto TimeAlive = Player.EliminatedTime >= 0.f ? Player.EliminatedTime : GetMatchTime();

        Results += std::format(R"({}{{"accountId":"{}","trackedStats":{{"PLACEMENT_STAT_INDEX":{},"TEAM_ELIMS_STAT_INDEX":{},"VICTORY_ROYALE_STAT":{},"TIME_ALIVE_STAT":{},"MATCH_PLAYED_STAT":1,"PLACEMENT_TIEBREAKER_STAT":{}}}}})",
                               Results.empty() ? "" : ",", AccountId, Placement, TeamEliminations[TeamIndex], Placement == 1 ? 1 : 0, (int32)TimeAlive, StartingTeams > Placement ? StartingTeams - Placement : 0);
    }

    if (!Results.empty())
        Queue(L"/results", R"({"results":[)" + Results + "]}");
}

static std::set<uint8> GetAliveTeams(AFortGameModeAthena* GameMode)
{
    std::set<uint8> AliveTeams;

    for (auto& PlayerController : GameMode->AlivePlayers)
    {
        if (PlayerController && PlayerController->PlayerState)
            AliveTeams.insert(((AFortPlayerStateAthena*)PlayerController->PlayerState)->TeamIndex);
    }

    for (auto& BotController : GameMode->AliveBots)
    {
        if (BotController && BotController->PlayerState)
            AliveTeams.insert(((AFortPlayerStateAthena*)BotController->PlayerState)->TeamIndex);
    }

    return AliveTeams;
}

void TournamentPlayerDivision(const wchar_t* AccountId, const wchar_t* Division)
{
    std::wstring Text = Division;
    auto Separator = Text.find(L':');

    if (Separator == std::wstring::npos || Separator == 0 || Separator + 1 >= Text.size())
        return;

    auto Account = Utf8(AccountId);

    for (auto& Char : Account)
        Char = (char)tolower((uint8)Char);

    PlayerDivisions[Account] = { Text.substr(0, Separator), Text.substr(Separator + 1, Text.find(L':', Separator + 1) - Separator - 1) };
}

void TournamentPlayerJoined(AFortGameModeAthena* GameMode, AFortPlayerStateAthena* PlayerState)
{
    FTournamentWindow Window;

    if (!PlayerState || !GetTournamentWindow(GameMode, PlayerState, Window))
        return;

    FEventTournamentIds EventTournamentIds{};
    EventTournamentIds.EventId = FString(Window.EventId.c_str());
    EventTournamentIds.WindowId = FString(Window.WindowId.c_str());

    PlayerState->ClientNotifyMatchEntered(EventTournamentIds);
    UpdateMatchAbandonState(PlayerState);

    EventTournamentIds.EventId.Free();
    EventTournamentIds.WindowId.Free();
}

void TournamentMatchStarted(AFortGameModeAthena* GameMode)
{
    if (!IsTournamentMatch(GameMode))
        return;

    for (auto& Player : GameMode->GameState->PlayerArray)
    {
        FTournamentWindow Window;

        if (Player && Player->IsA(AFortPlayerStateAthena::StaticClass()) && GetTournamentWindow(GameMode, (AFortPlayerStateAthena*)Player, Window))
            UpdateMatchAbandonState((AFortPlayerStateAthena*)Player);
    }

    GUID Guid{};
    CoCreateGuid(&Guid);
    MatchId = Wide(std::format("{:08x}{:04x}{:04x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}", Guid.Data1, Guid.Data2, Guid.Data3, Guid.Data4[0], Guid.Data4[1], Guid.Data4[2], Guid.Data4[3], Guid.Data4[4], Guid.Data4[5], Guid.Data4[6], Guid.Data4[7]));
    MatchStartTime = (float)UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
    TournamentPlayers.clear();
    TeamEliminations.clear();

    for (auto& PlayerController : GameMode->AlivePlayers)
    {
        auto PlayerState = PlayerController ? (AFortPlayerStateAthena*)PlayerController->PlayerState : nullptr;
        auto AccountId = PlayerState ? GetAccountId(PlayerState) : "";

        if (!AccountId.empty() && !TournamentPlayers.contains(AccountId))
            TournamentPlayers[AccountId] = { TWeakObjectPtr<AFortPlayerStateAthena>(PlayerState), PlayerState->TeamIndex };
    }

    StartingTeams = (int32)GetAliveTeams(GameMode).size();
    PlacementReported = ((AFortGameStateAthena*)GameMode->GameState)->TeamsLeft;
}

void TournamentPlayerEliminated(AFortGameModeAthena* GameMode, AFortPlayerStateAthena* PlayerState, AFortPlayerStateAthena* KillerPlayerState)
{
    if (MatchId.empty() || !PlayerState)
        return;

    auto Found = TournamentPlayers.find(GetAccountId(PlayerState));

    if (Found != TournamentPlayers.end() && Found->second.EliminatedTime < 0.f)
        Found->second.EliminatedTime = GetMatchTime();

    if (KillerPlayerState && KillerPlayerState->TeamIndex != PlayerState->TeamIndex)
        TeamEliminations[KillerPlayerState->TeamIndex]++;

    auto AliveTeams = GetAliveTeams(GameMode);

    if (!AliveTeams.contains(PlayerState->TeamIndex))
    {
        BuildAndSendTournamentStats(PlayerState->TeamIndex, (int32)AliveTeams.size() + 1);
        ReportPlacementPoints(GameMode, AliveTeams);
    }

    if (AliveTeams.size() == 1)
        BuildAndSendTournamentStats(*AliveTeams.begin(), 1);
}

void UFortControllerComponent_TournamentStatsTracker__Init()
{
    Session = WinHttpOpen(L"Remix-GS", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if (Session)
        std::thread(TournamentWorker).detach();
}
