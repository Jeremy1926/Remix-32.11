#include "pch.h"
#include "Shared.h"
#include "FortniteGame/Source/FortniteGame/Public/Conversation/FortConversationTaskNode_Service.h"

int32 GetPrice(UFortConversationTaskNode_Service* Node, const FName& PricingKey, float Level)
{
    if (PricingKey.ComparisonIndex == 0)
        return -1;

    for (int32 i = 0; i < Node->PricingTables.Num(); i++)
    {
        auto CurveTable = Node->PricingTables.Data[i];

        if (!CurveTable)
            continue;

        bool bHasRow = false;

        for (auto& Row : GetRowMap(CurveTable))
        {
            if (Row.Key() == PricingKey)
            {
                bHasRow = true;
                break;
            }
        }

        if (!bHasRow)
            continue;

        FScalableFloat Price{};
        Price.Value = 1.f;
        Price.Curve.CurveTable = CurveTable;
        Price.Curve.RowName = PricingKey;

        return int32(Price.Evaluate(Level));
    }

    return -1;
}
