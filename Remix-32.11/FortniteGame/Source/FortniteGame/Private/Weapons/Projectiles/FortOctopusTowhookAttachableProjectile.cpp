#include "pch.h"
#include "Shared.h"

INIT_MODULE(AFortOctopusTowhookAttachableProjectile);

static void (*OnRep_ReplicatedAttachedInfoOG)(AFortOctopusTowhookAttachableProjectile*);
static void OnRep_ReplicatedAttachedInfo(AFortOctopusTowhookAttachableProjectile* Projectile)
{
    OnRep_ReplicatedAttachedInfoOG(Projectile);

    auto OwningVehicle = Projectile->OwningVehicle;

    if (!OwningVehicle)
        return;

    auto Component = (UPrimitiveComponent*)Projectile->AttachedInfo.Hit.Component.Get();

    static auto CanGrappleToComponent = (bool (*)(AFortOctopusVehicle*, UActorComponent*))(ImageBase + 0x91CD05C);

    if (!CanGrappleToComponent(OwningVehicle, Component))
        return;

    OwningVehicle->ReplicatedAttachState.Component = Component;
    (FVector&)OwningVehicle->ReplicatedAttachState.LocalLocation = UKismetMathLibrary::InverseTransformLocation(Component->K2_GetComponentToWorld(), Projectile->AttachedInfo.Hit.Location);
    (FVector&)OwningVehicle->ReplicatedAttachState.LocalNormal = UKismetMathLibrary::InverseTransformDirection(Component->K2_GetComponentToWorld(), Projectile->AttachedInfo.Hit.Normal);
    OwningVehicle->OnRep_ReplicatedAttachState();
}

void AFortOctopusTowhookAttachableProjectile__Init()
{
    Hook<AFortOctopusTowhookAttachableProjectile>(uint32(0x103), OnRep_ReplicatedAttachedInfo, OnRep_ReplicatedAttachedInfoOG);
}
