#include "pch.h"
#include "Shared.h"
#include "Config.h"
#include "FortniteGame/Source/FortniteAI/Public/AI/FortAthenaAIBotController.h"
#include "FortniteGame/Source/FortniteGame/Public/Athena/FortPlayerControllerAthena.h"

INIT_POSTLOAD(UFortControllerComponent_Aircraft);

static void (*EnterAircraftOG)(UFortControllerComponent_Aircraft*, AActor*);
static void EnterAircraft(UFortControllerComponent_Aircraft* AircraftComponent, AActor* Aircraft)
{
    auto PlayerController = AircraftComponent->GetOwner();

    if (!PlayerController || !PlayerController->IsA(AFortPlayerControllerAthena::StaticClass()))
        return EnterAircraftOG(AircraftComponent, Aircraft);

    if (auto WorldInventory = (AFortInventory*)((AFortPlayerControllerAthena*)PlayerController)->WorldInventory.ObjectPointer)
        RemoveDroppableEntries(WorldInventory);

    EnterAircraftOG(AircraftComponent, Aircraft);
}

static void ServerAttemptAircraftJump(UFortControllerComponent_Aircraft* AircraftComponent, const FRotator& ClientRotation)
{
    static auto IsAircraftJumpAllowed = (bool (*)(UFortControllerComponent_Aircraft*))(ImageBase + 0x96670FC);
    static auto KickFromAircraft = (void (*)(UFortControllerComponent_Aircraft*))(ImageBase + 0x9667574);

    auto PlayerController = (AFortPlayerControllerAthena*)AircraftComponent->GetOwner();

    if (PlayerController && PlayerController->IsA(AFortAthenaAIBotController::StaticClass()))
    {
        if (IsAircraftJumpAllowed(AircraftComponent))
            KickFromAircraft(AircraftComponent);

        return;
    }

    if (!PlayerController || !PlayerController->IsA(AFortPlayerControllerAthena::StaticClass()) || !PlayerController->IsInAircraft())
        return;

    PlayerController->StateName = FName(L"Inactive");

    if (PlayerController->Pawn)
        PlayerController->UnPossess();

    UWorld::GetWorld()->AuthorityGameMode->RestartPlayer(PlayerController);
    PlayerController->SetControlRotation(ClientRotation);
    AircraftComponent->OnExitAircraftEvent.Process(PlayerController);

    if (bLateGame)
        PlaceLateGamePawn(PlayerController, (AFortPawn*)PlayerController->Pawn);
}

void UFortControllerComponent_Aircraft__PostLoad()
{
    Hook(ImageBase + 0x965F1E0, (void (*)(UFortControllerComponent_Aircraft*, AActor*))EnterAircraft, EnterAircraftOG);
    Hook<UFortControllerComponent_Aircraft>(uint32(0xAA), ServerAttemptAircraftJump);
}
