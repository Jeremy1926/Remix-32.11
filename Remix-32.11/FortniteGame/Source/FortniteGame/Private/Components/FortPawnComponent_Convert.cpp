#include "pch.h"
#include "Shared.h"

INIT_MODULE(UFortPawnComponent_Convert);

static void* (*InternalConvertPawnOG)(UFortPawnComponent_Convert*, AFortPlayerPawnAthena*, UFortPawnComponent_Convert*);
static void* InternalConvertPawn(UFortPawnComponent_Convert* ConverterComponent, AFortPlayerPawnAthena* ConvertedPawn, UFortPawnComponent_Convert* ConvertedComponent)
{
    if (ConverterComponent && ConvertedComponent && ConvertedPawn && ConvertedPawn->IsA(AFortPlayerPawnAthena::StaticClass()) && ConvertedPawn->IsDBNO())
    {
        auto ConvertedType = ConvertedComponent->ConvertedType;
        auto MaxConverted = ConvertedType != EConvertedType::NPC ? ConverterComponent->GetMaxConvertedCreature() : ConverterComponent->GetMaxConvertedNPC();

        if (ConvertedType != EConvertedType::Unknown && MaxConverted > 0)
        {
            ConvertedPawn->ForceReviveFromDBNO(nullptr);
            ConvertedPawn->SetHealth(ConvertedPawn->GetMaxHealth());
            ConvertedPawn->UnCrouch(false);
        }
    }

    return InternalConvertPawnOG(ConverterComponent, ConvertedPawn, ConvertedComponent);
}

void UFortPawnComponent_Convert__Init()
{
    Hook(ImageBase + 0x96ECAB0, InternalConvertPawn, InternalConvertPawnOG);
}
