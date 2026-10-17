#include "pch.h"
#include "Shared.h"

INIT_MODULE(UMcpProfileGroup);

static void* (*SendRequestNowOG)(UMcpProfileGroup*, void*, int);
static void* SendRequestNow(UMcpProfileGroup* McpProfileGroup, void* Request, int Context)
{
    return SendRequestNowOG(McpProfileGroup, Request, 3);
}

void UMcpProfileGroup__Init()
{
    Hook(ImageBase + 0x7DE4ED0, SendRequestNow, SendRequestNowOG);
}
