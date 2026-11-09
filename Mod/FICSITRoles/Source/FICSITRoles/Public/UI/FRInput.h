// Terminal hotkey as a real Enhanced Input action, listed in the game's Options > Controls menu.
#pragma once

#include "CoreMinimal.h"

class UInputAction;
class UFGInputMappingContext;
class APlayerController;

namespace FRInput
{
	/** Runtime action / mapping context (created once, rooted). */
	FICSITROLES_API UInputAction* Action();
	FICSITROLES_API UFGInputMappingContext* Context();
	/** Adds the context to the local player and registers it for player remapping. True on success. */
	FICSITROLES_API bool Install(APlayerController* Controller);
	/** Key currently bound by the player (remap included), or the default. */
	FICSITROLES_API FKey CurrentKey(APlayerController* Controller);
}
