"""Cooked MAM trees: editor generator (UFRAssetGen) writes Blueprint trees/schematics into /FICSITRoles/MAM;
runtime loads them instead of the native class pools (which crashed in the MAM tooltip)."""
import os, re

ROOT = r"F:\FICSIT-Roles-Rebuild\Mod\FICSITRoles"
SRC = os.path.join(ROOT, r"Source\FICSITRoles")


def rw(path, fn):
    s = open(path, encoding="utf-8").read()
    open(path, "w", encoding="utf-8").write(fn(s))


def must(s, a):
    assert a in s, a[:120]
    return a


# ---------------------------------------------------------------- uplugin / Build.cs
rw(os.path.join(ROOT, "FICSITRoles.uplugin"), lambda s: s.replace(must(s, '"CanContainContent": false'), '"CanContainContent": true'))
rw(os.path.join(SRC, "FICSITRoles.Build.cs"), lambda s: s.replace(
    must(s, '"UMG", "GameplayTags" });\n'),
    '"UMG", "GameplayTags" });\n\t\tif (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "AssetTools" });\n'))


# ---------------------------------------------------------------- header
def header(s):
    s = s.replace(must(s, '#include "Unlocks/FGUnlock.h"'), '#include "Unlocks/FGUnlock.h"\n#include "Kismet/BlueprintFunctionLibrary.h"')
    s = s.replace(must(s, "	UFRTechSchematic();\n	FName Node;\n	FName Division;"),
                  "	UFRTechSchematic();\n	UPROPERTY(EditDefaultsOnly, Category = \"FICSIT Roles\") FName Node;\n	UPROPERTY(EditDefaultsOnly, Category = \"FICSIT Roles\") FName Division;")
    s = s.replace(must(s, "	FName Division;\n	void Setup(FName Division);"),
                  "	UPROPERTY(EditDefaultsOnly, Category = \"FICSIT Roles\") FName Division;\n	void Setup(FName Division);\n	/** Localized name/description only (cooked trees keep their dependencies and icon). */\n	void SetTexts();")
    s = s.replace(must(s, "namespace FRResearch\n{"), '''/** Editor tool: writes the MAM trees of the config as Blueprint assets in /FICSITRoles/MAM (cooked with the mod). */
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
{''')
    return s


rw(os.path.join(SRC, "Public", "FRResearch.h"), header)


# ---------------------------------------------------------------- cpp
def cpp(s):
    s = s.replace(must(s, '#include "UObject/Package.h"\n'), '''#include "UObject/Package.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#if WITH_EDITOR
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "AssetToolsModule.h"
#include "AssetImportTask.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#endif
''', 1)

    # dependency helper: optional flags, no rooting for assets
    s = s.replace(must(s, "	UFGAvailabilityDependency* MakeAlwaysMetDependency(UObject* Outer)\n"),
                  "	UFGAvailabilityDependency* MakeAlwaysMetDependency(UObject* Outer, EObjectFlags Flags = RF_NoFlags)\n")
    s = s.replace(must(s, "		UFGAvailabilityDependency* Dep = NewObject<UFGAvailabilityDependency>(Outer, DepClass, MakeUniqueObjectName(Outer, DepClass, TEXT(\"AlwaysMet\")));"),
                  "		UFGAvailabilityDependency* Dep = NewObject<UFGAvailabilityDependency>(Outer, DepClass, MakeUniqueObjectName(Outer, DepClass, TEXT(\"AlwaysMet\")), Flags);")
    s = s.replace(must(s, "		Inner->SetObjectPropertyValue(Helper.GetRawPtr(Index), Starting);\n		Root(Dep);"),
                  "		Inner->SetObjectPropertyValue(Helper.GetRawPtr(Index), Starting);\n		if (Flags == RF_NoFlags) Root(Dep);")

    # shared layout builder + cooked paths, appended at the end of the anonymous namespace (before UFRTechUnlock)
    builder = r'''
	FString AssetId(FName Id) { return Id.ToString().Replace(TEXT("."), TEXT("_")).Replace(TEXT("-"), TEXT("_")); }
	FString TreeAsset(FName Division) { return TEXT("FRT_") + AssetId(Division); }
	FString SchematicAsset(FName Node) { return TEXT("FRS_") + AssetId(Node); }
	FString CookedClassPath(const FString& Asset) { return FString::Printf(TEXT("/FICSITRoles/MAM/%s.%s_C"), *Asset, *Asset); }

	TArray<const FFRTechNode*> NodesOf(FName Division)
	{
		TArray<const FFRTechNode*> Nodes;
		for (const FFRTechNode& N : FFRConfig::Get().Tech) if (N.Division == Division) Nodes.Add(&N);
		Nodes.StableSort([](const FFRTechNode& A, const FFRTechNode& B) { return A.Tier < B.Tier; });
		return Nodes;
	}

	/** Grid (one row per tier, centred on column 3) + vanilla-format links. */
	TArray<UFGResearchTreeNode*> BuildNodes(UObject* Outer, UClass* NodeClass, const FNodeLayout& Layout, const TArray<const FFRTechNode*>& Nodes,
		TFunctionRef<UClass*(const FFRTechNode&)> SchematicFor, EObjectFlags Flags)
	{
		TMap<FName, FIntPoint> Coords;
		TMap<int32, int32> TierCount, TierIndex;
		for (const FFRTechNode* N : Nodes) TierCount.FindOrAdd(N->Tier) += 1;
		for (const FFRTechNode* N : Nodes)
		{
			const int32 InTier = TierCount[N->Tier];
			int32& I = TierIndex.FindOrAdd(N->Tier);
			Coords.Add(N->Id, FIntPoint(FMath::Max(0, 3 - (InTier - 1) / 2) + I, (N->Tier - 1) * 2));
			++I;
		}
		TMap<FName, TArray<TPair<FIntPoint, TArray<FIntPoint>>>> ChildRoads;
		for (const FFRTechNode* N : Nodes)
			for (const FName R : N->Requires)
			{
				const FIntPoint* P = Coords.Find(R);
				if (!P) continue;
				const FIntPoint C = Coords[N->Id];
				TArray<FIntPoint> Path;
				FIntPoint Cur = *P;
				while (Cur.X != C.X) { Cur.X += C.X > Cur.X ? 1 : -1; Path.Add(Cur); }
				while (Cur.Y != C.Y) { Cur.Y += C.Y > Cur.Y ? 1 : -1; Path.Add(Cur); }
				ChildRoads.FindOrAdd(R).Emplace(C, Path);
			}
		TArray<UFGResearchTreeNode*> Result;
		for (const FFRTechNode* N : Nodes)
		{
			UClass* SchematicClass = SchematicFor(*N);
			if (!SchematicClass) continue;
			UFGResearchTreeNode* TreeNode = NewObject<UFGResearchTreeNode>(Outer, NodeClass, MakeUniqueObjectName(Outer, NodeClass, TEXT("TechNode")), Flags);
			TArray<FIntPoint> Parents;
			for (const FName R : N->Requires) if (const FIntPoint* P = Coords.Find(R)) Parents.Add(*P);
			Layout.Write(TreeNode, SchematicClass, Coords[N->Id], Parents);
			if (const TArray<TPair<FIntPoint, TArray<FIntPoint>>>* Roads = ChildRoads.Find(N->Id)) Layout.WriteRoads(TreeNode, *Roads);
			Result.Add(TreeNode);
		}
		return Result;
	}
}

// ---------------------------------------------------------------------------------------------
void UFRTechUnlock::Unlock('''
    s = s.replace(must(s, "}\n\n// ---------------------------------------------------------------------------------------------\nvoid UFRTechUnlock::Unlock("), builder, 1)

    # Fill: cooked classes only get localized texts (costs/icons/objects are in the asset)
    s = s.replace(must(s, "		FFRLoc::F(TEXT(\"mam.node_footer\"), FFRLoc::Division(Division)));\n	mCost.Reset();"),
                  "		FFRLoc::F(TEXT(\"mam.node_footer\"), FFRLoc::Division(Division)));\n	if (!GetClass()->HasAnyClassFlags(CLASS_Native)) return; // cooked asset: keep its own data\n	mCost.Reset();")

    # Tree texts split out
    s = s.replace(must(s, '''	Division = InDivision;
	const FText Name = FFRLoc::F(TEXT("mam.tree_name"), FFRLoc::Division(Division));
	mPreUnlockDisplayName = Name;
	mDisplayName = Name;
	mPreUnlockDescription = FFRLoc::T(TEXT("mam.tree_desc"));
	mPostUnlockDescription = mPreUnlockDescription;
'''), '''	Division = InDivision;
	SetTexts();
''')
    s = s.replace(must(s, "// ---------------------------------------------------------------------------------------------\nvoid FRResearch::Register("),
                  '''void UFRTechTree::SetTexts()
{
	const FText Name = FFRLoc::F(TEXT("mam.tree_name"), FFRLoc::Division(Division));
	mPreUnlockDisplayName = Name;
	mDisplayName = Name;
	mPreUnlockDescription = FFRLoc::T(TEXT("mam.tree_desc"));
	mPostUnlockDescription = mPreUnlockDescription;
}

// ---------------------------------------------------------------------------------------------
void FRResearch::Register(''')

    # Register: cooked assets only (native pools crashed), fallback = F7 terminal
    start = s.index("	UClass* NodeClass = LoadClass<UFGResearchTreeNode>(nullptr, TEXT(\"/Game/FactoryGame/Schematics/Research/BPD_ResearchTreeNode.BPD_ResearchTreeNode_C\"));\n	FNodeLayout Layout;")
    end = s.index("void FRResearch::FillContent()")
    s = s[:start] + '''	// Cooked Blueprint trees shipped in the mod pak (generated by UFRAssetGen in the editor).
	int32 Count = 0;
	for (const FFRDivisionDef& Div : Config.Divisions)
	{
		UClass* Tree = LoadClass<UFGResearchTree>(nullptr, *CookedClassPath(TreeAsset(Div.Id)));
		if (!Tree) continue;
		for (const FFRTechNode* N : NodesOf(Div.Id))
		{
			UClass* S = LoadClass<UFGSchematic>(nullptr, *CookedClassPath(SchematicAsset(N->Id)));
			if (!S) { UE_LOG(LogFRResearch, Warning, TEXT("Cooked schematic missing for %s"), *N->Id.ToString()); continue; }
			GNodeSchematics.Add(N->Id, S);
			if (Module) Module->mSchematics.AddUnique(S);
			++Count;
		}
		CastChecked<UFRTechTree>(Tree->GetDefaultObject())->SetTexts();
		GTrees.Add(Tree);
		if (Module) Module->mResearchTrees.AddUnique(Tree);
	}
	GMamActive = Count > 0;
	UE_LOG(LogFRResearch, Display, TEXT("MAM: %d technologies in %d cooked FICSIT trees%s"), Count, GTrees.Num(),
		GMamActive ? TEXT("") : TEXT(" - assets missing, technologies use the F7 terminal"));
}

''' + s[end:]

    # FillContent: cooked trees already have dependencies; skip the CDO repair loop for them
    s = s.replace(must(s, "		if (UFGResearchTree::GetUnlockDependencies(TreeClass).Num() > 0) continue;"),
                  "		if (!TreeClass->HasAnyClassFlags(CLASS_Native) || UFGResearchTree::GetUnlockDependencies(TreeClass).Num() > 0) continue;")

    # Generator
    s += r'''
// ---------------------------------------------------------------------------------------------
int32 UFRAssetGen::GenerateMamAssets()
{
#if WITH_EDITOR
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.SearchAllAssets(true);
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("FICSITRoles"));
	if (!Plugin || !Plugin->CanContainContent()) { UE_LOG(LogFRResearch, Error, TEXT("FICSITRoles plugin content not mounted")); return -1; }

	UClass* NodeClass = LoadClass<UFGResearchTreeNode>(nullptr, TEXT("/Game/FactoryGame/Schematics/Research/BPD_ResearchTreeNode.BPD_ResearchTreeNode_C"));
	FNodeLayout Layout;
	if (!NodeClass || !Layout.Resolve(NodeClass)) { UE_LOG(LogFRResearch, Error, TEXT("BPD_ResearchTreeNode unavailable")); return -1; }

	TArray<UPackage*> Packages;
	auto MakeBlueprint = [&Packages](const FString& Name, UClass* Parent) -> UClass*
	{
		UPackage* Package = CreatePackage(*(TEXT("/FICSITRoles/MAM/") + Name));
		Package->FullyLoad();
		UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(Parent, Package, FName(*Name), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		if (!BP || !BP->GeneratedClass) { UE_LOG(LogFRResearch, Error, TEXT("Blueprint creation failed: %s"), *Name); return nullptr; }
		FAssetRegistryModule::AssetCreated(BP);
		Packages.Add(Package);
		return BP->GeneratedClass;
	};
	auto ImportTexture = [](const FString& File, const FString& Name) -> UTexture2D*
	{
		if (!FPaths::FileExists(File)) return nullptr;
		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = File;
		Task->DestinationPath = TEXT("/FICSITRoles/MAM");
		Task->DestinationName = Name;
		Task->bAutomated = true;
		Task->bReplaceExisting = true;
		Task->bSave = true;
		FAssetToolsModule::GetModule().Get().ImportAssetTasks({ Task });
		return Task->GetObjects().Num() > 0 ? Cast<UTexture2D>(Task->GetObjects()[0]) : nullptr;
	};

	const EObjectFlags SubFlags = RF_Public | RF_ArchetypeObject | RF_Transactional;
	const FString UiDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("UI"));
	int32 Saved = 0;
	for (const FFRDivisionDef& Div : FFRConfig::Get().Divisions)
	{
		const TArray<const FFRTechNode*> Nodes = NodesOf(Div.Id);
		if (Nodes.Num() == 0) continue;
		UTexture2D* DivIcon = ImportTexture(FPaths::Combine(UiDir, FString::Printf(TEXT("div_%s.png"), *Div.Icon)), TEXT("T_div_") + AssetId(Div.Id));
		if (!DivIcon) DivIcon = ImportTexture(FPaths::Combine(UiDir, FString::Printf(TEXT("div_%s.png"), *Div.Id.ToString())), TEXT("T_div_") + AssetId(Div.Id));

		TMap<FName, UClass*> Classes;
		for (const FFRTechNode* N : Nodes)
		{
			UClass* C = MakeBlueprint(SchematicAsset(N->Id), UFRTechSchematic::StaticClass());
			if (!C) continue;
			UFRTechSchematic* S = CastChecked<UFRTechSchematic>(C->GetDefaultObject());
			S->Setup(N->Id, Div.Id, N->Tier);
			S->GenFill(*N, DivIcon);
			Classes.Add(N->Id, C);
		}
		UClass* TreeClass = MakeBlueprint(TreeAsset(Div.Id), UFRTechTree::StaticClass());
		if (!TreeClass) continue;
		UFRTechTree* Tree = CastChecked<UFRTechTree>(TreeClass->GetDefaultObject());
		Tree->Setup(Div.Id);
		if (DivIcon)
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(DivIcon);
			Brush.ImageSize = FVector2D(256, 256);
			Tree->SetIcon(Brush);
		}
		Tree->SetUnlockDependency(MakeAlwaysMetDependency(Tree, SubFlags));
		const TArray<UFGResearchTreeNode*> TreeNodes = BuildNodes(Tree, NodeClass, Layout, Nodes,
			[&Classes](const FFRTechNode& N) { UClass* const* C = Classes.Find(N.Id); return C ? *C : nullptr; }, SubFlags);
		UFGResearchTree::SetNodes(TreeClass, TreeNodes);
		UE_LOG(LogFRResearch, Display, TEXT("Generated tree %s: %d nodes, dependency=%d"), *TreeAsset(Div.Id), TreeNodes.Num(),
			UFGResearchTree::GetUnlockDependencies(TreeClass).Num());
	}

	for (UPackage* Package : Packages)
	{
		Package->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		if (UPackage::SavePackage(Package, nullptr, *File, Args)) ++Saved;
		else UE_LOG(LogFRResearch, Error, TEXT("Save failed: %s"), *File);
	}
	UE_LOG(LogFRResearch, Display, TEXT("FICSIT Roles MAM assets saved: %d / %d"), Saved, Packages.Num());
	return Saved;
#else
	return -1;
#endif
}

#if WITH_EDITOR
void UFRTechSchematic::GenFill(const FFRTechNode& N, UTexture2D* FallbackIcon)
{
	mDisplayName = FText::FromString(N.Id.ToString());
	mCost.Reset();
	for (const TPair<FString, int32>& C : N.Cost)
	{
		if (UClass* Item = LoadGameClass(C.Key)) mCost.Add(FItemAmount(Item, C.Value));
		else UE_LOG(LogFRResearch, Warning, TEXT("Cost item %s not found for %s"), *C.Key, *N.Id.ToString());
	}
	mUnlocks.Reset();
	UTexture2D* Icon = nullptr;
	for (const FString& T : N.Grants.Of(EFRPerm::Build))
	{
		FString Name = T;
		Name.RemoveFromEnd(TEXT("*"));
		if (Name.RemoveFromStart(TEXT("Build_")))
			if (UClass* Desc = LoadGameClass(TEXT("Desc_") + Name)) Icon = UFGItemDescriptor::GetSmallIcon(Desc);
		if (Icon) break;
	}
	if (!Icon)
		for (const FString& T : N.Grants.Of(EFRPerm::Collect))
			if (UClass* Item = LoadGameClass(T)) if ((Icon = UFGItemDescriptor::GetSmallIcon(Item)) != nullptr) break;
	if (!Icon) Icon = FallbackIcon;
	if (Icon)
	{
		mSchematicIcon.SetResourceObject(Icon);
		mSchematicIcon.ImageSize = FVector2D(256, 256);
		mSmallSchematicIcon = Icon;
	}
}
#endif
'''
    return s


rw(os.path.join(SRC, "Private", "FRResearch.cpp"), cpp)

# GenFill declaration
rw(os.path.join(SRC, "Public", "FRResearch.h"), lambda s: s.replace(
    must(s, "	void Fill();\n};"),
    "	void Fill();\n#if WITH_EDITOR\n	/** Editor generator: cost and icon written into the Blueprint asset. */\n	void GenFill(const struct FFRTechNode& N, class UTexture2D* FallbackIcon);\n#endif\n};", 1))

# sync must keep generated Content
rw(r"F:\FICSIT-Roles-Rebuild\scripts\sync.ps1", lambda s: s.replace(must(s, "'\\\\(Binaries|Intermediate)\\\\'"), "'\\\\(Binaries|Intermediate|Content)\\\\'"))
print("patched v16")
