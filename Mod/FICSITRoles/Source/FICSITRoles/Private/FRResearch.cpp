#include "FRResearch.h"
#include "FRConfig.h"
#include "FRLoc.h"
#include "FRRolesSubsystem.h"
#include "UI/FRNames.h"
#include "UI/FRStyle.h"
#include "FGResearchTreeNode.h"
#include "FGUnlockSubsystem.h"
#include "FGSchematicManager.h"
#include "Unlocks/FGUnlockInfoOnly.h"
#include "Resources/FGItemDescriptor.h"
#include "Module/GameWorldModule.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "AvailabilityDependencies/FGAvailabilityDependency.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogFRResearch, Log, All);

namespace
{
#include "FRResearchPools.inl"

	TMap<FName, TSubclassOf<UFGSchematic>> GNodeSchematics;
	TArray<UClass*> GTrees;
	bool GMamActive = false;
	// Objects hung on native CDOs are not traced by the GC: root them (and the node template class)
	// or the MAM UI reads freed memory. This was the cause of the earlier MAM crashes.
	TArray<TWeakObjectPtr<UObject>> GRooted;
	void Root(UObject* Object) { if (Object && !Object->IsRooted()) { Object->AddToRoot(); GRooted.Add(Object); } }
	void UnrootAll() { for (const TWeakObjectPtr<UObject>& O : GRooted) if (O.IsValid()) O->RemoveFromRoot(); GRooted.Reset(); }

	bool IsIntPoint(const FProperty* P)
	{
		const FStructProperty* S = CastField<FStructProperty>(P);
		if (!S || !S->Struct || S->Struct->GetStructureSize() != sizeof(FIntPoint)) return false;
		const FString N = S->Struct->GetName();
		return N.StartsWith(TEXT("IntPoint")) || N.StartsWith(TEXT("IntVector2"));
	}


	/** Loads a Blueprint class of the game by asset name (folders in the paks are not predictable). */
	UClass* LoadGameClass(const FString& AssetName)
	{
		static TMap<FString, TWeakObjectPtr<UClass>> Cache;
		if (const TWeakObjectPtr<UClass>* Found = Cache.Find(AssetName)) if (Found->IsValid()) return Found->Get();
		const FString Wanted = AssetName + TEXT("_C");
		for (TObjectIterator<UClass> It; It; ++It)
			if (It->GetName() == Wanted) { Cache.Add(AssetName, *It); return *It; }
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(FName(TEXT("/Game/FactoryGame")), Assets, true);
		for (const FAssetData& A : Assets)
		{
			if (A.AssetName.ToString() != AssetName) continue;
			const FString Path = A.PackageName.ToString() + TEXT(".") + AssetName + TEXT("_C");
			if (UClass* C = LoadClass<UObject>(nullptr, *Path)) { Cache.Add(AssetName, C); return C; }
		}
		UE_LOG(LogFRResearch, Display, TEXT("Game class %s not loaded yet"), *AssetName);
		return nullptr;
	}

	/** A "schematic purchased" dependency on the starting recipes: always met, displayed by the MAM tooltip. */
	UFGAvailabilityDependency* MakeAlwaysMetDependency(UObject* Outer, EObjectFlags Flags = RF_NoFlags)
	{
		UClass* DepClass = LoadGameClass(TEXT("BP_SchematicPurchasedDependency"));
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
		}
		UFGAvailabilityDependency* Dep = NewObject<UFGAvailabilityDependency>(Outer, DepClass, MakeUniqueObjectName(Outer, DepClass, TEXT("AlwaysMet")), Flags);
		FArrayProperty* Arr = FindFProperty<FArrayProperty>(DepClass, TEXT("mSchematics"));
		FClassProperty* Inner = Arr ? CastField<FClassProperty>(Arr->Inner) : nullptr;
		if (!Inner) return nullptr;
		FScriptArrayHelper Helper(Arr, Arr->ContainerPtrToValuePtr<void>(Dep));
		const int32 Index = Helper.AddValue();
		Inner->SetObjectPropertyValue(Helper.GetRawPtr(Index), Starting);
		if (Flags == RF_NoFlags) Root(Dep);
		return Dep;
	}


	/** Concrete Blueprint class of the game's "info only" unlock, borrowed from a loaded vanilla schematic. */
	UClass* InfoOnlyUnlockClass()
	{
		static TWeakObjectPtr<UClass> Cached;
		if (Cached.IsValid()) return Cached.Get();
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (!It->IsChildOf(UFGSchematic::StaticClass()) || It->IsChildOf(UFRTechSchematic::StaticClass()) || It->HasAnyClassFlags(CLASS_Abstract)) continue;
			for (UFGUnlock* U : UFGSchematic::GetUnlocks(*It))
				if (U && U->IsA(UFGUnlockInfoOnly::StaticClass()) && !U->GetClass()->HasAnyClassFlags(CLASS_Abstract))
				{
					Cached = U->GetClass();
					return U->GetClass();
				}
		}
		return nullptr;
	}

	/** Reflection map of the cooked BPD_ResearchTreeNode data (authored names, GUID suffix stripped). */
	struct FNodeLayout
	{
		FStructProperty* Data = nullptr;
		FProperty* Schematic = nullptr;
		FStructProperty* Coordinates = nullptr;
		FArrayProperty* Parents = nullptr;
		FMapProperty* Roads = nullptr;
		FArrayProperty* RoadPoints = nullptr;

		bool Resolve(UClass* NodeClass)
		{
			Data = CastField<FStructProperty>(NodeClass->FindPropertyByName(TEXT("mNodeDataStruct")));
			if (!Data) return false;
			for (TFieldIterator<FProperty> F(Data->Struct); F; ++F)
			{
				const FString Name = F->GetAuthoredName();
				if (const FClassProperty* C = CastField<FClassProperty>(*F))
				{
					if (C->MetaClass && C->MetaClass->IsChildOf(UFGSchematic::StaticClass())) Schematic = *F;
				}
				else if (Name.StartsWith(TEXT("Coordinates")) && IsIntPoint(*F)) Coordinates = CastField<FStructProperty>(*F);
				else if (FArrayProperty* A = CastField<FArrayProperty>(*F))
				{
					if (Name.StartsWith(TEXT("Parents")) && IsIntPoint(A->Inner)) Parents = A;
				}
				else if (FMapProperty* M = CastField<FMapProperty>(*F))
				{
					const FStructProperty* V = CastField<FStructProperty>(M->ValueProp);
					if (Name.StartsWith(TEXT("ChildrenAndRoads")) && IsIntPoint(M->KeyProp) && V)
					{
						for (TFieldIterator<FArrayProperty> P(V->Struct); P; ++P)
							if (P->GetAuthoredName().StartsWith(TEXT("Points")) && IsIntPoint(P->Inner)) { Roads = M; RoadPoints = *P; }
					}
				}
			}
			for (TFieldIterator<FProperty> F(Data->Struct); F; ++F)
				if (const FMapProperty* M = CastField<FMapProperty>(*F))
					UE_LOG(LogFRResearch, Display, TEXT("Node map %s: key=%s value=%s"), *F->GetAuthoredName(), *M->KeyProp->GetCPPType(), *M->ValueProp->GetCPPType());
			for (TFieldIterator<FProperty> F(Data->Struct); F; ++F)
				if (const FArrayProperty* A = CastField<FArrayProperty>(*F))
					UE_LOG(LogFRResearch, Display, TEXT("Node array %s: inner=%s"), *F->GetAuthoredName(), *A->Inner->GetCPPType());
			return Schematic && Coordinates;
		}

		/** Child coordinate -> grid cells from parent to child (parent excluded, child included), like vanilla. */
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
		{
			void* DataPtr = Data->ContainerPtrToValuePtr<void>(Node);
			CastField<FClassProperty>(Schematic)->SetObjectPropertyValue_InContainer(DataPtr, SchematicClass);
			*reinterpret_cast<FIntPoint*>(Coordinates->ContainerPtrToValuePtr<void>(DataPtr)) = Coord;
			if (Parents)
			{
				FScriptArrayHelper Helper(Parents, Parents->ContainerPtrToValuePtr<void>(DataPtr));
				Helper.EmptyAndAddValues(InParents.Num());
				for (int32 i = 0; i < InParents.Num(); ++i) *reinterpret_cast<FIntPoint*>(Helper.GetRawPtr(i)) = InParents[i];
			}
		}
	};

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
			const int32 X = N->Col >= 0 ? N->Col : FMath::Clamp(3 - (InTier - 1) / 2 + I, 0, 6);
			Coords.Add(N->Id, FIntPoint(X, FMath::Clamp(N->Tier - 1, 0, 8))); // vanilla grid: X 0..6, Y 0..8
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
void UFRTechUnlock::Unlock(AFGUnlockSubsystem* UnlockSubsystem)
{
	Super::Unlock(UnlockSubsystem);
	if (UnlockSubsystem && UnlockSubsystem->HasAuthority())
		if (AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(UnlockSubsystem)) Roles->OnMamResearched(Node, true);
}

void UFRTechUnlock::Apply(AFGUnlockSubsystem* UnlockSubsystem)
{
	Super::Apply(UnlockSubsystem);
	if (UnlockSubsystem && UnlockSubsystem->HasAuthority())
		if (AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(UnlockSubsystem)) Roles->OnMamResearched(Node, false);
}

UFRTechSchematic::UFRTechSchematic()
{
	mType = ESchematicType::EST_MAM;
	mTechTier = 0;
	mTimeToComplete = 30.f;
	mIsPlayerSpecific = false;
	mHiddenUntilDependenciesMet = false;
	mDependenciesBlocksSchematicAccess = false;
	mDisplayName = FText::FromString(TEXT("FICSIT Roles"));
}

void UFRTechSchematic::Setup(FName InNode, FName InDivision, int32 Tier)
{
	Node = InNode;
	Division = InDivision;
	mTimeToComplete = 20.f + 25.f * Tier;
	mDisplayName = FText::FromName(InNode);
	mUnlocks.Reset(); // filled in Fill() with the game's own 'info only' unlock
	mCost.Reset();
	InfoReward = nullptr;
}

void UFRTechSchematic::Fill()
{
	const FFRTechNode* N = FFRConfig::Get().FindNode(Node);
	if (!N) return;
	mDisplayName = FRNames::Node(Node);
	FString Grants;
	for (int32 P = 0; P < (int32)EFRPerm::Num; ++P)
		for (const FString& T : N->Grants.Of((EFRPerm)P))
			Grants += TEXT("\n - ") + FRNames::Target((EFRPerm)P, T).ToString();
	mDescription = FText::Format(FText::FromString(TEXT("{0}{1}\n\n{2}")), FFRLoc::T(TEXT("mam.node_intro")), FText::FromString(Grants),
		FFRLoc::F(TEXT("mam.node_footer"), FFRLoc::Division(Division)));
	if (!GetClass()->HasAnyClassFlags(CLASS_Native)) return; // cooked asset: keep its own data
	mCost.Reset();
	for (const TPair<FString, int32>& C : N->Cost)
		if (UClass* Item = FRFindItem(C.Key)) { mCost.Add(FItemAmount(Item, C.Value)); Root(Item); } // CDO refs are not GC-traced
	mUnlocks.Reset();
	// No reward tile: both crashes while hovering inside a node came from the reward tooltip, which needs
	// internal unlock data we cannot provide. What the node authorises is listed in its description.
	InfoReward = nullptr;
	const FSlateBrush* Icon = FRNames::NodeIcon(Node);
	if (!Icon) Icon = FRStyle::DivisionIcon(Division);
	if (Icon)
	{
		mSchematicIcon = *Icon;
		mSmallSchematicIcon = Cast<UTexture2D>(Icon->GetResourceObject());
		Root(mSmallSchematicIcon); // referenced only from this (untraced) CDO
		if (InfoReward)
		{
			for (const TCHAR* Field : { TEXT("mUnlockIconBig"), TEXT("mUnlockIconSmall"), TEXT("mUnlockIconCategory") })
				if (FObjectPropertyBase* P = FindFProperty<FObjectPropertyBase>(InfoReward->GetClass(), Field))
					P->SetObjectPropertyValue_InContainer(InfoReward, mSmallSchematicIcon);
		}
	}
}

void UFRTechTree::Setup(FName InDivision)
{
	Division = InDivision;
	SetTexts();
	mUnlockDependencies.Reset();
	mVisibilityDependencies.Reset();
	if (const FSlateBrush* Icon = FRStyle::DivisionIcon(Division)) mResearchTreeIcon = *Icon;
}

void UFRTechTree::SetTexts()
{
	const FText Name = FFRLoc::F(TEXT("mam.tree_name"), FFRLoc::Division(Division));
	mPreUnlockDisplayName = Name;
	mDisplayName = Name;
	mPreUnlockDescription = FFRLoc::T(TEXT("mam.tree_desc"));
	mPostUnlockDescription = mPreUnlockDescription;
}

// ---------------------------------------------------------------------------------------------
void FRResearch::Register(UGameWorldModule* Module)
{
	// Empty every CDO reference first: after UnrootAll() nothing reachable from an (untraced) CDO may stay.
	for (UClass* C : SchematicPool()) CastChecked<UFRTechSchematic>(C->GetDefaultObject())->Clear();
	for (UClass* T : TreePool())
	{
		UFGResearchTree::SetNodes(T, TArray<UFGResearchTreeNode*>());
		CastChecked<UFRTechTree>(T->GetDefaultObject())->SetUnlockDependency(nullptr);
	}
	UnrootAll();
	GNodeSchematics.Reset();
	GTrees.Reset();
	GMamActive = false;
	const FFRConfig& Config = FFRConfig::Get();
	if (!Config.bMamTrees) { UE_LOG(LogFRResearch, Display, TEXT("MAM trees disabled in config: technologies use the F7 terminal.")); return; }

	// Cooked Blueprint trees shipped in the mod pak (generated by UFRAssetGen in the editor).
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

void FRResearch::FillContent()
{
	for (UClass* TreeClass : GTrees)
	{
		if (!TreeClass->HasAnyClassFlags(CLASS_Native) || UFGResearchTree::GetUnlockDependencies(TreeClass).Num() > 0) continue;
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

	for (const TPair<FName, TSubclassOf<UFGSchematic>>& P : GNodeSchematics)
		if (P.Value) CastChecked<UFRTechSchematic>(P.Value->GetDefaultObject())->Fill();
	bool bMissing = false;
	for (const TPair<FName, TSubclassOf<UFGSchematic>>& P : GNodeSchematics)
	{
		const FFRTechNode* N = FFRConfig::Get().FindNode(P.Key);
		if (N && P.Value && N->Cost.Num() > 0 && UFGSchematic::GetCost(P.Value).Num() < N->Cost.Num()) bMissing = true;
	}
	static int32 Retries = 0;
	if (bMissing && Retries < 6 && GWorld)
	{
		++Retries;
		FTimerHandle Handle;
		GWorld->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([] { FRResearch::FillContent(); }), 5.f, false);
		UE_LOG(LogFRResearch, Display, TEXT("Some MAM cost items not loaded yet, retry %d scheduled"), Retries);
	}
}

TSubclassOf<UFGSchematic> FRResearch::SchematicOf(FName Node)
{
	const TSubclassOf<UFGSchematic>* Found = GNodeSchematics.Find(Node);
	return Found ? *Found : TSubclassOf<UFGSchematic>();
}

FName FRResearch::NodeOf(TSubclassOf<UFGSchematic> Schematic)
{
	if (!Schematic || !Schematic->IsChildOf(UFRTechSchematic::StaticClass())) return NAME_None;
	return CastChecked<UFRTechSchematic>(Schematic->GetDefaultObject())->Node;
}

bool FRResearch::IsActive()
{
	return GMamActive;
}

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
