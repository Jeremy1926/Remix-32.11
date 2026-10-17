#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortConversationRequirement_ControllerRequirement.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortConversationTaskNode.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortNonPlayerConversationParticipantComponent.h"

INIT_MODULE(UFortConversationTaskNode_Speech);

static AActor* GetParticipantActor(const FConversationContext* Context, const FGameplayTag& ParticipantID)
{
    auto Conversation = Context ? Context->ActiveConversation : nullptr;

    if (!Conversation)
        return nullptr;

    for (auto& Entry : Conversation->Participants.List)
        if (Entry.ParticipantID.TagName == ParticipantID.TagName)
            return Entry.Actor;

    return nullptr;
}

static void DestroyQueryResult(FContextualMessageQueryResult* Result)
{
    FText::Destroy(&Result->Message);
    FConversationNodeParameterPair::DestroyArray(Result->MetadataParameters);
    FreeGameplayTags(Result->MetadataTags);
}

static bool ResolveMessageForContext(UFortConversationTaskNode_Speech* Node, const FGameplayTag& SpeakerParticipantTag, const FConversationContext* Context, FContextualMessageQueryResult* OutResult)
{
    TMap<FGameplayTag, FContextualMessageConfig>* ConfigMap = nullptr;
    FContextualMessageConfig* DefaultConfig = nullptr;
    bool bUsePerNPC = false;
    bool bUseServiceProviderTag = false;

    if (Node->bUseInlineConfig)
    {
        ConfigMap = &Node->SpeakerEntryTagToConfig;
        DefaultConfig = &Node->GeneralConfig;
        bUsePerNPC = Node->bUsePerNPC_ContextualMessageLookup;
        bUseServiceProviderTag = Node->bLookupViaServiceProviderTag;
    }
    else if (auto Collection = Node->ExternalContextualMessageCollection)
    {
        ConfigMap = &Collection->SpeakerEntryTagToConfig;
        DefaultConfig = &Collection->GeneralConfig;
        bUsePerNPC = Collection->bUsePerNPC_ContextualMessageLookup;
        bUseServiceProviderTag = Collection->bLookupViaServiceProviderTag;
    }
    else
        return false;

    FContextualMessageConfig* Config = nullptr;

    if (bUsePerNPC)
    {
        auto Speaker = GetParticipantActor(Context, SpeakerParticipantTag);
        auto SpeakerComponent = Speaker ? (UFortNonPlayerConversationParticipantComponent*)Speaker->GetComponentByClass(UFortNonPlayerConversationParticipantComponent::StaticClass()) : nullptr;

        if (SpeakerComponent)
        {
            auto Key = bUseServiceProviderTag ? GetServiceProviderTag(SpeakerComponent) : SpeakerComponent->ConversationEntryTag;

            if (Key.TagName.ComparisonIndex != 0)
                for (auto& Entry : *ConfigMap)
                    if (Entry.Key().TagName == Key.TagName)
                        Config = &Entry.Value();
        }
    }

    if (!Config)
        Config = DefaultConfig;

    std::vector<FContextualMessageCandidate*> BestCandidates;
    float BestPriority = 0.f;

    for (int32 i = 0; i < Config->ContextualMessages.Num(); i++)
    {
        auto& Candidate = Config->ContextualMessages.Data[i];
        auto bRequireAny = Candidate.RequirementMatchPolicy == EContextRequirementMatchPolicy::RequireAny;
        auto bRequirementsMet = Candidate.ContextRequirements.Num() == 0 || !bRequireAny;

        for (int32 x = 0; x < Candidate.ContextRequirements.Num(); x++)
        {
            auto& Requirement = Candidate.ContextRequirements.Data[x];
            auto bIsMet = IsControllerRequirementMet(Requirement.Requirement, Context, Requirement.ParticipantID);

            if (bRequireAny && bIsMet)
            {
                bRequirementsMet = true;
                break;
            }

            if (!bRequireAny && !bIsMet)
            {
                bRequirementsMet = false;
                break;
            }
        }

        if (!bRequirementsMet)
            continue;

        auto CandidatePriority = Candidate.Priority.Evaluate();

        if (BestCandidates.empty() || CandidatePriority > BestPriority)
        {
            BestCandidates.clear();
            BestPriority = CandidatePriority;
        }
        else if (CandidatePriority < BestPriority)
            continue;

        BestCandidates.push_back(&Candidate);
    }

    FContextualMessageCandidate* Chosen = nullptr;

    if (!BestCandidates.empty())
    {
        float TotalWeight = 0.f;

        for (auto Candidate : BestCandidates)
            TotalWeight += max(Candidate->Weight.Evaluate(), 0.f);

        if (TotalWeight <= 0.f)
            Chosen = BestCandidates[rand() % BestCandidates.size()];
        else
        {
            auto Roll = (float(rand()) / float(RAND_MAX)) * TotalWeight;

            for (auto Candidate : BestCandidates)
            {
                auto CandidateWeight = Candidate->Weight.Evaluate();

                if (CandidateWeight <= 0.f)
                    continue;

                if (Roll <= CandidateWeight)
                {
                    Chosen = Candidate;
                    break;
                }

                Roll -= CandidateWeight;
            }

            if (!Chosen)
                Chosen = BestCandidates.back();
        }
    }

    auto& Message = Chosen ? Chosen->Message : Config->DefaultMessage;
    auto& MetadataParameters = Chosen ? Chosen->MetadataParameters : Config->DefaultMetadataParameters;
    auto& MetadataTags = Chosen ? Chosen->MetadataTags : Config->DefaultMetadataTags;

    DestroyQueryResult(OutResult);

    FText::CopyTo(&OutResult->Message, &Message);
    FConversationNodeParameterPair::CopyArray(OutResult->MetadataParameters, MetadataParameters);
    CopyTags(OutResult->MetadataTags, MetadataTags);

    return Chosen != nullptr;
}

static void ResolveMessageForContext(UObject* Context, FFrame& Stack, bool* Ret)
{
    FGameplayTag SpeakerParticipantTag{};
    Stack.StepCompiledIn(&SpeakerParticipantTag);

    auto ConversationContextTemp = FConversationContext::Allocate();
    auto ConversationContext = (FConversationContext*)Stack.StepCompiledInRefInternal(ConversationContextTemp);
    auto OutResultTemp = (FContextualMessageQueryResult*)FMemory::Malloc(sizeof(FContextualMessageQueryResult));
    memset(OutResultTemp, 0, sizeof(FContextualMessageQueryResult));

    auto OutResult = (FContextualMessageQueryResult*)Stack.StepCompiledInRefInternal(OutResultTemp);
    Stack.IncrementCode();

    *Ret = ResolveMessageForContext((UFortConversationTaskNode_Speech*)Context, SpeakerParticipantTag, ConversationContext, OutResult);

    DestroyQueryResult(OutResultTemp);
    FMemory::Free(OutResultTemp);
    ConversationContextTemp->Free();
}

void UFortConversationTaskNode_Speech__Init()
{
    ExecHook(UFortConversationTaskNode_Speech::StaticClass()->FindFunction("ResolveMessageForContext"), (void (*)(UObject*, FFrame&, bool*))ResolveMessageForContext);
}
