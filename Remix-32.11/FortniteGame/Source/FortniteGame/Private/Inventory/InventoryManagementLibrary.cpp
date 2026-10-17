#include "pch.h"
#include "Shared.h"

INIT_MODULE(UInventoryManagementLibrary);

static void RemoveItem(UObject* Context, FFrame& Stack, bool* Ret)
{
    FScriptInterface InventoryOwner{};
    FGuid ItemGuid{};
    int32 Count = 0;
    bool bForceRemoval = false;
    bool bForcePersistWhenEmpty = false;
    Stack.StepCompiledIn(&InventoryOwner);
    Stack.StepCompiledIn(&ItemGuid);
    Stack.StepCompiledIn(&Count);
    Stack.StepCompiledIn(&bForceRemoval);
    Stack.StepCompiledIn(&bForcePersistWhenEmpty);
    Stack.IncrementCode();

    auto Owner = InventoryOwner.ObjectPointer;
    auto Interface = InventoryOwner.InterfacePointer;
    *Ret = Owner && Interface && Owner->GetInterface(IFortInventoryOwnerInterface::StaticClass());

    if (!*Ret || !Owner->IsA(AActor::StaticClass()) || ((AActor*)Owner)->Role != ENetRole::ROLE_Authority)
        return;

    auto OwnerRemoveInventoryItem = (bool (*)(void*, FGuid&, int32, bool, bool))(*(void***)Interface)[0x3D];
    OwnerRemoveInventoryItem(Interface, ItemGuid, Count, bForceRemoval, bForcePersistWhenEmpty);
}

void UInventoryManagementLibrary__Init()
{
    ExecHook(UInventoryManagementLibrary::StaticClass()->FindFunction("RemoveItem"), RemoveItem);
}
