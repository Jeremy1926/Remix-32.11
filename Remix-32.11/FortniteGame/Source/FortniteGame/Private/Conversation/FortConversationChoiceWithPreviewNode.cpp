#include "pch.h"
#include "Shared.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationRegistry.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationTaskNode.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortConversationTaskNode.h"

INIT_MODULE(UFortConversationChoiceWithPreviewNode);

static FString TagContainerToString(const FGameplayTagContainer& Tags)
{
    std::wstring Result = L"(GameplayTags=(";

    for (int32 i = 0; i < Tags.GameplayTags.Num(); i++)
    {
        if (i > 0)
            Result += L",";

        auto TagName = Tags.GameplayTags.Data[i].TagName.ToString();
        Result += L"(TagName=\"" + std::wstring(TagName.begin(), TagName.end()) + L"\")";
    }

    Result += L"))";

    return FString(Result.c_str());
}

static void (*FillChoiceOG)(UFortConversationChoiceWithPreviewNode*, const FConversationContext*, FClientConversationOptionEntry*);
static void FillChoice(UFortConversationChoiceWithPreviewNode* Node, const FConversationContext* Context, FClientConversationOptionEntry* ChoiceEntry)
{
    FillChoiceOG(Node, Context, ChoiceEntry);

    auto Parent = Node->ParentNode;

    if (!Parent || !Parent->IsA(UConversationTaskNode::StaticClass()) || !Context || !Context->ActiveConversation || !Context->ConversationRegistry)
        return;

    for (auto& Connection : ((UConversationNodeWithLinks*)Parent)->OutputConnections)
    {
        auto Child = (UFortConversationTaskNode*)GetRuntimeNodeFromGUID(Context->ConversationRegistry, Connection, Context->ActiveConversation->ActiveConversationGraph);

        if (!Child || !Child->IsA(UFortConversationTaskNode::StaticClass()))
            continue;

        if (!Node->OptionalChildChoiceTagFilter.TagName.IsNone() && !Child->NodeTags.HasTag(Node->OptionalChildChoiceTagFilter))
            continue;

        auto ChildContext = Context->CreateChildContext(Child);

        if (CheckRequirements(Child, ChildContext) == EConversationRequirementResult::FailedAndHidden)
        {
            ChildContext->Free();
            continue;
        }

        TArray<FConversationBranchPoint> ChildChoices;
        ((void (*)(UConversationTaskNode*, TArray<FConversationBranchPoint>*, const FConversationContext*))Child->VTable[0x63])(Child, &ChildChoices, ChildContext);

        auto bPreviewed = false;

        if (ChildChoices.Num() > 0)
        {
            auto& ChildOption = ChildChoices.Data[0].ClientChoice;

            if (((bool (*)(UFortConversationTaskNode*, const FConversationContext*, const FClientConversationOptionEntry*))Child->VTable[0x68])(Child, ChildContext, &ChildOption))
            {
                auto PreviewTags = TagContainerToString(Child->NodeTags);

                AddExtraData(ChoiceEntry->ExtraData, L"preview_tags", PreviewTags);
                ((void (*)(UFortConversationTaskNode*, const FConversationContext*, const FClientConversationOptionEntry*, TArray<FConversationNodeParameterPair>*))Child->VTable[0x69])(Child, ChildContext, &ChildOption, &ChoiceEntry->ExtraData);

                PreviewTags.Free();
                bPreviewed = true;
            }
        }

        FConversationBranchPoint::DestroyArray(ChildChoices);
        ChildContext->Free();

        if (bPreviewed)
            break;
    }
}

void UFortConversationChoiceWithPreviewNode__Init()
{
    Hook<UFortConversationChoiceWithPreviewNode>(uint32(0x5C), FillChoice, FillChoiceOG);
}
