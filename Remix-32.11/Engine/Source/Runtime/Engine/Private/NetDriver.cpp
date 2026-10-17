#include "pch.h"
#include "Shared.h"

INIT_POSTLOAD(UNetDriver);

enum class EReplicationSystemSendPass : uint32_t
{
    Invalid,
    PostTickDispatch,
    TickFlush
};

struct FSendUpdateParams
{
    EReplicationSystemSendPass SendPass = EReplicationSystemSendPass::TickFlush;
    float DeltaSeconds = 0.f;
};

static void SendClientMoveAdjustments(UNetDriver* Driver)
{
    static auto SendClientAdjustment = (void (*)(APlayerController*))(ImageBase + 0x71152EC);

    for (auto Connection : Driver->ClientConnections)
    {
        if (!Connection || !Connection->ViewTarget)
            continue;

        if (auto PlayerController = Connection->PlayerController)
            SendClientAdjustment(PlayerController);

        for (auto ChildConnection : Connection->Children)
        {
            if (!ChildConnection)
                continue;

            if (auto PlayerController = ChildConnection->PlayerController)
                SendClientAdjustment(PlayerController);
        }
    }
}

static void (*TickFlushOG)(UNetDriver*, float);
static void TickFlush(UNetDriver* Driver, float DeltaSeconds)
{
    if (Driver->World && Driver->World->NetDriver == Driver)
    {
        for (auto& Ticker : Tickers)
            Ticker();
    }

    if (Driver->ClientConnections.Num() > 0)
    {
        auto ReplicationSystem = *(UReplicationSystem**)(__int64(Driver) + 0x808);

        if (ReplicationSystem)
        {
            static auto UpdateIrisReplicationViews = (void (*)(UNetDriver*))(ImageBase + 0x705EBE4);
            static auto PreSendUpdate = (void (*)(UReplicationSystem*, FSendUpdateParams&))(ImageBase + 0x63BF4A4);

            UpdateIrisReplicationViews(Driver);
            SendClientMoveAdjustments(Driver);

            FSendUpdateParams Params;
            Params.DeltaSeconds = DeltaSeconds;
            PreSendUpdate(ReplicationSystem, Params);
        }
    }

    return TickFlushOG(Driver, DeltaSeconds);
}

void UNetDriver__PostLoad()
{
    Hook(ImageBase + 0x23CE5C4, TickFlush, TickFlushOG);
}
