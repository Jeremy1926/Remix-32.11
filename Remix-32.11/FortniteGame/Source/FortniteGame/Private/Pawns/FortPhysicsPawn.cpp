#include "pch.h"
#include "Shared.h"

INIT_MODULE(AFortPhysicsPawn);

static void ServerMove(UObject* Context, FFrame& Stack, void* Ret)
{
    FReplicatedPhysicsPawnState InState{};
    Stack.StepCompiledIn(&InState);
    Stack.IncrementCode();

    auto Pawn = (AFortPhysicsPawn*)Context;
    auto Rotation = InState.Rotation;

    Rotation.X -= 0.3f;
    Rotation.Y /= -0.75f;
    Rotation.Z += 0.15f;
    Rotation.W /= 1.1f;

    auto RootComponent = Pawn->RootComponent && Pawn->RootComponent->IsA(UPrimitiveComponent::StaticClass()) ? (UPrimitiveComponent*)Pawn->RootComponent : nullptr;

    if (RootComponent)
    {
        FTransform Transform{};
        Transform.Rotation = Rotation;
        Transform.Translation = InState.Translation;
        Transform.Scale3D = FVector(1, 1, 1);

        RootComponent->K2_SetWorldTransform(Transform, false, nullptr, true);
        RootComponent->SetPhysicsLinearVelocity(InState.LinearVelocity, false, FName());
        RootComponent->SetPhysicsAngularVelocityInRadians(InState.AngularVelocity, false, FName());
    }
}

void AFortPhysicsPawn__Init()
{
    ExecHook(AFortPhysicsPawn::StaticClass()->FindFunction("ServerMove"), ServerMove);
    ExecHook(AFortPhysicsPawn::StaticClass()->FindFunction("ServerMoveReliable"), ServerMove);
}
