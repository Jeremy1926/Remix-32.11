#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortNonPlayerConversationParticipantComponent.h"

INIT_MODULE(UFortAthenaAISpawnerDataComponent_AIBotConversation);

static std::unordered_map<UClass*, int32> DescriptorTags;

static uint64 (*ApplyToPawnOG)(UFortAthenaAISpawnerDataComponent*, APawn*);
static uint64 ApplyToPawn(UFortAthenaAISpawnerDataComponent* Component, APawn* Pawn)
{
    auto Result = ApplyToPawnOG(Component, Pawn);

    if (!Component || !Pawn)
        return Result;

    auto ConversationComponent = Pawn->GetComponentByClass(UFortNonPlayerConversationParticipantComponent::StaticClass());

    if (!ConversationComponent)
        return Result;

    FGameplayTag Tag{};
    auto Found = DescriptorTags.find(Component->Class);

    if (Found != DescriptorTags.end())
        Tag.TagName.ComparisonIndex = Found->second;
    else
    {
        for (int32 i = 0; i < UObject::GObjects->Num(); i++)
        {
            auto SpawnerData = (UFortAthenaAISpawnerData*)UObject::GObjects->GetByIndex(i);

            if (!SpawnerData || !SpawnerData->IsDefaultObject() || !SpawnerData->IsA(UFortAthenaAISpawnerData::StaticClass()) || SpawnerData->ConversationComponent != Component->Class)
                continue;

            if (SpawnerData->DescriptorTag.GameplayTags.Num() > 0)
            {
                Tag = SpawnerData->DescriptorTag.GameplayTags.Data[0];
                break;
            }
        }

        if (Tag.TagName.ComparisonIndex != 0)
            DescriptorTags[Component->Class] = Tag.TagName.ComparisonIndex;
    }

    SetNPCUniqueIDTag(ConversationComponent, Tag);

    return Result;
}

void UFortAthenaAISpawnerDataComponent_AIBotConversation__Init()
{
    Hook<UFortAthenaAISpawnerDataComponent_AIBotConversation>(uint32(0x5B), ApplyToPawn, ApplyToPawnOG);
}
