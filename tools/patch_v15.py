"""GC audit fixes: clear CDO refs before unrooting, reuse reward object, reset InfoReward, retry Fill, null checks."""
import os

SRC = r"F:\FICSIT-Roles-Rebuild\Mod\FICSITRoles\Source\FICSITRoles"


def rw(rel, fn):
    p = os.path.join(SRC, rel)
    s = open(p, encoding="utf-8").read()
    open(p, "w", encoding="utf-8").write(fn(s))


def must(s, a):
    assert a in s, a[:100]
    return a


rw("Public/FRResearch.h", lambda s: s.replace(must(s, "	UPROPERTY(Transient) TObjectPtr<UFGUnlock> InfoReward;"),
   "	UPROPERTY(Transient) TObjectPtr<UFGUnlock> InfoReward;\n	/** Drops every reference before the rooted objects are released (second world load). */\n	void Clear() { mCost.Reset(); mUnlocks.Reset(); mSchematicDependencies.Reset(); InfoReward = nullptr; }"))


def cpp(s):
    # 1) clear CDO references before unrooting
    s = s.replace(must(s, "	UnrootAll();\n	GNodeSchematics.Reset();\n	GTrees.Reset();"),
                  '''	// Empty every CDO reference first: after UnrootAll() nothing reachable from an (untraced) CDO may stay.
	for (UClass* C : SchematicPool()) CastChecked<UFRTechSchematic>(C->GetDefaultObject())->Clear();
	for (UClass* T : TreePool())
	{
		UFGResearchTree::SetNodes(T, TArray<UFGResearchTreeNode*>());
		CastChecked<UFRTechTree>(T->GetDefaultObject())->SetUnlockDependency(nullptr);
	}
	UnrootAll();
	GNodeSchematics.Reset();
	GTrees.Reset();''')
    # 2) Setup resets the reward
    s = s.replace(must(s, "	mUnlocks.Reset(); // filled in Fill() with the game's own 'info only' unlock"),
                  "	mUnlocks.Reset(); // filled in Fill() with the game's own 'info only' unlock\n	mCost.Reset();\n	InfoReward = nullptr;")
    # 3) reuse the reward object
    old = '''	mUnlocks.Reset();
	if (UClass* InfoClass = InfoOnlyUnlockClass())
	{
		UFGUnlockInfoOnly* Info = NewObject<UFGUnlockInfoOnly>(this, InfoClass, MakeUniqueObjectName(this, InfoClass, TEXT("Habilitation")));'''
    new = '''	mUnlocks.Reset();
	UFGUnlock* PreviousReward = InfoReward;
	InfoReward = nullptr;
	if (UClass* InfoClass = InfoOnlyUnlockClass())
	{
		UFGUnlockInfoOnly* Info = Cast<UFGUnlockInfoOnly>(PreviousReward);
		if (!Info || Info->GetClass() != InfoClass)
			Info = NewObject<UFGUnlockInfoOnly>(this, InfoClass, MakeUniqueObjectName(this, InfoClass, TEXT("Habilitation")));'''
    s = s.replace(must(s, old), new)
    # 4/5) retry + null check
    old = '''	for (const TPair<FName, TSubclassOf<UFGSchematic>>& P : GNodeSchematics)
	{
		const FFRTechNode* N = FFRConfig::Get().FindNode(P.Key);
		if (N && N->Cost.Num() > 0 && P.Value->GetDefaultObject<UFGSchematic>() && UFGSchematic::GetCost(P.Value).Num() == 0)
			UE_LOG(LogFRResearch, Warning, TEXT("Technology %s has no resolvable cost item"), *P.Key.ToString());
	}'''
    new = '''	bool bMissing = false;
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
	}'''
    s = s.replace(must(s, old), new)
    s = s.replace(must(s, '#include "Engine/Texture2D.h"'), '#include "Engine/Texture2D.h"\n#include "Engine/World.h"\n#include "TimerManager.h"', 1)
    return s


rw("Private/FRResearch.cpp", cpp)
print("patched v15")
