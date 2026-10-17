#include "pch.h"
#include "Shared.h"

INIT_MODULE(ABuildingFoundation);

static void SetDynamicFoundationEnabled(UObject* Context, FFrame& Stack, void* Ret)
{
    bool bEnabled;
    Stack.StepCompiledIn(&bEnabled);
    Stack.IncrementCode();

    auto Foundation = (ABuildingFoundation*)Context;

    Foundation->DynamicFoundationRepData.EnabledState = bEnabled ? EDynamicFoundationEnabledState::Enabled : EDynamicFoundationEnabledState::Disabled;
    Foundation->OnRep_DynamicFoundationRepData();

    Foundation->FoundationEnabledState = bEnabled ? EDynamicFoundationEnabledState::Enabled : EDynamicFoundationEnabledState::Disabled;

    bool bSetup = true;

    if (bEnabled && Foundation->LevelToStream.ComparisonIndex == 0)
    {
        static auto SelectAndSetupMyBuildingLevel = (bool (*)(ABuildingFoundation*, void*))(ImageBase + 0x94EFF0C);
        bSetup = SelectAndSetupMyBuildingLevel(Foundation, nullptr);
    }

    static auto StreamInMyBuilding = (void (*)(ABuildingFoundation*, bool))(ImageBase + 0x94F3928);

    if (bSetup)
        StreamInMyBuilding(Foundation, false);
}

static void SetDynamicFoundationTransform(UObject* Context, FFrame& Stack, void* Ret)
{
    FTransform NewTransformTemp{};
    auto& NewTransform = Stack.StepCompiledInRef(&NewTransformTemp);
    Stack.IncrementCode();

    auto Foundation = (ABuildingFoundation*)Context;

    Foundation->DynamicFoundationTransform = NewTransform;
    Foundation->DynamicFoundationRepData.Rotation = UKismetMathLibrary::Quat_Rotator(NewTransform.Rotation);
    Foundation->DynamicFoundationRepData.Translation = NewTransform.Translation;
    Foundation->StreamingData.FoundationLocation = NewTransform.Translation;
    Foundation->OnRep_DynamicFoundationRepData();
}

void ABuildingFoundation__Init()
{
    ExecHook(ABuildingFoundation::StaticClass()->FindFunction("SetDynamicFoundationEnabled"), SetDynamicFoundationEnabled);
    ExecHook(ABuildingFoundation::StaticClass()->FindFunction("SetDynamicFoundationTransform"), SetDynamicFoundationTransform);
}
