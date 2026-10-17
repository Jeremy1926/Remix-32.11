#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/FortGameModeZone.h"

INIT_MODULE(AFortGameModeZone);
INIT_TICKER(AFortGameModeZone);

struct FVoiceChatTokenRequest
{
    TWeakObjectPtr<AFortPlayerControllerZone> PlayerController;
    int32 RequestID;
    FString ChannelName;
    FSharedRef HttpRequest;
};

static std::vector<FVoiceChatTokenRequest> VoiceChatTokenRequests;
static std::wstring CurrentGameSessionId;

static const std::wstring& GetCurrentGameSessionId()
{
    if (CurrentGameSessionId.empty())
    {
        std::random_device Device;
        CurrentGameSessionId = std::format(L"{:08x}{:08x}{:08x}{:08x}", Device(), Device(), Device(), Device());
    }

    return CurrentGameSessionId;
}

static std::wstring GenerateVoiceChatChannel(AFortGameModeAthena* GameMode, AFortPlayerControllerZone* PlayerController)
{
    static auto EvaluateVoiceChannelMutators = (bool (*)(AFortGameModeAthena*, FString*, AFortPlayerControllerZone*))(ImageBase + 0x917F200);
    static auto GetCurrentPlaylistData = (UFortPlaylistAthena* (*)(AFortGameStateAthena*))(ImageBase + 0x4B6DB14);
    static auto AsBool = (bool (*)(const FScalableFloat*, float))(ImageBase + 0x1EECCB4);
    static auto& EnableVoiceChannelMutatorUsage = *(int32*)(ImageBase + 0x12C5D6EC);

    auto GameState = (AFortGameStateAthena*)GameMode->GameState;
    auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;

    if (!GameState || !GameState->IsA(AFortGameStateAthena::StaticClass()) || !PlayerState || !PlayerState->IsA(AFortPlayerStateAthena::StaticClass()))
        return L"";

    auto& GameSessionId = GetCurrentGameSessionId();
    FString MutatorChannel;
    auto bMutatorChannel = EnableVoiceChannelMutatorUsage && EvaluateVoiceChannelMutators(GameMode, &MutatorChannel, PlayerController);
    auto MutatorChannelName = MutatorChannel.Num() > 1 ? MutatorChannel.ToWString() : L"";

    MutatorChannel.Free();

    if (bMutatorChannel)
        return MutatorChannelName.empty() ? L"" : GameSessionId + L"." + MutatorChannelName;

    auto Playlist = GetCurrentPlaylistData(GameState);

    if ((Playlist && Playlist->bForceDisableTeamOrSquadGameChannel) || PlayerState->bOnlySpectator)
        return L"";

    auto CustomGameChannel = Playlist && AsBool(&Playlist->ShouldUseCustomGameChannel, 0.f) ? Playlist->CustomGameChannel : ECustomGameVoiceChannel::Squad;

    if (CustomGameChannel == ECustomGameVoiceChannel::WholeServer)
        return GameSessionId + L".WholeServer";

    if (CustomGameChannel == ECustomGameVoiceChannel::FullTeam)
        return GameSessionId + L"." + std::to_wstring(PlayerState->TeamIndex);

    int32 NumberOfPlayersInSquad = 0;

    for (auto& Member : GameState->GameMemberInfoArray.Members)
    {
        if (Member.SquadID == PlayerState->SquadID)
            NumberOfPlayersInSquad++;
    }

    return NumberOfPlayersInSquad > 1 ? GameSessionId + L"." + std::to_wstring(PlayerState->SquadID) : L"";
}

void SendVoiceChatChannelJoin(AFortGameModeZone* GameMode, AFortPlayerControllerZone* PlayerController)
{
    static auto VoiceChatChannelName = FName(L"VoiceChatChannel");

    if (!PlayerController)
        return;

    auto Channel = GenerateVoiceChatChannel((AFortGameModeAthena*)GameMode, PlayerController);

    if (Channel == PlayerController->VoiceChatChannel.ToWString())
        return;

    PlayerController->VoiceChatChannel.Free();
    PlayerController->VoiceChatChannel = Channel.empty() ? FString() : FString(Channel.c_str());
    UNetPushModelHelpers::MarkPropertyDirty(PlayerController, VoiceChatChannelName);
}

static void SendVoiceChatToken(AFortGameModeZone*, AFortPlayerControllerZone* PlayerController, int32 RequestID, bool, const FString& ChannelName)
{
    static auto IOnlineSubsystem_Get = (void* (*)(const FName&))(ImageBase + 0x21048AC);
    static auto GetMcpGameService = (FSharedRef* (*)(void*, FSharedRef*))(ImageBase + 0x1F60394);
    static auto GetIdentityInterface = (FSharedRef* (*)(void*, FSharedRef*))(ImageBase + 0x2C7C0F0);
    static auto ProcessRequest = (void (*)(void*, void*, const void*, const FSharedRef*))(ImageBase + 0x762A4A0);
    static auto ToString = (FString* (*)(const FUniqueNetIdRepl*, FString*))(ImageBase + 0x35E21FC);
    static auto TSharedRef_Destructor = (void (*)(FSharedRef*))(ImageBase + 0x14EAAD8);
    static auto& McpSubsystemName = *(FName*)(ImageBase + 0x12ED97E8);

    if (!PlayerController)
        return;

    auto PlayerState = PlayerController->PlayerState;
    auto& VoiceChatChannel = PlayerController->VoiceChatChannel;
    auto Subsystem = PlayerState && VoiceChatChannel.Num() > 1 ? IOnlineSubsystem_Get(McpSubsystemName) : nullptr;
    FSharedRef GameService{};
    FSharedRef Identity{};
    FSharedRef UserId{};

    if (Subsystem)
    {
        GetMcpGameService(Subsystem, &GameService);
        GetIdentityInterface(Subsystem, &Identity);
    }

    if (Identity.Object)
    {
        auto GetUniquePlayerId = (FSharedRef* (*)(void*, FSharedRef*, int32))(*(void***)Identity.Object)[0x22];

        GetUniquePlayerId(Identity.Object, &UserId, 0);
    }

    if (GameService.Object && UserId.Object)
    {
        auto CreateRequest = (FSharedRef* (*)(void*, FSharedRef*))(*(void***)GameService.Object)[0x1B];
        auto GetBaseUrl = (FString* (*)(void*, FString*))(*(void***)GameService.Object)[0xB];
        FSharedRef HttpRequest{};
        FString BaseUrl;
        FString AccountId;

        CreateRequest(GameService.Object, &HttpRequest);
        GetBaseUrl(GameService.Object, &BaseUrl);
        ToString(&PlayerState->UniqueID, &AccountId);

        auto HttpRequestVTable = *(void***)HttpRequest.Object;
        auto SetVerb = (void (*)(void*, const FString&))HttpRequestVTable[0xC];
        auto SetURL = (void (*)(void*, const FString&))HttpRequestVTable[0xD];
        auto SetContentAsString = (void (*)(void*, const FString&))HttpRequestVTable[0x12];
        auto SetHeader = (void (*)(void*, const FString&, const FString&))HttpRequestVTable[0x16];
        auto URL = FString((BaseUrl.ToWString() + L"/api/game/v2/voice/" + AccountId.ToWString() + L"/dedicated_server/createJoinToken").c_str());
        auto Content = FString((L"{\"channelName\":\"" + VoiceChatChannel.ToWString() + L"\"}").c_str());
        auto HeaderName = FString(L"Content-Type");
        auto HeaderValue = FString(L"application/json");
        auto Verb = FString(L"POST");

        SetURL(HttpRequest.Object, URL);
        SetContentAsString(HttpRequest.Object, Content);
        SetHeader(HttpRequest.Object, HeaderName, HeaderValue);
        SetVerb(HttpRequest.Object, Verb);
        ProcessRequest(Subsystem, GameService.Object, UserId.Object, &HttpRequest);

        VoiceChatTokenRequests.push_back({ PlayerController, RequestID, FString(VoiceChatChannel.CStr()), HttpRequest });

        Verb.Free();
        HeaderValue.Free();
        HeaderName.Free();
        Content.Free();
        URL.Free();
        AccountId.Free();
        BaseUrl.Free();
    }
    else
    {
        auto Error = FString(L"disabled");

        PlayerController->ClientVoiceChatSendJoinToken(RequestID, false, Error, FString(), ChannelName);
        Error.Free();
    }

    TSharedRef_Destructor(&UserId);
    TSharedRef_Destructor(&Identity);
    TSharedRef_Destructor(&GameService);
}

static void OnHttpRequestJoinTokenComplete(FVoiceChatTokenRequest& Request, bool bSucceeded)
{
    static auto TJsonReaderFactory_Create = (FSharedRef* (*)(FSharedRef*, const FString*))(ImageBase + 0x27DF56C);
    static auto FJsonSerializer_Deserialize = (bool (*)(void*, FSharedRef*, int32))(ImageBase + 0x22CB314);
    static auto TryGetStringField = (bool (*)(void*, const FJsonKey*, FString*))(ImageBase + 0x22C9958);
    static auto TSharedRef_Destructor = (void (*)(FSharedRef*))(ImageBase + 0x14EAAD8);
    alignas(16) static const FJsonKey TokenField{ L"token", 5 };
    alignas(16) static const FJsonKey ErrorCodeField{ L"errorCode", 9 };

    FString Token;
    FString Error;
    FSharedRef Response{};

    if (bSucceeded)
    {
        auto GetResponse = (FSharedRef* (*)(void*, FSharedRef*))(*(void***)Request.HttpRequest.Object)[0x24];

        GetResponse(Request.HttpRequest.Object, &Response);
    }

    if (Response.Object)
    {
        auto GetContentAsString = (FString* (*)(void*, FString*))(*(void***)Response.Object)[0xC];
        FString Content;
        FSharedRef JsonReader{};
        FSharedRef JsonObject{};

        GetContentAsString(Response.Object, &Content);
        TJsonReaderFactory_Create(&JsonReader, &Content);

        if (FJsonSerializer_Deserialize(JsonReader.Object, &JsonObject, 0) && JsonObject.Object && !TryGetStringField(JsonObject.Object, &TokenField, &Token))
            TryGetStringField(JsonObject.Object, &ErrorCodeField, &Error);

        TSharedRef_Destructor(&JsonObject);
        TSharedRef_Destructor(&JsonReader);
        Content.Free();
    }

    auto bSuccess = Token.Num() > 1;

    if (!bSuccess && Error.Num() <= 1)
    {
        Error.Free();
        Error = FString(L"disabled");
    }

    if (auto PlayerController = Request.PlayerController.Get())
        PlayerController->ClientVoiceChatSendJoinToken(Request.RequestID, bSuccess, Error, Token, Request.ChannelName);

    Token.Free();
    Error.Free();
    Request.ChannelName.Free();
    TSharedRef_Destructor(&Response);
    TSharedRef_Destructor(&Request.HttpRequest);
}

void AFortGameModeZone__Init()
{
    Hook(ImageBase + 0x9A45C00, SendVoiceChatToken);
    HookEvery<AFortGameModeZone>(uint32(0x1B8), SendVoiceChatChannelJoin);
}

void AFortGameModeZone__Tick()
{
    for (auto It = VoiceChatTokenRequests.begin(); It != VoiceChatTokenRequests.end();)
    {
        auto GetStatus = (EHttpRequestStatus (*)(void*))(*(void***)It->HttpRequest.Object)[0x2];
        auto Status = GetStatus(It->HttpRequest.Object);

        if (Status == EHttpRequestStatus::NotStarted || Status == EHttpRequestStatus::Processing)
        {
            It++;
            continue;
        }

        OnHttpRequestJoinTokenComplete(*It, Status == EHttpRequestStatus::Succeeded);
        It = VoiceChatTokenRequests.erase(It);
    }
}
