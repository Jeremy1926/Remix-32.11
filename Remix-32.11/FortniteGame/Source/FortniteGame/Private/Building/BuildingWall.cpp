#include "pch.h"
#include "Shared.h"

INIT_POSTLOAD(ABuildingWall);

static int64 OpenableInterfaceOffset = 0;

static void Close_Implementation(void* OpenableInterface, AController* ControllerInstigator)
{
    static auto SetIsDoorOpen = (void (*)(ABuildingWall*, EDoorOpenStyle, TWeakObjectPtr<APawn>))(ImageBase + 0x955C49C);

    auto BuildingWall = (ABuildingWall*)(__int64(OpenableInterface) - OpenableInterfaceOffset);

    SetIsDoorOpen(BuildingWall, EDoorOpenStyle::Close, TWeakObjectPtr<APawn>(ControllerInstigator ? ControllerInstigator->Pawn : nullptr));
}

static void Open_Implementation(void* OpenableInterface, AController* ControllerInstigator, bool bRequestFastOpen)
{
    static auto SetIsDoorOpen = (void (*)(ABuildingWall*, EDoorOpenStyle, TWeakObjectPtr<APawn>))(ImageBase + 0x955C49C);

    auto BuildingWall = (ABuildingWall*)(__int64(OpenableInterface) - OpenableInterfaceOffset);

    BuildingWall->bFastOpenRequested = bRequestFastOpen;
    SetIsDoorOpen(BuildingWall, EDoorOpenStyle::Open, TWeakObjectPtr<APawn>(ControllerInstigator ? ControllerInstigator->Pawn : nullptr));
}

void ABuildingWall__PostLoad()
{
    OpenableInterfaceOffset = __int64(ABuildingWall::GetDefaultObj()->GetInterface(IOpenableInterface::StaticClass())) - __int64(ABuildingWall::GetDefaultObj());

    for (int32 i = 0; i < UObject::GObjects->Num(); i++)
    {
        auto Object = UObject::GObjects->GetByIndex(i);

        if (Object && Object->IsDefaultObject() && Object->IsA(ABuildingWall::StaticClass()))
        {
            auto OpenableVft = *(void***)(__int64(Object) + OpenableInterfaceOffset);

            VirtualSwap(OpenableVft, 0x1, Close_Implementation);
            VirtualSwap(OpenableVft, 0x2, Open_Implementation);
        }
    }
}
