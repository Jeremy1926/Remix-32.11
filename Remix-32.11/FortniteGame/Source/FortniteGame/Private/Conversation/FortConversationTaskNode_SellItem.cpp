#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortConversationTaskNode_SellItem.h"
#include "FortniteGame/Source/FortniteGame/Private/Conversation/FortConversationRequirement_HasService.h"
#include "FortniteGame/Source/FortniteGame/Public/Building/BuildingItemWeaponUpgradeActor.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortConversationRequirement_ControllerRequirement.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortConversationTaskNode.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortConversationTaskNode_Service.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortNonPlayerConversationParticipantComponent.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortInventory.h"
#include "FortniteGame/Source/FortniteGame/Public/Items/FortPickup.h"
#include "FortniteGame/Source/FortniteGame/Public/Weapons/FortWeapon.h"

INIT_MODULE(UFortConversationTaskNode_SellItem);

static std::unordered_map<uint64, int32> Purchases;

static void AddChoice(TArray<FConversationBranchPoint>& BranchBuilder, const FConversationContext* Context, const FClientConversationOptionEntry& Choice)
{
    BranchBuilder.Reserve(1);

    auto& BranchPoint = BranchBuilder.Data[BranchBuilder.Num()];

    memset(&BranchPoint, 0, sizeof(FConversationBranchPoint));
    FConversationNodeHandle::CopyArray(BranchPoint.ReturnScopeStack, Context->ReturnScopeStack);
    BranchPoint.ClientChoice.CopyFrom(Choice);
    BranchBuilder.NumElements++;
}

static FString ObjectPathToString(const UObject* Object)
{
    return Object ? UKismetSystemLibrary::GetPathName(Object) : FString();
}

static FString IntToString(int32 Value)
{
    return FString(std::to_wstring(Value).c_str());
}

static AFortPlayerControllerAthena* GetPlayerController(const FConversationContext* Context)
{
    auto Conversation = Context ? Context->ActiveConversation : nullptr;

    if (!Conversation)
        return nullptr;

    for (int32 i = 0; i < Conversation->Participants.List.Num(); i++)
    {
        auto Actor = Conversation->Participants.List.Data[i].Actor;

        if (Actor && Actor->IsA(APlayerController::StaticClass()))
            return (AFortPlayerControllerAthena*)Actor;
    }

    return nullptr;
}

static int32 GetCurrencyCount(AFortPlayerControllerAthena* PlayerController, const UFortItemDefinition* Currency)
{
    auto Inventory = PlayerController ? (AFortInventory*)PlayerController->WorldInventory.ObjectPointer : nullptr;

    if (!Inventory || !Currency)
        return 0;

    int32 Count = 0;

    for (int32 i = 0; i < Inventory->Inventory.ReplicatedEntries.Num(); i++)
        if (Inventory->Inventory.ReplicatedEntries.Data[i].ItemDefinition == Currency)
            Count += Inventory->Inventory.ReplicatedEntries.Data[i].Count;

    return Count;
}

static uint64 GetPurchaseKey(UFortConversationTaskNode_SellItem* Node, const UObject* NPCComponent, const AActor* PlayerController)
{
    return GetObjectKey(NPCComponent) ^ (GetObjectKey(PlayerController) * 0x9E3779B97F4A7C15ull) ^ (uint64(uint32(Node->SellSlot)) << 56) ^ uint64(uint32(Node->OptionalFilterCategoryTag.TagName.ComparisonIndex));
}

static int32 GetItemCost(UFortConversationTaskNode_SellItem* Node, const UFortNonPlayerConversationParticipantComponent* NPCComponent, const UFortItemDefinition* ItemDefinition, bool& bOutDiscounted)
{
    bOutDiscounted = false;

    auto ItemPath = ObjectPathToString(ItemDefinition);
    auto Cost = ItemPath.Num() > 1 ? GetPrice(Node, FName(ItemPath.CStr()), float(ItemDefinition->Rarity) + 1.f) : -1;
    ItemPath.Free();

    if (Cost < 0)
        return 0;

    if (Node->bPriceDiscountsEnabled && NPCComponent)
    {
        auto Multiplier = GetDiscountMultiplier(NPCComponent);

        if (Multiplier < 1.f)
        {
            Cost = int32(float(Cost) * Multiplier);
            bOutDiscounted = true;
        }
    }

    return (std::max)(Cost, 0);
}

static int32 GetRemainingStock(UFortConversationTaskNode_SellItem* Node, const UObject* NPCComponent, const AActor* PlayerController)
{
    auto Found = Purchases.find(GetPurchaseKey(Node, NPCComponent, PlayerController));
    auto Purchased = Found == Purchases.end() ? 0 : Found->second;

    return Purchased >= 1 ? 0 : 1 - Purchased;
}

EConversationRequirementResult IsRequirementSatisfied(UFortConversationTaskNode_SellItem* Node, const FConversationContext* Context)
{
    auto NPCComponent = GetNPCComponent(Context);

    return NPCComponent && GetSaleItem(NPCComponent, Node->SellSlot, Node->OptionalFilterCategoryTag) ? EConversationRequirementResult::Passed : EConversationRequirementResult::FailedAndHidden;
}

static void GatherDynamicChoices(UFortConversationTaskNode_SellItem* Node, TArray<FConversationBranchPoint>* BranchBuilder, const FConversationContext* Context)
{
    auto NPCComponent = GetNPCComponent(Context);
    auto PlayerController = GetPlayerController(Context);
    auto SaleItem = NPCComponent ? GetSaleItem(NPCComponent, Node->SellSlot, Node->OptionalFilterCategoryTag) : nullptr;

    if (!SaleItem || !PlayerController)
        return;

    auto Choice = FClientConversationOptionEntry::Allocate();
    bool bDiscounted = false;
    auto Cost = GetItemCost(Node, NPCComponent, SaleItem->ItemDefinition, bDiscounted);
    auto Remaining = GetRemainingStock(Node, NPCComponent, PlayerController);
    auto bCanAfford = GetCurrencyCount(PlayerController, Node->ResourceCurrency) >= Cost;

    if (Remaining > 0)
    {
        TArray<FFormatArgumentData> InArgs;
        FFormatArgumentData Argument{};

        Argument.ArgumentName = FString(L"0");
        Argument.ArgumentValueType = EFormatArgumentType::Text;
        Argument.ArgumentValue = SaleItem->ItemDefinition->ItemName;
        InArgs.Add(Argument);

        Choice->ChoiceText = UKismetTextLibrary::Format(Node->BuyTextFormat, InArgs);

        InArgs.Data[0].ArgumentName.Free();
        InArgs.Free();
    }
    else
        FText::CopyTo(&Choice->ChoiceText, &Node->SoldOutTextFormat);

    CopyTags(Choice->ChoiceTags, Node->NodeTags);

    Choice->ChoiceType = Remaining > 0 && bCanAfford ? EConversationChoiceType::UserChoiceAvailable : EConversationChoiceType::UserChoiceUnavailable;
    Choice->ChoiceReference.NodeReference.NodeGUID = Node->Compiled_NodeGUID;

    auto& ExtraData = Choice->ExtraData;
    GatherStaticExtraData(Node, Context, &ExtraData);

    auto ItemPath = ObjectPathToString(SaleItem->ItemDefinition);
    auto CurrencyPath = ObjectPathToString(Node->ResourceCurrency);
    auto StackSize = IntToString(SaleItem->StackSize);
    auto RemainingString = IntToString(Remaining);
    auto CostString = IntToString(Cost);

    AddExtraData(ExtraData, L"item", ItemPath);
    AddExtraData(ExtraData, L"stacksize", StackSize);
    AddExtraData(ExtraData, L"remaining", RemainingString);
    AddExtraData(ExtraData, L"cost", CostString);
    AddExtraData(ExtraData, L"currency", CurrencyPath);
    AddExtraData(ExtraData, L"discountprice", bDiscounted ? L"1" : L"0");
    AddExtraData(ExtraData, L"locked", L"0");
    AddExtraData(ExtraData, L"unlock_level", L"0");

    if (Remaining <= 0)
        AddExtraData(ExtraData, L"cannotpurchasereason", L"1");
    else if (!bCanAfford)
        AddExtraData(ExtraData, L"cannotpurchasereason", L"0");

    ItemPath.Free();
    CurrencyPath.Free();
    StackSize.Free();
    RemainingString.Free();
    CostString.Free();

    AddChoice(*BranchBuilder, Context, *Choice);
    Choice->Free();
}

static FConversationTaskResult* ExecuteTaskNode(UFortConversationTaskNode_SellItem* Node, FConversationTaskResult* Result, const FConversationContext* Context)
{
    memset(Result, 0, sizeof(FConversationTaskResult));
    Result->Type = EConversationTaskResultType::ReturnToCurrentClientChoice;

    auto NPCComponent = GetNPCComponent(Context);
    auto PlayerController = GetPlayerController(Context);
    auto SaleItem = NPCComponent ? GetSaleItem(NPCComponent, Node->SellSlot, Node->OptionalFilterCategoryTag) : nullptr;

    if (!SaleItem || !PlayerController || !PlayerController->WorldInventory.ObjectPointer || GetRemainingStock(Node, NPCComponent, PlayerController) <= 0)
        return Result;

    bool bDiscounted = false;
    auto Cost = GetItemCost(Node, NPCComponent, SaleItem->ItemDefinition, bDiscounted);

    if (GetCurrencyCount(PlayerController, Node->ResourceCurrency) < Cost)
        return Result;

    ConsumeResource(PlayerController, Node->ResourceCurrency, Cost);

    auto Inventory = (AFortInventory*)PlayerController->WorldInventory.ObjectPointer;
    auto Pawn = PlayerController->MyFortPawn;
    auto WeaponDefinition = SaleItem->ItemDefinition->IsA(UFortWeaponItemDefinition::StaticClass()) ? (UFortWeaponItemDefinition*)SaleItem->ItemDefinition : nullptr;
    auto Stats = WeaponDefinition ? GetStats(WeaponDefinition) : nullptr;
    auto LoadedAmmo = Stats ? Stats->ClipSize : 0;
    static auto ApplyRandomWeaponModTag = FName(L"Conversation.Sell.ApplyRandomWeaponMod");
    bool bApplyRandomWeaponMod = false;

    if (WeaponDefinition && Pawn && SaleItem->Row)
        for (int32 i = 0; i < SaleItem->Row->LootTags.GameplayTags.Num() && !bApplyRandomWeaponMod; i++)
            bApplyRandomWeaponMod = SaleItem->Row->LootTags.GameplayTags.Data[i].TagName == ApplyRandomWeaponModTag;

    if (bApplyRandomWeaponMod)
    {
        if (auto Pickup = SpawnPickup(Pawn->K2_GetActorLocation(), SaleItem->ItemDefinition, SaleItem->StackSize, LoadedAmmo, EFortPickupSourceTypeFlag::Other, EFortPickupSpawnSource::Unset, Pawn))
            ApplyModSetToPickup(GetRandomWeaponModSetData(NPCComponent, WeaponDefinition), Pickup);
    }
    else
        GiveItem(Inventory, SaleItem->ItemDefinition, SaleItem->StackSize, LoadedAmmo);

    Purchases[GetPurchaseKey(Node, NPCComponent, PlayerController)]++;
    Result->Type = EConversationTaskResultType::AdvanceConversation;

    return Result;
}

void UFortConversationTaskNode_SellItem__Init()
{
    Hook<UFortConversationTaskNode_SellItem>(uint32(0x60), (EConversationRequirementResult (*)(UFortConversationTaskNode_SellItem*, const FConversationContext*))IsRequirementSatisfied);
    Hook<UFortConversationTaskNode_SellItem>(uint32(0x65), GatherDynamicChoices);
    Hook<UFortConversationTaskNode_SellItem>(uint32(0x5F), ExecuteTaskNode);
}
