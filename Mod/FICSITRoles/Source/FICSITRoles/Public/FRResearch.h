// Division technologies as real MAM research. Each tech node of the config becomes a MAM schematic in a
// "FICSIT - <division>" tree. Research = world unlock paid in the MAM; the habilitation stays personal to the
// division (enforced by AFRRolesSubsystem). Fixed pools of native classes because UCLASSes cannot come from JSON.
#pragma once

#include "CoreMinimal.h"
#include "FGSchematic.h"
#include "FGResearchTree.h"
#include "Unlocks/FGUnlock.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FRResearch.generated.h"

class UGameWorldModule;

UCLASS()
class FICSITROLES_API UFRTechUnlock : public UFGUnlock
{
	GENERATED_BODY()
public:
	UPROPERTY() FName Node;
	virtual void Unlock(class AFGUnlockSubsystem* UnlockSubsystem) override;
	virtual void Apply(class AFGUnlockSubsystem* UnlockSubsystem) override;
};

UCLASS(Abstract)
class FICSITROLES_API UFRTechSchematic : public UFGSchematic
{
	GENERATED_BODY()
public:
	UFRTechSchematic();
	UPROPERTY(EditDefaultsOnly, Category = "FICSIT Roles") FName Node;
	UPROPERTY(EditDefaultsOnly, Category = "FICSIT Roles") FName Division;
	UPROPERTY(Transient) TObjectPtr<UFGUnlock> InfoReward;
	/** Drops every reference before the rooted objects are released (second world load). */
	void Clear() { mCost.Reset(); mUnlocks.Reset(); mSchematicDependencies.Reset(); InfoReward = nullptr; }
	void Setup(FName InNode, FName InDivision, int32 Tier);
	/** Cost, names and icon need the game items loaded: filled once a world is running. */
	void Fill();
#if WITH_EDITOR
	/** Editor generator: cost and icon written into the Blueprint asset. */
	void GenFill(const struct FFRTechNode& N, class UTexture2D* FallbackIcon);
#endif
};

UCLASS() class FICSITROLES_API UFRTech0 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech1 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech2 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech3 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech4 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech5 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech6 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech7 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech8 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech9 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech10 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech11 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech12 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech13 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech14 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech15 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech16 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech17 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech18 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech19 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech20 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech21 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech22 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech23 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech24 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech25 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech26 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech27 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech28 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech29 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech30 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech31 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech32 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech33 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech34 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech35 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech36 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech37 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech38 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech39 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech40 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech41 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech42 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech43 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech44 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech45 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech46 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech47 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech48 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech49 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech50 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech51 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech52 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech53 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech54 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech55 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech56 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech57 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech58 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech59 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech60 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech61 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech62 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech63 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech64 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech65 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech66 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech67 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech68 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech69 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech70 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech71 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech72 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech73 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech74 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech75 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech76 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech77 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech78 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech79 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech80 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech81 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech82 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech83 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech84 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech85 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech86 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech87 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech88 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech89 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech90 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech91 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech92 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech93 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech94 : public UFRTechSchematic { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTech95 : public UFRTechSchematic { GENERATED_BODY() };

UCLASS(Abstract)
class FICSITROLES_API UFRTechTree : public UFGResearchTree
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = "FICSIT Roles") FName Division;
	void Setup(FName Division);
	/** Localized name/description only (cooked trees keep their dependencies and icon). */
	void SetTexts();
	void SetIcon(const FSlateBrush& Brush) { mResearchTreeIcon = Brush; }
	void SetUnlockDependency(class UFGAvailabilityDependency* Dependency) { mUnlockDependencies.Reset(); if (Dependency) mUnlockDependencies.Add(Dependency); }
};

UCLASS() class FICSITROLES_API UFRTechTree0 : public UFRTechTree { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTechTree1 : public UFRTechTree { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTechTree2 : public UFRTechTree { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTechTree3 : public UFRTechTree { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTechTree4 : public UFRTechTree { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTechTree5 : public UFRTechTree { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTechTree6 : public UFRTechTree { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRTechTree7 : public UFRTechTree { GENERATED_BODY() };

/** Editor tool: writes the MAM trees of the config as Blueprint assets in /FICSITRoles/MAM (cooked with the mod). */
UCLASS()
class FICSITROLES_API UFRAssetGen : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Returns the number of saved assets, -1 on error. Editor only. */
	UFUNCTION(BlueprintCallable, Category = "FICSIT Roles")
	static int32 GenerateMamAssets();
};

namespace FRResearch
{
	/** Builds trees/nodes (every machine) and registers them in the module (SML registers them on the server). */
	FICSITROLES_API void Register(UGameWorldModule* Module);
	/** Fills costs, names, icons once items are loaded (call after the world started). */
	FICSITROLES_API void FillContent();
	FICSITROLES_API TSubclassOf<UFGSchematic> SchematicOf(FName Node);
	FICSITROLES_API FName NodeOf(TSubclassOf<UFGSchematic> Schematic);
	FICSITROLES_API bool IsActive();
	/** True for the FICSIT research trees created by this mod. */
	FICSITROLES_API bool IsOurTree(UClass* Tree);
	/** Our own 'fully researched' test (the vanilla one crashes on runtime-built trees). */
	FICSITROLES_API bool IsTreeComplete(UObject* WorldContext, UClass* Tree, UClass* SchematicToIgnore);
}

