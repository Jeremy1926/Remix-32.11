#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortConversationTaskNode.h"

INIT_MODULE(UFortConversationTaskNode);

void AddExtraData(TArray<FConversationNodeParameterPair>& ExtraData, const wchar_t* Key, const FString& Value)
{
    ExtraData.Reserve(1);

    auto& Pair = ExtraData.Data[ExtraData.Num()];

    memset(&Pair, 0, sizeof(FConversationNodeParameterPair));
    Pair.Name = FString(Key);
    Pair.Value = FConversationNodeParameterPair::CopyString(Value);
    ExtraData.NumElements++;
}

void AddExtraData(TArray<FConversationNodeParameterPair>& ExtraData, const wchar_t* Key, const wchar_t* Value)
{
    auto ValueString = FString(Value);

    AddExtraData(ExtraData, Key, ValueString);
    ValueString.Free();
}

void CopyTags(FGameplayTagContainer& Tags, const FGameplayTagContainer& Source)
{
    for (int32 i = 0; i < Source.GameplayTags.Num(); i++)
        Tags.GameplayTags.Add(Source.GameplayTags.Data[i]);

    for (int32 i = 0; i < Source.ParentTags.Num(); i++)
        Tags.ParentTags.Add(Source.ParentTags.Data[i]);
}

static void (*GatherStaticExtraDataBase)(UConversationTaskNode*, const FConversationContext*, TArray<FConversationNodeParameterPair>*);

void GatherStaticExtraData(UFortConversationTaskNode* Node, const FConversationContext* Context, TArray<FConversationNodeParameterPair>* InOutExtraData)
{
    GatherStaticExtraDataBase(Node, Context, InOutExtraData);

    if (Node->NodeType.Data && Node->NodeType.Num() > 1)
        AddExtraData(*InOutExtraData, L"Type", Node->NodeType);

    auto& BriefWidgetClassOverride = Node->BriefWidgetClassOverride.ObjectID.AssetPath;

    if (BriefWidgetClassOverride.PackageName.ComparisonIndex != 0)
    {
        auto PackageName = BriefWidgetClassOverride.PackageName.GetRawString();
        auto FullPath = std::wstring(PackageName.begin(), PackageName.end());

        if (BriefWidgetClassOverride.AssetName.ComparisonIndex != 0)
        {
            auto AssetName = BriefWidgetClassOverride.AssetName.GetRawString();
            FullPath += L"." + std::wstring(AssetName.begin(), AssetName.end());
        }

        auto& SubPathString = Node->BriefWidgetClassOverride.ObjectID.SubPathString;

        if (SubPathString.Num() > 1)
            FullPath += L":" + SubPathString.ToWString();

        auto BriefWidgetClassPath = FString(FullPath.c_str());

        if (BriefWidgetClassPath.Num() > 1)
            AddExtraData(*InOutExtraData, L"BriefWidgetClassPath", BriefWidgetClassPath);

        BriefWidgetClassPath.Free();
    }

    if (Node->IsA(UFortConversationTaskNode_Back::StaticClass()))
        AddExtraData(*InOutExtraData, L"IsBackTaskNode", L"1");
}

void UFortConversationTaskNode__Init()
{
    GatherStaticExtraDataBase = decltype(GatherStaticExtraDataBase)(UConversationTaskNode::GetDefaultObj()->VTable[0x5D]);

    for (int32 i = 0; i < UObject::GObjects->Num(); i++)
    {
        auto Object = UObject::GObjects->GetByIndex(i);

        if (Object && Object->IsDefaultObject() && Object->IsA(UFortConversationTaskNode::StaticClass()) && Object->VTable[0x5D] == (void*)GatherStaticExtraDataBase)
            VirtualSwap(Object->VTable, 0x5D, GatherStaticExtraData);
    }
}
