#include "pch.h"
#include "Shared.h"
#include "Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/GameplayCueInterface.h"

static uint8* SetNum(FMinimalGameplayCueReplicationProxy* Proxy, uint32 InlineOffset, uint32 NumOffset, int32 ElementSize, int32 NewNum)
{
    auto Base = (uint8*)Proxy;
    auto& SecondaryData = *(uint8**)(Base + NumOffset - 8);
    auto& ArrayNum = *(int32*)(Base + NumOffset);
    auto& ArrayMax = *(int32*)(Base + NumOffset + 4);

    if (NewNum > ArrayMax)
    {
        SecondaryData = (uint8*)FMemory::Realloc(SecondaryData, uint64(NewNum) * ElementSize);
        ArrayMax = NewNum;
    }

    ArrayNum = NewNum;

    return SecondaryData ? SecondaryData : Base + InlineOffset;
}

bool PreReplication(FMinimalGameplayCueReplicationProxy* Proxy, FActiveGameplayCueContainer& SourceContainer)
{
    auto& LastSourceArrayReplicationKey = *(int32*)(uint64(Proxy) + 0x2C8);

    if (LastSourceArrayReplicationKey == SourceContainer.ArrayReplicationKey)
        return false;

    LastSourceArrayReplicationKey = SourceContainer.ArrayReplicationKey;

    auto& GameplayCues = SourceContainer.GameplayCues;
    auto ReplicatedTags = (FGameplayTag*)SetNum(Proxy, 0x40, 0x88, sizeof(FGameplayTag), GameplayCues.Num());
    auto ReplicatedLocations = (FVector*)SetNum(Proxy, 0x90, 0x218, sizeof(FVector), GameplayCues.Num());

    for (int32 i = 0; i < GameplayCues.Num(); i++)
    {
        auto& Parameters = GameplayCues[i].Parameters;

        ReplicatedTags[i] = GameplayCues[i].GameplayCueTag;
        ReplicatedLocations[i] = Parameters.bReplicateLocationWhenUsingMinimalRepProxy ? (FVector)Parameters.Location : FVector();
    }

    return true;
}
