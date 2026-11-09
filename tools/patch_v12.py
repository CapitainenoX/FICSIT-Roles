"""Find the unlock dependency among loaded classes / vanilla trees (asset registry is stripped in Steam builds);
trees always registered, dependency attached as soon as available; dump a vanilla node to learn link format."""
import os

SRC = r"F:\FICSIT-Roles-Rebuild\Mod\FICSITRoles\Source\FICSITRoles"


def rw(rel, fn):
    p = os.path.join(SRC, rel)
    s = open(p, encoding="utf-8").read()
    open(p, "w", encoding="utf-8").write(fn(s))


def must(s, a):
    assert a in s, a[:100]
    return a


def research(s):
    # 1) LoadGameClass: loaded classes first
    old = '''		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();'''
    new = '''		const FString Wanted = AssetName + TEXT("_C");
		for (TObjectIterator<UClass> It; It; ++It)
			if (It->GetName() == Wanted) { Cache.Add(AssetName, *It); return *It; }
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();'''
    s = s.replace(must(s, old), new)
    s = s.replace(must(s, '		UE_LOG(LogFRResearch, Warning, TEXT("Game class %s not found in the asset registry"), *AssetName);'),
                  '		UE_LOG(LogFRResearch, Display, TEXT("Game class %s not loaded yet"), *AssetName);')
    s = s.replace(must(s, '#include "Modules/ModuleManager.h"'), '#include "Modules/ModuleManager.h"\n#include "UObject/UObjectIterator.h"', 1)
    # 2) dependency: fall back to duplicating a vanilla tree's "schematic purchased" dependency
    old = '''		UClass* DepClass = LoadGameClass(TEXT("BP_SchematicPurchasedDependency"));
		UClass* Starting = LoadGameClass(TEXT("Schematic_StartingRecipes"));
		if (!DepClass || !Starting || DepClass->HasAnyClassFlags(CLASS_Abstract)) return nullptr;'''
    new = '''		UClass* DepClass = LoadGameClass(TEXT("BP_SchematicPurchasedDependency"));
		UClass* Starting = LoadGameClass(TEXT("Schematic_StartingRecipes"));
		if (!DepClass)
		{
			// Borrow the class from a vanilla tree that already uses a "schematic purchased" dependency.
			for (TObjectIterator<UClass> It; It && !DepClass; ++It)
			{
				if (!It->IsChildOf(UFGResearchTree::StaticClass()) || It->IsChildOf(UFRTechTree::StaticClass()) || It->HasAnyClassFlags(CLASS_Abstract)) continue;
				for (UFGAvailabilityDependency* D : UFGResearchTree::GetUnlockDependencies(*It))
					if (D && D->GetClass()->GetName().Contains(TEXT("SchematicPurchased")) && !D->GetClass()->HasAnyClassFlags(CLASS_Abstract)) { DepClass = D->GetClass(); break; }
			}
		}
		if (!DepClass || !Starting || DepClass->HasAnyClassFlags(CLASS_Abstract))
		{
			UE_LOG(LogFRResearch, Display, TEXT("Unlock dependency not available yet (class=%d starting=%d)"), DepClass != nullptr, Starting != nullptr);
			return nullptr;
		}'''
    s = s.replace(must(s, old), new)
    # 3) never abort registration for a missing dependency; attach later
    old = '''	if (!MakeAlwaysMetDependency(GetTransientPackage()))
	{
		UE_LOG(LogFRResearch, Error, TEXT("No valid unlock dependency for FICSIT trees: MAM trees disabled to avoid the MAM tooltip crash."));
		return;
	}'''
    s = s.replace(must(s, old), "")
    s = s.replace(must(s, "		Tree->SetUnlockDependency(MakeAlwaysMetDependency(Tree));"),
                  "		Tree->SetUnlockDependency(MakeAlwaysMetDependency(Tree)); // may be null now: retried in FillContent\n		GTrees.Add(TreeClass);")
    s = s.replace(must(s, "	TMap<FName, TSubclassOf<UFGSchematic>> GNodeSchematics;"),
                  "	TMap<FName, TSubclassOf<UFGSchematic>> GNodeSchematics;\n	TArray<UClass*> GTrees;")
    s = s.replace(must(s, "	UnrootAll();\n	GNodeSchematics.Reset();"), "	UnrootAll();\n	GNodeSchematics.Reset();\n	GTrees.Reset();")
    # FillContent: attach missing dependencies + one-time dump of a vanilla node (link format)
    old = "void FRResearch::FillContent()\n{"
    new = '''void FRResearch::FillContent()
{
	for (UClass* TreeClass : GTrees)
	{
		if (UFGResearchTree::GetUnlockDependencies(TreeClass).Num() > 0) continue;
		UFRTechTree* Tree = CastChecked<UFRTechTree>(TreeClass->GetDefaultObject());
		UFGAvailabilityDependency* Dep = MakeAlwaysMetDependency(Tree);
		Tree->SetUnlockDependency(Dep);
		UE_LOG(LogFRResearch, Display, TEXT("Tree %s unlock dependency %s"), *TreeClass->GetName(), Dep ? TEXT("attached") : TEXT("STILL MISSING"));
	}
	static bool bDumped = false;
	if (!bDumped)
	{
		bDumped = true;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (!It->IsChildOf(UFGResearchTree::StaticClass()) || It->IsChildOf(UFRTechTree::StaticClass()) || It->HasAnyClassFlags(CLASS_Abstract)) continue;
			const TArray<UFGResearchTreeNode*> Nodes = UFGResearchTree::GetNodes(*It);
			UE_LOG(LogFRResearch, Display, TEXT("Vanilla tree %s: %d nodes, %d unlock deps"), *It->GetName(), Nodes.Num(), UFGResearchTree::GetUnlockDependencies(*It).Num());
			for (UFGAvailabilityDependency* D : UFGResearchTree::GetUnlockDependencies(*It))
				if (D) UE_LOG(LogFRResearch, Display, TEXT("  unlock dep class %s"), *D->GetClass()->GetPathName());
			int32 Shown = 0;
			for (UFGResearchTreeNode* N : Nodes)
			{
				if (!N || Shown >= 3) continue;
				FStructProperty* Data = CastField<FStructProperty>(N->GetClass()->FindPropertyByName(TEXT("mNodeDataStruct")));
				if (!Data) continue;
				FString Out;
				Data->ExportTextItem_Direct(Out, Data->ContainerPtrToValuePtr<void>(N), nullptr, nullptr, PPF_None);
				UE_LOG(LogFRResearch, Display, TEXT("  node: %s"), *Out.Left(1500));
				++Shown;
			}
		}
	}
'''
    s = s.replace(must(s, old), new, 1)
    return s


rw("Private/FRResearch.cpp", research)
print("patched v12")
