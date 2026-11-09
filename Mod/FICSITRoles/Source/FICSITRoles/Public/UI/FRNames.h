// Player-facing names/icons for config targets, taken from the game's own (already localized) data.
#pragma once

#include "CoreMinimal.h"
#include "FRConfig.h"

struct FSlateBrush;

namespace FRNames
{
	/** Display name of a restricted target (building, recipe, item, equipment, "DropPod"). */
	FICSITROLES_API FText Target(EFRPerm Perm, const FString& ShortName);
	/** Display name of a tech node: localization "tech.<id>.name" if present, else its first grant. */
	FICSITROLES_API FText Node(FName NodeId);
	/** Display name of an item by short class name. */
	FICSITROLES_API FText Item(const FString& ShortName);
	/** Small icon of an item (null if unknown). */
	FICSITROLES_API const FSlateBrush* ItemIcon(const FString& ShortName);
	/** Icon of a node's first build/item grant (null if none). */
	FICSITROLES_API const FSlateBrush* NodeIcon(FName NodeId);
}
