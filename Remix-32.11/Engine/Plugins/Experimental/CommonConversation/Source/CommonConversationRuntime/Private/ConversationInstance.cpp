#include "pch.h"
#include "Shared.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationInstance.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationParticipantComponent.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationRegistry.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationTaskNode.h"

struct FCheckpoint
{
    FConversationBranchPoint ClientBranchPoint;
    TArray<FConversationChoiceReference> ScopeStack;
};

TArray<FClientConversationOptionEntry>& GetCurrentUserChoices(UConversationInstance* Conversation)
{
    return *(TArray<FClientConversationOptionEntry>*)(uint64(Conversation) + 0x40);
}

static FGameplayTag& GetStartingEntryGameplayTag(UConversationInstance* Conversation)
{
    return *(FGameplayTag*)(uint64(Conversation) + 0x68);
}

static FConversationBranchPoint& GetStartingBranchPoint(UConversationInstance* Conversation)
{
    return *(FConversationBranchPoint*)(uint64(Conversation) + 0x70);
}

static FConversationBranchPoint& GetCurrentBranchPoint(UConversationInstance* Conversation)
{
    return *(FConversationBranchPoint*)(uint64(Conversation) + 0xE8);
}

FConversationNodeHandle& GetCurrentNodeHandle(UConversationInstance* Conversation)
{
    return GetCurrentBranchPoint(Conversation).ClientChoice.ChoiceReference.NodeReference;
}

static TArray<FCheckpoint>& GetClientBranchPoints(UConversationInstance* Conversation)
{
    return *(TArray<FCheckpoint>*)(uint64(Conversation) + 0x160);
}

static TArray<FConversationBranchPoint>& GetCurrentBranchPoints(UConversationInstance* Conversation)
{
    return *(TArray<FConversationBranchPoint>*)(uint64(Conversation) + 0x170);
}

static TArray<FConversationChoiceReference>& GetScopeStack(UConversationInstance* Conversation)
{
    return *(TArray<FConversationChoiceReference>*)(uint64(Conversation) + 0x180);
}

static FRandomStream& GetConversationRNG(UConversationInstance* Conversation)
{
    return *(FRandomStream*)(uint64(Conversation) + 0x190);
}

static bool& GetbConversationStarted(UConversationInstance* Conversation)
{
    return *(bool*)(uint64(Conversation) + 0x198);
}

template <typename T>
static void ResetArray(TArray<T>& Array)
{
    for (int32 i = 0; i < Array.Num(); i++)
        Array.Data[i].Destroy();

    Array.NumElements = 0;
}

template <typename T>
static void AddCopy(TArray<T>& Array, const T& Element)
{
    Array.Reserve(1);
    memset(&Array.Data[Array.Num()], 0, sizeof(T));
    Array.Data[Array.Num()].CopyFrom(Element);
    Array.NumElements++;
}

static void ModifyCurrentConversationNode(UConversationInstance* Conversation, const FConversationBranchPoint& NewBranchPoint);
static void ProcessCurrentConversationNode(UConversationInstance* Conversation);

UObject* GetWorld(UObject* Object)
{
    for (auto Outer = Object; Outer; Outer = Outer->Outer)
        if (Outer->IsA(UWorld::StaticClass()))
            return Outer;

    return nullptr;
}

std::vector<AActor*> GetParticipantActors(UConversationInstance* Conversation)
{
    std::vector<AActor*> Actors;

    for (int32 i = 0; i < Conversation->Participants.List.Num(); i++)
        Actors.push_back(Conversation->Participants.List.Data[i].Actor);

    return Actors;
}

static FConversationContext* CreateServerContext(UConversationInstance* Conversation)
{
    return FConversationContext::CreateServerContext(Conversation, nullptr, GetFromWorld(GetWorld(Conversation)));
}

static UConversationNode* ResolveNode(UConversationInstance* Conversation, const FConversationNodeHandle& Handle)
{
    auto Registry = GetFromWorld(GetWorld(Conversation));

    return Registry ? GetRuntimeNodeFromGUID(Registry, Handle.NodeGUID, Conversation->ActiveConversationGraph) : nullptr;
}

static void ServerRemoveParticipant(UConversationInstance* Conversation, const FGameplayTag& ParticipantID, const FConversationParticipants& PreservedParticipants)
{
    auto& List = Conversation->Participants.List;

    for (int32 i = 0; i < List.Num(); i++)
    {
        auto& Entry = List.Data[i];

        if (Entry.ParticipantID.TagName != ParticipantID.TagName)
            continue;

        auto OldParticipant = FindFor(Entry.Actor);

        if (OldParticipant && GetbConversationStarted(Conversation))
            ServerNotifyConversationEnded(OldParticipant, Conversation, PreservedParticipants);

        List.Remove(i);
        break;
    }
}

void ServerAssignParticipant(UConversationInstance* Conversation, const FGameplayTag& ParticipantID, AActor* ParticipantActor)
{
    if (ParticipantID.TagName.ComparisonIndex == 0 || !ParticipantActor)
        return;

    ServerRemoveParticipant(Conversation, ParticipantID, Conversation->Participants);

    FConversationParticipantEntry NewEntry{};
    NewEntry.ParticipantID = ParticipantID;
    NewEntry.Actor = ParticipantActor;
    Conversation->Participants.List.Add(NewEntry);

    if (GetbConversationStarted(Conversation))
        if (auto ParticipantComponent = FindFor(ParticipantActor))
            ServerNotifyConversationStarted(ParticipantComponent, Conversation, ParticipantID);
}

static int32 ChooseBranchIndex(UConversationInstance* Conversation, int32 NumBranchIndices)
{
    if (NumBranchIndices <= 0)
        return 0;

    auto& ConversationRNG = GetConversationRNG(Conversation);
    auto Seed = (uint32(ConversationRNG.Seed) * 196314165U) + 907633515U;
    ConversationRNG.Seed = int32(Seed);

    uint32 FractionBits = 0x3F800000U | (Seed >> 9);
    float Fraction = *(float*)&FractionBits - 1.0f;

    return int32(Fraction * float(NumBranchIndices));
}

static void PopClientBranchPoint(UConversationInstance* Conversation)
{
    auto& ClientBranchPoints = GetClientBranchPoints(Conversation);

    if (ClientBranchPoints.Num() <= 0)
        return;

    auto& Checkpoint = ClientBranchPoints.Data[ClientBranchPoints.Num() - 1];

    Checkpoint.ClientBranchPoint.Destroy();
    FConversationChoiceReference::DestroyArray(Checkpoint.ScopeStack);
    ClientBranchPoints.NumElements--;
}

static void ResetConversationProgress(UConversationInstance* Conversation)
{
    GetStartingEntryGameplayTag(Conversation).TagName.ComparisonIndex = 0;
    GetStartingBranchPoint(Conversation).Reset();
    GetCurrentBranchPoint(Conversation).Reset();
    ResetArray(GetCurrentBranchPoints(Conversation));

    while (GetClientBranchPoints(Conversation).Num() > 0)
        PopClientBranchPoint(Conversation);

    ResetArray(GetCurrentUserChoices(Conversation));
}

void DetermineBranches(UConversationInstance* Conversation, TArray<FGuid>& EnabledPaths, const TArray<FGuid>& SourceList, EConversationRequirementResult MaximumRequirementResult)
{
    auto Context = CreateServerContext(Conversation);
    auto Registry = GetFromWorld(GetWorld(Conversation));

    for (int32 i = 0; i < SourceList.Num(); i++)
    {
        auto TestNode = Registry ? GetRuntimeNodeFromGUID(Registry, SourceList.Data[i], Conversation->ActiveConversationGraph) : nullptr;

        if (TestNode && TestNode->IsA(UConversationTaskNode::StaticClass()) && CheckRequirements((UConversationTaskNode*)TestNode, Context) <= MaximumRequirementResult)
            EnabledPaths.Add(SourceList.Data[i]);
    }

    Context->Free();
}

void ServerAbortConversation(UConversationInstance* Conversation)
{
    if (GetbConversationStarted(Conversation))
    {
        ((void (*)(UConversationInstance*))Conversation->VTable[0x5A])(Conversation);

        auto ParticipantsCopy = FConversationParticipants::Allocate();
        ParticipantsCopy->CopyFrom(Conversation->Participants);

        for (int32 i = 0; i < ParticipantsCopy->List.Num(); i++)
            ServerRemoveParticipant(Conversation, ParticipantsCopy->List.Data[i].ParticipantID, *ParticipantsCopy);

        ParticipantsCopy->Free();
    }

    ResetConversationProgress(Conversation);
    GetbConversationStarted(Conversation) = false;
}

void ServerStartConversation(UConversationInstance* Conversation, const FGameplayTag& EntryPoint, UConversationDatabase* Graph)
{
    ResetConversationProgress(Conversation);
    GetStartingEntryGameplayTag(Conversation) = EntryPoint;
    Conversation->ActiveConversationGraph = Graph;

    auto Registry = GetFromWorld(GetWorld(Conversation));
    TArray<FGuid> PotentialStartingPoints;

    if (Registry)
        GetOutputLinkGUIDs(Registry, PotentialStartingPoints, Graph, EntryPoint);

    if (PotentialStartingPoints.Num() == 0)
    {
        ServerAbortConversation(Conversation);
        return;
    }

    TArray<FGuid> LegalStartingPoints;
    DetermineBranches(Conversation, LegalStartingPoints, PotentialStartingPoints, EConversationRequirementResult::FailedButVisible);
    PotentialStartingPoints.Free();

    if (LegalStartingPoints.Num() == 0)
    {
        ServerAbortConversation(Conversation);
        return;
    }

    auto StartingGuid = LegalStartingPoints.Data[ChooseBranchIndex(Conversation, LegalStartingPoints.Num())];

    GetStartingBranchPoint(Conversation).Reset();
    GetStartingBranchPoint(Conversation).ClientChoice.ChoiceReference.NodeReference.NodeGUID = StartingGuid;
    GetCurrentBranchPoint(Conversation).Reset();
    GetCurrentBranchPoint(Conversation).ClientChoice.ChoiceReference.NodeReference.NodeGUID = StartingGuid;
    LegalStartingPoints.Free();

    auto& List = Conversation->Participants.List;

    for (int32 i = 0; i < List.Num(); i++)
        if (auto ParticipantComponent = FindFor(List.Data[i].Actor))
            ServerNotifyConversationStarted(ParticipantComponent, Conversation, List.Data[i].ParticipantID);

    GetbConversationStarted(Conversation) = true;

    LARGE_INTEGER Cycles;
    QueryPerformanceCounter(&Cycles);

    auto& ConversationRNG = GetConversationRNG(Conversation);
    ConversationRNG.InitialSeed = int32(Cycles.LowPart);
    ConversationRNG.Seed = int32(Cycles.LowPart);

    ((void (*)(UConversationInstance*))Conversation->VTable[0x59])(Conversation);

    ProcessCurrentConversationNode(Conversation);
}

static FConversationBranchPoint* FindBranchPointFromClientChoice(UConversationInstance* Conversation, const FConversationChoiceReference& InChoice)
{
    auto& CurrentBranchPoints = GetCurrentBranchPoints(Conversation);

    for (int32 i = 0; i < CurrentBranchPoints.Num(); i++)
    {
        auto& BranchPoint = CurrentBranchPoints.Data[i];

        if (BranchPoint.ClientChoice.ChoiceType != EConversationChoiceType::ServerOnly && BranchPoint.ClientChoice.ChoiceReference.IsIdenticalTo(InChoice))
            return &BranchPoint;
    }

    return nullptr;
}

static void ModifyCurrentConversationNode(UConversationInstance* Conversation, const FConversationChoiceReference& NewChoice)
{
    auto BranchPoint = FConversationBranchPoint::Allocate();
    BranchPoint->ClientChoice.ChoiceReference.CopyFrom(NewChoice);

    ModifyCurrentConversationNode(Conversation, *BranchPoint);
    BranchPoint->Free();
}

void ServerAdvanceConversation(UConversationInstance* Conversation, const FAdvanceConversationRequest& InChoicePicked)
{
    if (!GetbConversationStarted(Conversation) || !GetCurrentBranchPoint(Conversation).ClientChoice.ChoiceReference.IsValid())
        return;

    TArray<FConversationBranchPoint> CandidateDestinations;
    auto ServerContext = CreateServerContext(Conversation);
    auto& Choice = InChoicePicked.Choice;

    if (Choice.NodeReference.IsValid() || Choice.NodeParameters.Num() > 0)
    {
        auto BranchPoint = FindBranchPointFromClientChoice(Conversation, Choice);

        if (!BranchPoint)
        {
            ServerContext->Free();

            auto Context = CreateServerContext(Conversation);

            for (auto Actor : GetParticipantActors(Conversation))
                if (auto ParticipantComponent = FindFor(Actor))
                    SendClientUpdatedChoices(ParticipantComponent, Context, true);

            Context->Free();
            return;
        }

        AddCopy(CandidateDestinations, *BranchPoint);

        auto TaskNode = ResolveNode(Conversation, CandidateDestinations.Data[0].ClientChoice.ChoiceReference.NodeReference);

        if (TaskNode && TaskNode->IsA(UConversationTaskNode::StaticClass()))
        {
            auto& SubNodes = ((UConversationTaskNode*)TaskNode)->SubNodes;

            for (int32 i = 0; i < SubNodes.Num(); i++)
            {
                auto SubNode = SubNodes.Data[i];

                if (SubNode && SubNode->IsA(UConversationChoiceNode::StaticClass()))
                {
                    ((void (*)(UConversationSubNode*, const FConversationContext*, const FClientConversationOptionEntry*))SubNode->VTable[0x5E])(SubNode, ServerContext, &CandidateDestinations.Data[0].ClientChoice);
                    break;
                }
            }
        }
    }
    else
    {
        auto& ScopeStack = GetScopeStack(Conversation);
        auto& CurrentBranchPoints = GetCurrentBranchPoints(Conversation);

        if (CurrentBranchPoints.Num() == 0 && ScopeStack.Num() > 0)
        {
            auto ReturnChoice = FConversationChoiceReference::Allocate();
            ReturnChoice->CopyFrom(ScopeStack.Data[ScopeStack.Num() - 1]);

            ServerContext->Free();
            ModifyCurrentConversationNode(Conversation, *ReturnChoice);
            ReturnChoice->Free();
            return;
        }

        for (int32 i = 0; i < CurrentBranchPoints.Num(); i++)
            if (CurrentBranchPoints.Data[i].ClientChoice.IsChoiceAvailable())
                AddCopy(CandidateDestinations, CurrentBranchPoints.Data[i]);
    }

    TArray<FConversationBranchPoint> ValidDestinations;
    auto RequirementContext = CreateServerContext(Conversation);

    for (int32 i = 0; i < CandidateDestinations.Num(); i++)
    {
        auto& BranchPoint = CandidateDestinations.Data[i];
        auto TaskNode = ResolveNode(Conversation, BranchPoint.ClientChoice.ChoiceReference.NodeReference);

        if (TaskNode && TaskNode->IsA(UConversationTaskNode::StaticClass()))
        {
            auto Node = (UConversationTaskNode*)TaskNode;

            if (!Node->bIgnoreRequirementsWhileAdvancingConversations && CheckRequirements(Node, RequirementContext) != EConversationRequirementResult::Passed)
                continue;
        }

        AddCopy(ValidDestinations, BranchPoint);
    }

    RequirementContext->Free();
    ServerContext->Free();
    FConversationBranchPoint::DestroyArray(CandidateDestinations);

    if (ValidDestinations.Num() == 0)
        ServerAbortConversation(Conversation);
    else
        ModifyCurrentConversationNode(Conversation, ValidDestinations.Data[ChooseBranchIndex(Conversation, ValidDestinations.Num())]);

    FConversationBranchPoint::DestroyArray(ValidDestinations);
}

static void PushClientBranchPoint(UConversationInstance* Conversation)
{
    auto& ClientBranchPoints = GetClientBranchPoints(Conversation);

    ClientBranchPoints.Reserve(1);

    auto& Checkpoint = ClientBranchPoints.Data[ClientBranchPoints.Num()];
    memset(&Checkpoint, 0, sizeof(FCheckpoint));
    ClientBranchPoints.NumElements++;

    Checkpoint.ClientBranchPoint.CopyFrom(GetCurrentBranchPoint(Conversation));
    FConversationChoiceReference::CopyArray(Checkpoint.ScopeStack, GetScopeStack(Conversation));
}

static void RestoreCheckpoint(UConversationInstance* Conversation, int32 Index)
{
    auto& Checkpoint = GetClientBranchPoints(Conversation).Data[Index];
    auto BranchPoint = FConversationBranchPoint::Allocate();
    BranchPoint->CopyFrom(Checkpoint.ClientBranchPoint);

    auto& ScopeStack = GetScopeStack(Conversation);
    FConversationChoiceReference::DestroyArray(ScopeStack);
    FConversationChoiceReference::CopyArray(ScopeStack, Checkpoint.ScopeStack);

    ModifyCurrentConversationNode(Conversation, *BranchPoint);
    BranchPoint->Free();
}

static void ModifyCurrentConversationNode(UConversationInstance* Conversation, const FConversationBranchPoint& NewBranchPoint)
{
    auto NewBranchPointCopy = FConversationBranchPoint::Allocate();
    NewBranchPointCopy->CopyFrom(NewBranchPoint);

    auto& CurrentBranchPoint = GetCurrentBranchPoint(Conversation);
    CurrentBranchPoint.Reset();
    CurrentBranchPoint.CopyFrom(*NewBranchPointCopy);

    auto& ScopeStack = GetScopeStack(Conversation);
    auto& ReturnScopeStack = NewBranchPointCopy->ReturnScopeStack;

    for (int32 i = 0; i < ReturnScopeStack.Num(); i++)
    {
        ScopeStack.Reserve(1);
        memset(&ScopeStack.Data[ScopeStack.Num()], 0, sizeof(FConversationChoiceReference));
        ScopeStack.Data[ScopeStack.Num()].NodeReference.NodeGUID = ReturnScopeStack.Data[i].NodeGUID;
        ScopeStack.NumElements++;
    }

    NewBranchPointCopy->Free();

    ProcessCurrentConversationNode(Conversation);
}

static void AddDefaultUserChoice(UConversationInstance* Conversation)
{
    auto DefaultChoice = FClientConversationOptionEntry::Allocate();
    auto NameSpace = FString(L"ConversationInstance");
    auto Key = FString(L"ConversationInstance_DefaultText");
    auto SourceString = FString(L"Continue");
    FText OutText{};

    if (UKismetTextLibrary::FindTextInLocalizationTable(NameSpace, Key, &OutText, SourceString))
        DefaultChoice->ChoiceText = OutText;
    else
    {
        FText::Destroy(&OutText);
        DefaultChoice->ChoiceText = UKismetTextLibrary::Conv_StringToText(SourceString);
    }

    NameSpace.Free();
    Key.Free();
    SourceString.Free();

    DefaultChoice->ChoiceType = EConversationChoiceType::UserChoiceAvailable;
    AddCopy(GetCurrentUserChoices(Conversation), *DefaultChoice);
    DefaultChoice->Free();
}

static void UpdateNextChoices(UConversationInstance* Conversation, const FConversationContext* Context)
{
    TArray<FConversationBranchPoint> AllChoices;
    auto TaskNode = ResolveNode(Conversation, GetCurrentNodeHandle(Conversation));

    if (TaskNode && TaskNode->IsA(UConversationTaskNode::StaticClass()))
    {
        auto ChoiceContext = Context->CreateChildContext((UConversationTaskNode*)TaskNode);
        GenerateNextChoices((UConversationTaskNode*)TaskNode, AllChoices, ChoiceContext);
        ChoiceContext->Free();
    }

    auto& CurrentUserChoices = GetCurrentUserChoices(Conversation);
    auto& CurrentBranchPoints = GetCurrentBranchPoints(Conversation);

    ResetArray(CurrentUserChoices);
    FConversationBranchPoint::DestroyArray(CurrentBranchPoints);

    CurrentBranchPoints = AllChoices;

    for (int32 i = 0; i < CurrentBranchPoints.Num(); i++)
        if (CurrentBranchPoints.Data[i].ClientChoice.ChoiceType != EConversationChoiceType::ServerOnly)
            AddCopy(CurrentUserChoices, CurrentBranchPoints.Data[i].ClientChoice);

    if ((CurrentBranchPoints.Num() > 0 || GetScopeStack(Conversation).Num() > 0) && CurrentUserChoices.Num() == 0)
        AddDefaultUserChoice(Conversation);
}

static void ProcessCurrentConversationNode(UConversationInstance* Conversation)
{
    auto AnonContext = CreateServerContext(Conversation);
    auto CurrentNode = ResolveNode(Conversation, GetCurrentNodeHandle(Conversation));

    if (!CurrentNode || !CurrentNode->IsA(UConversationTaskNode::StaticClass()))
    {
        AnonContext->Free();
        ServerAbortConversation(Conversation);
        return;
    }

    auto TaskNode = (UConversationTaskNode*)CurrentNode;
    auto Context = AnonContext->CreateChildContext(TaskNode);
    auto TaskResult = FConversationTaskResult::Allocate();

    AnonContext->Free();
    ExecuteTaskNodeWithSideEffects(TaskNode, Context, TaskResult);

    auto& ScopeStack = GetScopeStack(Conversation);
    auto& ClientBranchPoints = GetClientBranchPoints(Conversation);

    if (ScopeStack.Num() > 0 && ScopeStack.Data[ScopeStack.Num() - 1].NodeReference.NodeGUID == TaskNode->Compiled_NodeGUID)
    {
        ScopeStack.Data[ScopeStack.Num() - 1].Destroy();
        ScopeStack.NumElements--;
    }

    UpdateNextChoices(Conversation, Context);

    switch (TaskResult->Type)
    {
    case EConversationTaskResultType::AbortConversation:
        ServerAbortConversation(Conversation);
        break;
    case EConversationTaskResultType::AdvanceConversation:
    {
        auto AnyRequest = FAdvanceConversationRequest::Allocate();
        ServerAdvanceConversation(Conversation, *AnyRequest);
        AnyRequest->Free();
        break;
    }
    case EConversationTaskResultType::AdvanceConversationWithChoice:
        ModifyCurrentConversationNode(Conversation, TaskResult->AdvanceToChoice.Choice);
        break;
    case EConversationTaskResultType::PauseConversationAndSendClientChoices:
    {
        auto LastMessage = FClientConversationMessagePayload::Allocate();

        LastMessage->Message.CopyFrom(TaskResult->Message);
        FClientConversationOptionEntry::CopyArray(LastMessage->Options, GetCurrentUserChoices(Conversation));
        LastMessage->CurrentNode.NodeGUID = GetCurrentNodeHandle(Conversation).NodeGUID;
        LastMessage->Participants.CopyFrom(Conversation->Participants);

        PushClientBranchPoint(Conversation);

        for (int32 i = 0; i < LastMessage->Participants.List.Num(); i++)
            if (auto ParticipantComponent = FindFor(LastMessage->Participants.List.Data[i].Actor))
                SendClientConversationMessage(ParticipantComponent, Context, *LastMessage);

        LastMessage->Free();
        break;
    }
    case EConversationTaskResultType::ReturnToLastClientChoice:
        if (ClientBranchPoints.Num() > 1)
        {
            PopClientBranchPoint(Conversation);
            RestoreCheckpoint(Conversation, ClientBranchPoints.Num() - 1);
        }
        break;
    case EConversationTaskResultType::ReturnToCurrentClientChoice:
        if (ClientBranchPoints.Num() > 0)
        {
            auto& Checkpoint = ClientBranchPoints.Data[ClientBranchPoints.Num() - 1];
            auto BranchPoint = FConversationBranchPoint::Allocate();
            BranchPoint->CopyFrom(Checkpoint.ClientBranchPoint);

            TArray<FConversationChoiceReference> CheckpointScopeStack;
            FConversationChoiceReference::CopyArray(CheckpointScopeStack, Checkpoint.ScopeStack);

            PopClientBranchPoint(Conversation);

            FConversationChoiceReference::DestroyArray(ScopeStack);
            ScopeStack = CheckpointScopeStack;

            ModifyCurrentConversationNode(Conversation, *BranchPoint);
            BranchPoint->Free();
        }
        break;
    case EConversationTaskResultType::ReturnToConversationStart:
    {
        auto EntryStartPointGameplayTagCache = GetStartingEntryGameplayTag(Conversation);
        auto StartingBranchPointCache = FConversationBranchPoint::Allocate();
        StartingBranchPointCache->CopyFrom(GetStartingBranchPoint(Conversation));

        ResetConversationProgress(Conversation);

        GetStartingEntryGameplayTag(Conversation) = EntryStartPointGameplayTagCache;
        GetStartingBranchPoint(Conversation).CopyFrom(*StartingBranchPointCache);

        ModifyCurrentConversationNode(Conversation, *StartingBranchPointCache);
        StartingBranchPointCache->Free();
        break;
    }
    default:
        break;
    }

    TaskResult->Free();
    Context->Free();
}
