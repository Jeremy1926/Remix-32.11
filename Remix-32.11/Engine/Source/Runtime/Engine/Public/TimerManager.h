#pragma once
#include "pch.h"

void SetActorTimer(AActor* Actor, void* Callback, uint64& Handle, float Delay);
void ClearTimer(AActor* Actor, uint64& Handle);
