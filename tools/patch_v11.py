"""MAM trees get a real, always-met unlock dependency (fixes the hover crash) + ChildrenAndRoads layout log."""
import os

SRC = r"F:\FICSIT-Roles-Rebuild\Mod\FICSITRoles\Source\FICSITRoles"


def rw(rel, fn):
    p = os.path.join(SRC, rel)
    s = open(p, encoding="utf-8").read()
    open(p, "w", encoding="utf-8").write(fn(s))


def must(s, a):
    assert a in s, a[:100]
    return a


rw("FICSITRoles.Build.cs", lambda s: s.replace(must(s, '"Projects", "Json"'), '"Projects", "Json", "AssetRegistry"'))
rw("Public/FRResearch.h", lambda s: s.replace(must(s, "	void SetIcon(const FSlateBrush& Brush) { mResearchTreeIcon = Brush; }"),
   "	void SetIcon(const FSlateBrush& Brush) { mResearchTreeIcon = Brush; }\n	void SetUnlockDependency(class UFGAvailabilityDependency* Dependency) { mUnlockDependencies.Reset(); if (Dependency) mUnlockDependencies.Add(Dependency); }"))


def research(s):
    s = s.replace(must(s, '#include "UObject/UnrealType.h"'),
                  '#include "UObject/UnrealType.h"\n#include "AvailabilityDependencies/FGAvailabilityDependency.h"\n#include "AssetRegistry/AssetRegistryModule.h"\n#include "AssetRegistry/IAssetRegistry.h"\n#include "Modules/ModuleManager.h"')
    helper = r'''
	/** Loads a Blueprint class of the game by asset name (folders in the paks are not predictable). */
	UClass* LoadGameClass(const FString& AssetName)
	{
		static TMap<FString, TWeakObjectPtr<UClass>> Cache;
		if (const TWeakObjectPtr<UClass>* Found = Cache.Find(AssetName)) if (Found->IsValid()) return Found->Get();
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(FName(TEXT("/Game/FactoryGame")), Assets, true);
		for (const FAssetData& A : Assets)
		{
			if (A.AssetName.ToString() != AssetName) continue;
			const FString Path = A.PackageName.ToString() + TEXT(".") + AssetName + TEXT("_C");
			if (UClass* C = LoadClass<UObject>(nullptr, *Path)) { Cache.Add(AssetName, C); return C; }
		}
		UE_LOG(LogFRResearch, Warning, TEXT("Game class %s not found in the asset registry"), *AssetName);
		return nullptr;
	}

	/** A "schematic purchased" dependency on the starting recipes: always met, displayed by the MAM tooltip. */
	UFGAvailabilityDependency* MakeAlwaysMetDependency(UObject* Outer)
	{
		UClass* DepClass = LoadGameClass(TEXT("BP_SchematicPurchasedDependency"));
		UClass* Starting = LoadGameClass(TEXT("Schematic_StartingRecipes"));
		if (!DepClass || !Starting || DepClass->HasAnyClassFlags(CLASS_Abstract)) return nullptr;
		UFGAvailabilityDependency* Dep = NewObject<UFGAvailabilityDependency>(Outer, DepClass, MakeUniqueObjectName(Outer, DepClass, TEXT("AlwaysMet")));
		FArrayProperty* Arr = FindFProperty<FArrayProperty>(DepClass, TEXT("mSchematics"));
		FClassProperty* Inner = Arr ? CastField<FClassProperty>(Arr->Inner) : nullptr;
		if (!Inner) return nullptr;
		FScriptArrayHelper Helper(Arr, Arr->ContainerPtrToValuePtr<void>(Dep));
		const int32 Index = Helper.AddValue();
		Inner->SetObjectPropertyValue(Helper.GetRawPtr(Index), Starting);
		Root(Dep);
		return Dep;
	}
'''
    marker = "	/** Reflection map of the cooked BPD_ResearchTreeNode data"
    s = s.replace(must(s, marker), helper + "\n" + marker, 1)
    # log ChildrenAndRoads structure once (needed to draw links safely later)
    s = s.replace(must(s, "			return Schematic && Coordinates;"), '''			for (TFieldIterator<FProperty> F(Data->Struct); F; ++F)
				if (const FMapProperty* M = CastField<FMapProperty>(*F))
					UE_LOG(LogFRResearch, Display, TEXT("Node map %s: key=%s value=%s"), *F->GetAuthoredName(), *M->KeyProp->GetCPPType(), *M->ValueProp->GetCPPType());
			for (TFieldIterator<FProperty> F(Data->Struct); F; ++F)
				if (const FArrayProperty* A = CastField<FArrayProperty>(*F))
					UE_LOG(LogFRResearch, Display, TEXT("Node array %s: inner=%s"), *F->GetAuthoredName(), *A->Inner->GetCPPType());
			return Schematic && Coordinates;''')
    # every tree needs one valid unlock dependency, otherwise do not register any tree
    old = "	NodeClass->AddToRoot(); // never let the template class be unloaded while our nodes exist"
    new = old + '''
	if (!MakeAlwaysMetDependency(GetTransientPackage()))
	{
		UE_LOG(LogFRResearch, Error, TEXT("No valid unlock dependency for FICSIT trees: MAM trees disabled to avoid the MAM tooltip crash."));
		return;
	}'''
    s = s.replace(must(s, old), new)
    s = s.replace(must(s, "		Tree->Setup(Div.Id);\n		UFGResearchTree::SetNodes(TreeClass, TreeNodes);"),
                  "		Tree->Setup(Div.Id);\n		Tree->SetUnlockDependency(MakeAlwaysMetDependency(Tree));\n		UFGResearchTree::SetNodes(TreeClass, TreeNodes);")
    return s


rw("Private/FRResearch.cpp", research)
print("patched v11")
