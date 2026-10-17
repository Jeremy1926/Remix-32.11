#include "pch.h"
#include "Shared.h"
#include "SDK/SDK/AccoladesRuntime_structs.hpp"
#include "SDK/SDK/AccoladesRuntime_classes.hpp"

INIT_POSTLOAD(UFortControllerComponent_AccoladeCollection);
INIT_TICKER(UFortControllerComponent_AccoladeCollection);

struct FAccoladeCollectionRequest
{
    TWeakObjectPtr<UFortControllerComponent_AccoladeCollection> Component;
    FSharedRef HttpRequest;
    bool bRestore;
};

static std::vector<FAccoladeCollectionRequest> AccoladeCollectionRequests;

static EPersistenceFrameworkResult& GetRestoreResult(UFortControllerComponent_AccoladeCollection* Component)
{
    return *(EPersistenceFrameworkResult*)(__int64(Component) + 0x1E8);
}

static TMap<FName, FFortAccoladeCollectionData>& GetAccoladeCollection(UFortControllerComponent_AccoladeCollection* Component)
{
    return *(TMap<FName, FFortAccoladeCollectionData>*)(__int64(Component) + 0xD8);
}

static void NotifyRestored(UFortControllerComponent_AccoladeCollection* Component, EPersistenceFrameworkResult Result)
{
    ((void (*)(UFortControllerComponent_AccoladeCollection*, EPersistenceFrameworkResult))Component->VTable[0xAB])(Component, Result);
    Component->ClientNotifyRestored(Result);
}

static bool SendCollectionRequest(UFortControllerComponent_AccoladeCollection* Component, bool bRestore, const std::wstring& Content)
{
    static auto IOnlineSubsystem_Get = (void* (*)(const FName&))(ImageBase + 0x21048AC);
    static auto GetMcpGameService = (FSharedRef* (*)(void*, FSharedRef*))(ImageBase + 0x1F60394);
    static auto GetIdentityInterface = (FSharedRef* (*)(void*, FSharedRef*))(ImageBase + 0x2C7C0F0);
    static auto ProcessRequest = (void (*)(void*, void*, const void*, const FSharedRef*))(ImageBase + 0x762A4A0);
    static auto ToString = (FString* (*)(const FUniqueNetIdRepl*, FString*))(ImageBase + 0x35E21FC);
    static auto TSharedRef_Destructor = (void (*)(FSharedRef*))(ImageBase + 0x14EAAD8);
    static auto& McpSubsystemName = *(FName*)(ImageBase + 0x12ED97E8);

    auto Owner = Component->GetOwner();
    auto PlayerState = Owner && Owner->IsA(AFortPlayerControllerAthena::StaticClass()) ? ((AFortPlayerControllerAthena*)Owner)->PlayerState : nullptr;
    auto Subsystem = PlayerState ? IOnlineSubsystem_Get(McpSubsystemName) : nullptr;
    FSharedRef GameService{};
    FSharedRef Identity{};
    FSharedRef UserId{};
    bool bSent = false;

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
        auto URL = FString((BaseUrl.ToWString() + L"/api/game/v2/accolades/" + AccountId.ToWString() + L"/dedicated_server/collection").c_str());
        auto Verb = FString(bRestore ? L"GET" : L"POST");

        SetURL(HttpRequest.Object, URL);
        SetVerb(HttpRequest.Object, Verb);

        if (!bRestore)
        {
            auto Body = FString(Content.c_str());
            auto HeaderName = FString(L"Content-Type");
            auto HeaderValue = FString(L"application/json");

            SetContentAsString(HttpRequest.Object, Body);
            SetHeader(HttpRequest.Object, HeaderName, HeaderValue);

            HeaderValue.Free();
            HeaderName.Free();
            Body.Free();
        }

        ProcessRequest(Subsystem, GameService.Object, UserId.Object, &HttpRequest);
        AccoladeCollectionRequests.push_back({ Component, HttpRequest, bRestore });
        bSent = true;

        Verb.Free();
        URL.Free();
        AccountId.Free();
        BaseUrl.Free();
    }

    TSharedRef_Destructor(&UserId);
    TSharedRef_Destructor(&Identity);
    TSharedRef_Destructor(&GameService);

    return bSent;
}

static bool RestorePersistentInfo(UFortControllerComponent_AccoladeCollection* Component, FSharedRef& HttpRequest)
{
    static auto TJsonReaderFactory_Create = (FSharedRef* (*)(FSharedRef*, const FString*))(ImageBase + 0x27DF56C);
    static auto FJsonSerializer_Deserialize = (bool (*)(void*, FSharedRef*, int32))(ImageBase + 0x22CB314);
    static auto TryGetStringField = (bool (*)(void*, const FJsonKey*, FString*))(ImageBase + 0x22C9958);
    static auto JsonObjectToUStruct = (bool (*)(void*, UScriptStruct*, void*, int64, int64, bool, void*, void*))(ImageBase + 0x527491C);
    static auto ApplyPersistentInfo = (void (*)(FPFWAccoladeCollection_PersistentInfo*, UFortControllerComponent_AccoladeCollection**))(ImageBase + 0xB490A48);
    static auto DestroyPersistentInfo = (void (*)(FPFWAccoladeCollection_PersistentInfo*))(ImageBase + 0xB488588);
    static auto TSharedRef_Destructor = (void (*)(FSharedRef*))(ImageBase + 0x14EAAD8);
    static auto PersistentInfoStruct = FindObject<UScriptStruct>(L"/Script/AccoladesRuntime.PFWAccoladeCollection_PersistentInfo");
    alignas(16) static const FJsonKey ErrorCodeField{ L"errorCode", 9 };

    auto GetResponse = (FSharedRef* (*)(void*, FSharedRef*))(*(void***)HttpRequest.Object)[0x24];
    FSharedRef Response{};
    bool bRestored = false;

    GetResponse(HttpRequest.Object, &Response);

    if (Response.Object && PersistentInfoStruct)
    {
        auto GetContentAsString = (FString* (*)(void*, FString*))(*(void***)Response.Object)[0xC];
        FString Content;
        FString ErrorCode;
        FSharedRef JsonReader{};
        FSharedRef JsonObject{};
        FPFWAccoladeCollection_PersistentInfo PersistentInfo{};

        GetContentAsString(Response.Object, &Content);
        TJsonReaderFactory_Create(&JsonReader, &Content);

        if (FJsonSerializer_Deserialize(JsonReader.Object, &JsonObject, 0) && JsonObject.Object && !TryGetStringField(JsonObject.Object, &ErrorCodeField, &ErrorCode) &&
            JsonObjectToUStruct(JsonObject.Object, PersistentInfoStruct, &PersistentInfo, 0, 0, false, nullptr, nullptr))
        {
            ApplyPersistentInfo(&PersistentInfo, &Component);
            bRestored = true;
        }

        DestroyPersistentInfo(&PersistentInfo);
        TSharedRef_Destructor(&JsonObject);
        TSharedRef_Destructor(&JsonReader);
        ErrorCode.Free();
        Content.Free();
    }

    TSharedRef_Destructor(&Response);

    return bRestored;
}

static std::wstring JsonString(FName Name)
{
    auto Value = Name.IsNone() ? std::string() : Name.ToString();

    return L"\"" + std::wstring(Value.begin(), Value.end()) + L"\"";
}

static std::wstring GetPersistentInfoJson(UFortControllerComponent_AccoladeCollection* Component)
{
    std::wstring AchievedAccolades;
    std::wstring UnacknowledgedAccolades;

    for (auto& Pair : GetAccoladeCollection(Component))
    {
        auto& AccoladeName = Pair.Key();
        auto& CollectionData = Pair.Value();

        AchievedAccolades += (AchievedAccolades.empty() ? L"" : L",") + std::wstring(L"{\"accoladeName\":") + JsonString(AccoladeName) + L",\"accoladeCount\":" +
                             std::to_wstring(CollectionData.AchievedCount) + L"}";

        if (!CollectionData.bAcknowledged)
            UnacknowledgedAccolades += (UnacknowledgedAccolades.empty() ? L"" : L",") + JsonString(AccoladeName);
    }

    return L"{\"accoladeCollection_AchievedAccolades\":[" + AchievedAccolades + L"],\"accoladeCollection_PinnedAccolade\":{\"accoladeName\":" + JsonString(Component->PinnedAccoladeName) +
           L"},\"accoladeCollection_UnacknowledgedAccolades\":{\"accoladeNames\":[" + UnacknowledgedAccolades + L"]}}";
}

static void (*BeginPlayOG)(UFortControllerComponent_AccoladeCollection*);
static void BeginPlay(UFortControllerComponent_AccoladeCollection* Component)
{
    BeginPlayOG(Component);

    auto Owner = Component->GetOwner();

    if (Owner && Owner->IsA(AFortPlayerControllerAthena::StaticClass()))
        ((AFortPlayerControllerAthena*)Owner)->OnMcpProfilesInitializedEvent.Bind(Component, FName(L"HandlePlayerProfileInitialized"));
}

static void (*HandlePlayerProfileInitializedOG)(UObject*, FFrame&, void*);
static void HandlePlayerProfileInitialized(UObject* Context, FFrame& Stack, void* Ret)
{
    HandlePlayerProfileInitializedOG(Context, Stack, Ret);

    auto Component = (UFortControllerComponent_AccoladeCollection*)Context;

    if (GetRestoreResult(Component) != EPersistenceFrameworkResult::NotReady)
        return;

    GetRestoreResult(Component) = EPersistenceFrameworkResult::InProgress;

    if (!SendCollectionRequest(Component, true, L""))
        NotifyRestored(Component, EPersistenceFrameworkResult::Failed);
}

static void (*OnEndMatchForPlayerOG)(UObject*, FFrame&, void*);
static void OnEndMatchForPlayer(UObject* Context, FFrame& Stack, void* Ret)
{
    OnEndMatchForPlayerOG(Context, Stack, Ret);

    auto Component = (UFortControllerComponent_AccoladeCollection*)Context;

    if (GetRestoreResult(Component) == EPersistenceFrameworkResult::Success)
        SendCollectionRequest(Component, false, GetPersistentInfoJson(Component));
}

void UFortControllerComponent_AccoladeCollection__PostLoad()
{
    Hook<UFortControllerComponent_AccoladeCollection>(uint32(0x78), BeginPlay, BeginPlayOG);
    ExecHook(UFortControllerComponent_AccoladeCollection::StaticClass()->FindFunction("HandlePlayerProfileInitialized"), HandlePlayerProfileInitialized, HandlePlayerProfileInitializedOG);
    ExecHook(UFortControllerComponent_AccoladeCollection::StaticClass()->FindFunction("OnEndMatchForPlayer"), OnEndMatchForPlayer, OnEndMatchForPlayerOG);
}

void UFortControllerComponent_AccoladeCollection__Tick()
{
    static auto TSharedRef_Destructor = (void (*)(FSharedRef*))(ImageBase + 0x14EAAD8);

    for (auto It = AccoladeCollectionRequests.begin(); It != AccoladeCollectionRequests.end();)
    {
        auto GetStatus = (EHttpRequestStatus (*)(void*))(*(void***)It->HttpRequest.Object)[0x2];
        auto Status = GetStatus(It->HttpRequest.Object);

        if (Status == EHttpRequestStatus::NotStarted || Status == EHttpRequestStatus::Processing)
        {
            It++;
            continue;
        }

        if (auto Component = It->bRestore ? It->Component.Get() : nullptr)
        {
            auto bRestored = Status == EHttpRequestStatus::Succeeded && RestorePersistentInfo(Component, It->HttpRequest);

            NotifyRestored(Component, bRestored ? EPersistenceFrameworkResult::Success : EPersistenceFrameworkResult::Failed);
        }

        TSharedRef_Destructor(&It->HttpRequest);
        It = AccoladeCollectionRequests.erase(It);
    }
}
