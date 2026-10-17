#include "pch.h"
#include "Shared.h"

INIT_POSTLOAD(UFortMovementComp_Character);

struct FCharacterNetworkMoveData
{
    void* Vft;
    uint32 NetworkMoveType;
    float TimeStamp;
};

static void (*ServerMove_PerformMovementOG)(UCharacterMovementComponent*, const FCharacterNetworkMoveData*);
static void ServerMove_PerformMovement(UCharacterMovementComponent* CharacterMovement, const FCharacterNetworkMoveData* MoveData)
{
    auto Movement = (UFortMovementComp_Character*)CharacterMovement;
    auto Pawn = Movement && MoveData && Movement->IsA(UFortMovementComp_Character::StaticClass()) ? Movement->PawnOwner : nullptr;

    if (!Pawn || !Pawn->IsA(AFortPawn::StaticClass()))
        return ServerMove_PerformMovementOG(CharacterMovement, MoveData);

    static auto GetMovementModeExtensionLogic = (UObject** (*)(AActor*, UObject**, uint32))(ImageBase + 0x2AAABA0);
    static auto MakeMovementModeExtensionContext = (FMMERuntimeContext* (*)(FMMERuntimeContext*, UObject*))(ImageBase + 0x1BCE080);
    static auto PushMovementModeExtension = (void (*)(UObject*, FMMERuntimeContext*))(ImageBase + 0x351D4B8);
    static auto GetPredictionData_Server_Character = (uint8* (*)(UCharacterMovementComponent*))(ImageBase + 0x6DD54B0);
    static auto IsClientTimeStampValid = (bool (*)(UCharacterMovementComponent*, float, uint8*, bool*))(ImageBase + 0x6DD5F04);
    static auto bEndMMEOnServerIfNoUpdateReceived = (bool*)(ImageBase + 0x12C6028C);

    auto& Received = Movement->ReceivedPackMovementModeExtensionRuntimeData;
    auto& Pushed = Movement->PushedMovementModeExtensionRuntimeData;
    auto& Pending = *(TArray<uint64>*)(uint64(&Movement->SavedMoveRuntimeDataBackup) + sizeof(TArray<uint64>) * 2);
    auto bTimeStampResetDetected = false;
    auto bNewestMove = IsClientTimeStampValid(CharacterMovement, MoveData->TimeStamp, GetPredictionData_Server_Character(CharacterMovement), &bTimeStampResetDetected);

    for (auto Runtime : Pushed)
    {
        if (!Runtime)
            continue;

        UFortMovementMode_BaseExtRuntimeData* Source = nullptr;

        for (auto Candidate : Received)
        {
            if (Candidate && Candidate->LogicID.MovementMode_ExtCRC_ID == Runtime->LogicID.MovementMode_ExtCRC_ID && Candidate->RuntimeID == Runtime->RuntimeID)
                Source = Candidate;
        }

        if (Source)
        {
            Runtime->bForceManualActivationNetworked = Source->bForceManualActivationNetworked;
            Runtime->bForceManualEndNetworked = Source->bForceManualEndNetworked;
        }
        else if (bNewestMove && *bEndMMEOnServerIfNoUpdateReceived)
            Runtime->bForceManualEnd = true;
    }

    for (auto PendingExtension : Pending)
    {
        auto LogicID = uint32(PendingExtension);
        auto RuntimeID = uint8(PendingExtension >> 32);
        int32 SourceIndex = -1;

        for (int i = 0; i < Received.Num(); i++)
        {
            if (Received[i] && Received[i]->LogicID.MovementMode_ExtCRC_ID == LogicID && Received[i]->RuntimeID == RuntimeID)
                SourceIndex = i;
        }

        UObject* Logic = nullptr;
        GetMovementModeExtensionLogic(Pawn, &Logic, LogicID);

        if (SourceIndex == -1 || !Logic || !((bool (*)(UObject*))Logic->VTable[0x7C])(Logic))
            continue;

        auto Runtime = Received[SourceIndex];
        Received[SourceIndex] = nullptr;
        Pushed.Add(Runtime);

        FMMERuntimeContext Context;
        MakeMovementModeExtensionContext(&Context, Runtime);
        PushMovementModeExtension(Logic, &Context);
    }

    Pending.Clear();

    return ServerMove_PerformMovementOG(CharacterMovement, MoveData);
}

void UFortMovementComp_Character__PostLoad()
{
    Hook(ImageBase + 0x6DE05DC, ServerMove_PerformMovement, ServerMove_PerformMovementOG);
}
