"""MAM: override IsTreeFullyResearched for FICSIT trees + vanilla-format links (Parents + ChildrenAndRoads)."""
import os

SRC = r"F:\FICSIT-Roles-Rebuild\Mod\FICSITRoles\Source\FICSITRoles"


def rw(rel, fn):
    p = os.path.join(SRC, rel)
    s = open(p, encoding="utf-8").read()
    open(p, "w", encoding="utf-8").write(fn(s))


def must(s, a):
    assert a in s, a[:100]
    return a


rw("Public/FRResearch.h", lambda s: s.replace(must(s, "	FICSITROLES_API bool IsActive();"),
   "	FICSITROLES_API bool IsActive();\n	/** True for the FICSIT research trees created by this mod. */\n	FICSITROLES_API bool IsOurTree(UClass* Tree);\n	/** Our own 'fully researched' test (the vanilla one crashes on runtime-built trees). */\n	FICSITROLES_API bool IsTreeComplete(UObject* WorldContext, UClass* Tree, UClass* SchematicToIgnore);"))


def research(s):
    # layout: also resolve ChildrenAndRoads + its Points array
    s = s.replace(must(s, "		FArrayProperty* Parents = nullptr;\n"),
                  "		FArrayProperty* Parents = nullptr;\n		FMapProperty* Roads = nullptr;\n		FArrayProperty* RoadPoints = nullptr;\n")
    old = '''					if (Name.StartsWith(TEXT("Parents")) && IsIntPoint(A->Inner)) Parents = A;
				}'''
    new = '''					if (Name.StartsWith(TEXT("Parents")) && IsIntPoint(A->Inner)) Parents = A;
				}
				else if (FMapProperty* M = CastField<FMapProperty>(*F))
				{
					const FStructProperty* V = CastField<FStructProperty>(M->ValueProp);
					if (Name.StartsWith(TEXT("ChildrenAndRoads")) && IsIntPoint(M->KeyProp) && V)
					{
						for (TFieldIterator<FArrayProperty> P(V->Struct); P; ++P)
							if (P->GetAuthoredName().StartsWith(TEXT("Points")) && IsIntPoint(P->Inner)) { Roads = M; RoadPoints = *P; }
					}
				}'''
    s = s.replace(must(s, old), new)
    old = '''		void Write(UObject* Node, UClass* SchematicClass, FIntPoint Coord, const TArray<FIntPoint>& InParents) const
		{'''
    new = '''		/** Child coordinate -> grid cells from parent to child (parent excluded, child included), like vanilla. */
		void WriteRoads(UObject* Node, const TArray<TPair<FIntPoint, TArray<FIntPoint>>>& Children) const
		{
			if (!Roads || !RoadPoints) return;
			void* DataPtr = Data->ContainerPtrToValuePtr<void>(Node);
			FScriptMapHelper Map(Roads, Roads->ContainerPtrToValuePtr<void>(DataPtr));
			Map.EmptyValues();
			for (const TPair<FIntPoint, TArray<FIntPoint>>& C : Children)
			{
				const int32 Index = Map.AddDefaultValue_Invalid_NeedsRehash();
				*reinterpret_cast<FIntPoint*>(Map.GetKeyPtr(Index)) = C.Key;
				FScriptArrayHelper Points(RoadPoints, RoadPoints->ContainerPtrToValuePtr<void>(Map.GetValuePtr(Index)));
				Points.EmptyAndAddValues(C.Value.Num());
				for (int32 i = 0; i < C.Value.Num(); ++i) *reinterpret_cast<FIntPoint*>(Points.GetRawPtr(i)) = C.Value[i];
			}
			Map.Rehash();
		}

		void Write(UObject* Node, UClass* SchematicClass, FIntPoint Coord, const TArray<FIntPoint>& InParents) const
		{'''
    s = s.replace(must(s, old), new)
    # centred grid (vanilla trees are centred on column 3), one row per tier with a road row between
    old = '''		TMap<FName, FIntPoint> Coords;
		TMap<int32, int32> PerTier;
		for (const FFRTechNode* N : Nodes)
		{
			int32& Column = PerTier.FindOrAdd(N->Tier);
			Coords.Add(N->Id, FIntPoint(Column, (N->Tier - 1) * 2));
			Column += 1;
		}'''
    new = '''		TMap<FName, FIntPoint> Coords;
		TMap<int32, int32> TierCount, TierIndex;
		for (const FFRTechNode* N : Nodes) TierCount.FindOrAdd(N->Tier) += 1;
		for (const FFRTechNode* N : Nodes)
		{
			const int32 Count = TierCount[N->Tier];
			int32& I = TierIndex.FindOrAdd(N->Tier);
			const int32 X = FMath::Max(0, 3 - (Count - 1) / 2) + I; // centred on column 3
			Coords.Add(N->Id, FIntPoint(X, (N->Tier - 1) * 2));
			++I;
		}
		// Roads: horizontally along the parent row, then down to the child.
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
			}'''
    s = s.replace(must(s, old), new)
    old = '''			Layout.Write(TreeNode, SchematicClass, Coords[N->Id], TArray<FIntPoint>());'''
    new = '''			TArray<FIntPoint> Parents;
			for (const FName R : N->Requires) if (const FIntPoint* P = Coords.Find(R)) Parents.Add(*P);
			Layout.Write(TreeNode, SchematicClass, Coords[N->Id], Parents);
			if (const TArray<TPair<FIntPoint, TArray<FIntPoint>>>* Roads = ChildRoads.Find(N->Id)) Layout.WriteRoads(TreeNode, *Roads);'''
    s = s.replace(must(s, old), new)
    # stale variable from the old code path (Parents declared above Write call)
    s = s.replace("			TArray<FIntPoint> Parents;\n			for (const FName R : N->Requires) if (const FIntPoint* P = Coords.Find(R)) Parents.Add(*P);\n			// Parent links are NOT written", "			// Parent links are NOT written", 1)
    s += r'''
bool FRResearch::IsOurTree(UClass* Tree)
{
	return Tree && Tree->IsChildOf(UFRTechTree::StaticClass());
}

bool FRResearch::IsTreeComplete(UObject* WorldContext, UClass* Tree, UClass* SchematicToIgnore)
{
	const AFGSchematicManager* Manager = AFGSchematicManager::Get(WorldContext);
	if (!Manager || !Tree) return false;
	const UFRTechTree* Cdo = Tree->GetDefaultObject<UFRTechTree>();
	for (const TPair<FName, TSubclassOf<UFGSchematic>>& P : GNodeSchematics)
	{
		UFRTechSchematic* S = P.Value ? P.Value->GetDefaultObject<UFRTechSchematic>() : nullptr;
		const FFRTechNode* N = FFRConfig::Get().FindNode(P.Key);
		if (!S || !N || P.Value == SchematicToIgnore) continue;
		// node belongs to this tree when the tree was set up for the node's division
		if (Cdo && Cdo->Division == N->Division && !Manager->IsSchematicPurchased(P.Value)) return false;
	}
	return true;
}
'''
    s = s.replace(must(s, '#include "FGUnlockSubsystem.h"'), '#include "FGUnlockSubsystem.h"\n#include "FGSchematicManager.h"', 1)
    s = s.replace(must(s, "void UFRTechTree::Setup(FName Division)\n{"), "void UFRTechTree::Setup(FName InDivision)\n{\n	Division = InDivision;\n	const FName Division = InDivision;", 1)
    return s


rw("Private/FRResearch.cpp", research)
rw("Public/FRResearch.h", lambda s: s.replace(must(s, "	void Setup(FName Division);\n	void SetIcon"), "	FName Division;\n	void Setup(FName Division);\n	void SetIcon"))


def module(s):
    hook = '''		// The vanilla "fully researched" walk crashes on runtime-built trees: answer it ourselves for FICSIT trees.
		SUBSCRIBE_METHOD(AFGResearchManager::IsTreeFullyResearched, [](auto& Scope, UObject* WorldContext, TSubclassOf<UFGResearchTree> Tree, TSubclassOf<UFGSchematic> Ignore) {
			if (FRResearch::IsOurTree(Tree)) Scope.Override(FRResearch::IsTreeComplete(WorldContext, Tree, Ignore));
		});
'''
    return s.replace(must(s, "		// Official Controls menu:"), hook + "		// Official Controls menu:", 1)


rw("Private/FRModule.cpp", module)
print("patched v13")
