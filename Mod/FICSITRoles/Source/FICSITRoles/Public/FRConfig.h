// Declarative configuration (Resources/Config/FICSITRoles.json, schema 2), loaded once per process.
// A broken or missing file never crashes the game: built-in defaults are used and the
// problem is reported through Diagnostics.
#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "InputCoreTypes.h"

/** Restricted action families. Targets are class names without "_C" (patterns: trailing * = prefix). */
enum class EFRPerm : uint8 { Build = 0, Craft, Collect, Equip, Num };

struct FFRPermSet
{
	TArray<FString> Lists[(int32)EFRPerm::Num];
	TArray<FString>& Of(EFRPerm P) { return Lists[(int32)P]; }
	const TArray<FString>& Of(EFRPerm P) const { return Lists[(int32)P]; }
};

struct FFRDivisionDef
{
	FName Id;
	FLinearColor Color = FLinearColor::White;
	FString Icon;
	FFRPermSet Base;
	TArray<FString> StartRecipes;              // recipe class paths unlocked once the division is held
	TArray<TPair<FString, int32>> StartKit;    // item class names given once to each new member
};

struct FFRTechNode
{
	FName Id;
	FName Division;
	int32 Tier = 1;   // MAM row (1..9)
	int32 Col = -1;   // MAM column (0..6), -1 = automatic
	TArray<FName> Requires;
	TArray<TPair<FString, int32>> Cost;        // item class names, paid from the player's inventory
	FFRPermSet Grants;
};

/** Who may perform an action on a target. */
struct FFRPermRule
{
	TArray<FName> BaseOwners;
	TArray<TPair<FName, FName>> NodeGrants;    // (division, tech node)
};

class FICSITROLES_API FFRConfig
{
public:
	static const FFRConfig& Get();

	TArray<FFRDivisionDef> Divisions;
	TArray<FFRTechNode> Tech;
	FString TerminalKey = TEXT("F7");
	int32 MaxExpectedPlayers = 8;
	uint8 DefaultCoverage = 0; // EFRCoverage::Off
	float DenyCooldown = 3.f;
	bool bMamTrees = true;
	int32 Revision = 0;
	/** File actually loaded (user copy in Saved/ when available). */
	FString LoadedPath;
	TArray<FString> Diagnostics;
	/** Save migration: old node/division id -> new id (config key "renamed"). Chains are followed. */
	TMap<FName, FName> Renamed;
	FName Migrate(FName Id) const { for (int32 i = 0; i < 8; ++i) { const FName* N = Renamed.Find(Id); if (!N) break; Id = *N; } return Id; }

	const FFRDivisionDef* FindDivision(FName Id) const;
	int32 DivisionIndex(FName Id) const;
	const FFRTechNode* FindNode(FName Id) const;

	/** Rule for an action target (class name without _C). Null = common. Cached, game thread only. */
	const FFRPermRule* RuleFor(EFRPerm Perm, const FString& Name) const;

	/** Absolute path of a file in the plugin Resources folder. */
	static FString ResourcePath(const FString& Relative);

private:
	void Load();
	void LoadDefaults();
	static bool Matches(const FString& Pattern, const FString& Name);
	mutable TMap<FString, int32> RuleCache[(int32)EFRPerm::Num];
	mutable TIndirectArray<FFRPermRule> Rules; // stable addresses
};

/** Class name without the Blueprint "_C" suffix. */
FICSITROLES_API FString FRShortClassName(const UClass* Class);
/** Item descriptor by short class name ("Desc_IronPlate") or full path. Cached. */
FICSITROLES_API UClass* FRFindItem(const FString& NameOrPath);
/** Any loaded class by short name (no _C) deriving from Base. Cached. */
FICSITROLES_API UClass* FRFindClass(const FString& ShortName, UClass* Base);

/** Terminal hotkey: per-user override (Saved/FICSITRoles/UserSettings.json), else config default. */
FICSITROLES_API FKey FRTerminalKey();
FICSITROLES_API void FRSetTerminalKey(const FKey& Key);
FICSITROLES_API FText FRTerminalKeyName();
