"""MAM rewards use the game's Blueprint 'info only' unlock (custom UFGUnlock crashed the reward tooltip);
completion detected through AFGSchematicManager::PurchasedSchematicDelegate + load sync."""
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
    s = s.replace(must(s, '#include "FGSchematicManager.h"'), '#include "FGSchematicManager.h"\n#include "Unlocks/FGUnlockInfoOnly.h"', 1)
    # Setup: no custom unlock object any more
    old = '''	mUnlocks.Reset();
	UFRTechUnlock* Unlock = NewObject<UFRTechUnlock>(this, MakeUniqueObjectName(this, UFRTechUnlock::StaticClass(), TEXT("TechUnlock")));
	Unlock->Node = InNode;
	mUnlocks.Add(Unlock);
	Root(Unlock);'''
    s = s.replace(must(s, old), "	mUnlocks.Reset(); // filled in Fill() with the game's own 'info only' unlock")
    helper = r'''
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
'''
    marker = "	/** Reflection map of the cooked BPD_ResearchTreeNode data"
    s = s.replace(must(s, marker), helper + "\n" + marker, 1)
    # Fill: add the info-only reward
    old = '''	const FSlateBrush* Icon = FRNames::NodeIcon(Node);'''
    new = '''	mUnlocks.Reset();
	if (UClass* InfoClass = InfoOnlyUnlockClass())
	{
		UFGUnlockInfoOnly* Info = NewObject<UFGUnlockInfoOnly>(this, InfoClass, MakeUniqueObjectName(this, InfoClass, TEXT("Habilitation")));
		auto SetText = [Info](const TCHAR* Field, const FText& Value) {
			if (FTextProperty* P = FindFProperty<FTextProperty>(Info->GetClass(), Field)) P->SetPropertyValue_InContainer(Info, Value);
		};
		SetText(TEXT("mUnlockName"), FFRLoc::F(TEXT("mam.reward"), FFRLoc::Division(Division)));
		SetText(TEXT("mUnlockDescription"), FText::FromString(Grants.TrimStart()));
		Root(Info);
		mUnlocks.Add(Info);
		InfoReward = Info;
	}
	const FSlateBrush* Icon = FRNames::NodeIcon(Node);'''
    s = s.replace(must(s, old), new)
    old = '''		mSchematicIcon = *Icon;
		mSmallSchematicIcon = Cast<UTexture2D>(Icon->GetResourceObject());'''
    new = old + '''
		if (InfoReward)
		{
			for (const TCHAR* Field : { TEXT("mUnlockIconBig"), TEXT("mUnlockIconSmall"), TEXT("mUnlockIconCategory") })
				if (FObjectPropertyBase* P = FindFProperty<FObjectPropertyBase>(InfoReward->GetClass(), Field))
					P->SetObjectPropertyValue_InContainer(InfoReward, mSmallSchematicIcon);
		}'''
    s = s.replace(must(s, old), new)
    return s


rw("Private/FRResearch.cpp", research)
rw("Public/FRResearch.h", lambda s: s.replace(must(s, "	FName Node;\n	FName Division;\n	void Setup(FName InNode"),
   "	FName Node;\n	FName Division;\n	UPROPERTY(Transient) TObjectPtr<UFGUnlock> InfoReward;\n	void Setup(FName InNode"))


def subh(s):
    s = s.replace(must(s, "	void OnMamResearched(FName Node, bool bNotify);"),
                  "	void OnMamResearched(FName Node, bool bNotify);\n	/** Marks every already-purchased FICSIT MAM schematic as researched (save load). */\n	void SyncMamResearch();")
    s = s.replace(must(s, "	UFUNCTION() void OnRep_State();"),
                  "	UFUNCTION() void OnRep_State();\n	UFUNCTION() void OnSchematicPurchased(TSubclassOf<UFGSchematic> Schematic);")
    return s


rw("Public/FRRolesSubsystem.h", subh)


def subc(s):
    s = s.replace(must(s, "FTimerDelegate::CreateWeakLambda(this, [this] { GrantStartRecipes(); FRResearch::FillContent(); })"),
                  '''FTimerDelegate::CreateWeakLambda(this, [this] {
		GrantStartRecipes();
		FRResearch::FillContent();
		if (AFGSchematicManager* Schematics = AFGSchematicManager::Get(GetWorld()))
			Schematics->PurchasedSchematicDelegate.AddUniqueDynamic(this, &AFRRolesSubsystem::OnSchematicPurchased);
		SyncMamResearch();
	})''')
    s = s.replace(must(s, "void AFRRolesSubsystem::GiveStartKit("), '''void AFRRolesSubsystem::OnSchematicPurchased(TSubclassOf<UFGSchematic> Schematic)
{
	const FName Node = FRResearch::NodeOf(Schematic);
	if (!Node.IsNone()) OnMamResearched(Node, true);
}

void AFRRolesSubsystem::SyncMamResearch()
{
	const AFGSchematicManager* Schematics = AFGSchematicManager::Get(GetWorld());
	if (!Schematics) return;
	for (const FFRTechNode& N : FFRConfig::Get().Tech)
	{
		const TSubclassOf<UFGSchematic> S = FRResearch::SchematicOf(N.Id);
		if (S && Schematics->IsSchematicPurchased(S)) OnMamResearched(N.Id, false);
	}
}

void AFRRolesSubsystem::GiveStartKit(''', 1)
    return s


rw("Private/FRRolesSubsystem.cpp", subc)
print("patched v14")
