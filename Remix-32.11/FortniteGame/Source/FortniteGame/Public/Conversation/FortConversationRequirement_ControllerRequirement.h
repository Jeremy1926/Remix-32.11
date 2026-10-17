#pragma once
#include "pch.h"

bool IsControllerRequirementMet(UFortControllerRequirement* Requirement, const FConversationContext* Context, const FGameplayTag& ParticipantID);
EConversationRequirementResult IsRequirementSatisfied(UFortConversationRequirement_ControllerRequirement* Node, const FConversationContext* Context);
