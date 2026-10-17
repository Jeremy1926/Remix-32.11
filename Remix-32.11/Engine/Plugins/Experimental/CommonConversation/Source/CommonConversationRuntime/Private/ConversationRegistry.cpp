#include "pch.h"
#include "Shared.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationRegistry.h"

static UObject* CachedWorld = nullptr;
static UConversationRegistry* CachedRegistry = nullptr;

UConversationRegistry* GetFromWorld(UObject* World)
{
    if (!World)
        return nullptr;

    if (CachedWorld == World && CachedRegistry)
        return CachedRegistry;

    CachedWorld = World;
    CachedRegistry = (UConversationRegistry*)USubsystemBlueprintLibrary::GetWorldSubsystem(World, UConversationRegistry::StaticClass());

    return CachedRegistry;
}

static UConversationNode* TryGetRuntimeNodeFromGUID(UConversationRegistry* Registry, const FGuid& NodeGUID, UConversationDatabase* Graph)
{
    static auto TryGetRuntimeNodeFromGUID = (UConversationNode* (*)(UConversationRegistry*, const FGuid*, UConversationDatabase*))(ImageBase + 0x8A005B0);

    return TryGetRuntimeNodeFromGUID(Registry, &NodeGUID, Graph);
}

UConversationNode* GetRuntimeNodeFromGUID(UConversationRegistry* Registry, const FGuid& NodeGUID, UConversationDatabase* Graph)
{
    if ((NodeGUID.A | NodeGUID.B | NodeGUID.C | NodeGUID.D) == 0)
        return nullptr;

    return TryGetRuntimeNodeFromGUID(Registry, NodeGUID, Graph);
}

static void AppendOutputLinkGUIDs(UConversationRegistry* Registry, TArray<FGuid>& Result, const FGuid& SourceGUID, UConversationDatabase* Graph)
{
    auto SourceNode = GetRuntimeNodeFromGUID(Registry, SourceGUID, Graph);

    if (!SourceNode || !SourceNode->IsA(UConversationNodeWithLinks::StaticClass()))
        return;

    auto& OutputConnections = ((UConversationNodeWithLinks*)SourceNode)->OutputConnections;

    for (int32 i = 0; i < OutputConnections.Num(); i++)
        Result.Add(OutputConnections.Data[i]);
}

void GetOutputLinkGUIDs(UConversationRegistry* Registry, TArray<FGuid>& Result, UConversationDatabase* Graph, const FGameplayTag& EntryPoint)
{
    if (!Graph)
    {
        TryGetRuntimeNodeFromGUID(Registry, FGuid{}, nullptr);

        auto& EntryTagToEntryList = *(TMap<FGameplayTag, TArray<FGuid>>*)(uint64(Registry) + 0x150);

        for (auto& Entry : EntryTagToEntryList)
        {
            if (Entry.Key().TagName != EntryPoint.TagName)
                continue;

            for (int32 i = 0; i < Entry.Value().Num(); i++)
                AppendOutputLinkGUIDs(Registry, Result, Entry.Value().Data[i], nullptr);

            break;
        }

        return;
    }

    auto& EntryTags = Graph->EntryTags;

    for (int32 i = 0; i < EntryTags.Num(); i++)
    {
        auto& EntryList = EntryTags.Data[i];

        if (EntryList.EntryTag.TagName != EntryPoint.TagName)
            continue;

        for (int32 j = 0; j < EntryList.DestinationList.Num(); j++)
            AppendOutputLinkGUIDs(Registry, Result, EntryList.DestinationList.Data[j], Graph);

        break;
    }
}
