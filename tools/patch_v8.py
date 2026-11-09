"""MAM technologies wiring, user config file in Saved/, official keybinding (Enhanced Input + Controls menu)."""
import json
import os

ROOT = r"F:\FICSIT-Roles-Rebuild\Mod\FICSITRoles"
SRC = os.path.join(ROOT, "Source", "FICSITRoles")


def rw(rel, fn):
    p = os.path.join(SRC, rel)
    s = open(p, encoding="utf-8").read()
    open(p, "w", encoding="utf-8").write(fn(s))


def must(s, a):
    assert a in s, a[:100]
    return a


# --- default config: revision + MAM switch ----------------------------------------------------
cp = os.path.join(ROOT, "Resources", "Config", "FICSITRoles.json")
cfg = json.load(open(cp, encoding="utf-8"))
new = {"schema": cfg.get("schema", 2), "configRevision": 3,
       "_doc_user": "Edit the copy in %LOCALAPPDATA%\\FactoryGame\\Saved\\FICSITRoles\\FICSITRoles.json (created on first launch). A newer mod revision replaces it and keeps a .old backup.",
       "mamTrees": True}
new.update({k: v for k, v in cfg.items() if k not in new})
json.dump(new, open(cp, "w", encoding="utf-8"), ensure_ascii=False, indent=2)

# --- build deps -------------------------------------------------------------------------------
rw("FICSITRoles.Build.cs", lambda s: s.replace(must(s, '"InputCore", "Engine"'), '"InputCore", "EnhancedInput", "Engine"'))

# --- config loader ----------------------------------------------------------------------------
rw("Public/FRConfig.h", lambda s: s.replace(must(s, "	float DenyCooldown = 3.f;"),
   "	float DenyCooldown = 3.f;\n	bool bMamTrees = true;\n	int32 Revision = 0;\n	/** File actually loaded (user copy in Saved/ when available). */\n	FString LoadedPath;"))


def cfgcpp(s):
    old = '''	const FString Path = ResourcePath(TEXT("Config/FICSITRoles.json"));
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())'''
    new = '''	// The editable copy lives in Saved/FICSITRoles (survives mod updates). It is created from the shipped
	// default on first launch, and replaced (with a .old backup) when the mod ships a newer revision.
	const FString Default = ResourcePath(TEXT("Config/FICSITRoles.json"));
	const FString User = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FICSITRoles"), TEXT("FICSITRoles.json"));
	auto RevisionOf = [](const FString& File) -> int32 {
		FString Raw; TSharedPtr<FJsonObject> Obj; double R = -1;
		if (FFileHelper::LoadFileToString(Raw, *File) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Obj) && Obj.IsValid())
			Obj->TryGetNumberField(TEXT("configRevision"), R);
		return (int32)R;
	};
	IFileManager& Files = IFileManager::Get();
	const int32 DefaultRev = RevisionOf(Default);
	if (!Files.FileExists(*User))
	{
		Files.Copy(*User, *Default);
	}
	else if (RevisionOf(User) < DefaultRev)
	{
		Files.Copy(*(User + TEXT(".old")), *User);
		Files.Copy(*User, *Default);
		Diagnostics.Add(TEXT("User config replaced by a newer mod revision (previous kept as .old)"));
	}
	const FString Path = Files.FileExists(*User) ? User : Default;
	LoadedPath = Path;
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())'''
    s = s.replace(must(s, old), new)
    s = s.replace(must(s, '	Root->TryGetStringField(TEXT("terminalKey"), TerminalKey);'),
                  '	Root->TryGetStringField(TEXT("terminalKey"), TerminalKey);\n	Root->TryGetBoolField(TEXT("mamTrees"), bMamTrees);\n	{ double Rev = 0; if (Root->TryGetNumberField(TEXT("configRevision"), Rev)) Revision = (int32)Rev; }')
    s = s.replace(must(s, '	UE_LOG(LogFRConfig, Display, TEXT("Config loaded: %d divisions, %d tech nodes"), Divisions.Num(), Tech.Num());'),
                  '	UE_LOG(LogFRConfig, Display, TEXT("Config loaded from %s: %d divisions, %d tech nodes, MAM=%d"), *Path, Divisions.Num(), Tech.Num(), bMamTrees);')
    s = s.replace(must(s, '#include "Misc/Paths.h"'), '#include "Misc/Paths.h"\n#include "HAL/FileManager.h"')
    return s


rw("Private/FRConfig.cpp", cfgcpp)

# --- subsystem: MAM completion ----------------------------------------------------------------
rw("Public/FRRolesSubsystem.h", lambda s: s.replace(must(s, "	void ServerResearch(AFGPlayerController* Controller, FName Node);"),
   "	void ServerResearch(AFGPlayerController* Controller, FName Node);\n	/** A FICSIT MAM schematic completed (bNotify=false when re-applied on load). */\n	void OnMamResearched(FName Node, bool bNotify);"))


def sub(s):
    s = s.replace(must(s, '#include "FGCharacterPlayer.h"'), '#include "FGCharacterPlayer.h"\n#include "FRResearch.h"', 1)
    s = s.replace(must(s, "	FFRPlayerRecord* Record = EnsureRecord(Controller);\n	const FFRTechNode* Node = FFRConfig::Get().FindNode(NodeId);"),
                  "	if (FRResearch::IsActive()) { Notify(Controller, TEXT(\"tech.use_mam\"), FString(), FString(), 0); return; } // researched in the MAM\n	FFRPlayerRecord* Record = EnsureRecord(Controller);\n	const FFRTechNode* Node = FFRConfig::Get().FindNode(NodeId);")
    s = s.replace(must(s, "void AFRRolesSubsystem::GiveStartKit("), '''void AFRRolesSubsystem::OnMamResearched(FName NodeId, bool bNotify)
{
	if (!HasAuthority() || NodeId.IsNone() || IsResearched(NodeId)) return;
	const FFRTechNode* Node = FFRConfig::Get().FindNode(NodeId);
	Researched.Add(NodeId);
	if (bNotify)
	{
		AddLog(TEXT("log.tech"), FString(TEXT("MAM")), NodeId.ToString());
		if (Node) NotifyAll(TEXT("notify.tech"), NodeId.ToString(), FString(TEXT("MAM")), 1, Node->Division);
	}
	UE_LOG(LogFRRoles, Display, TEXT("MAM technology %s %s"), *NodeId.ToString(), bNotify ? TEXT("researched") : TEXT("restored"));
	MarkChanged();
}

void AFRRolesSubsystem::GiveStartKit(''', 1)
    s = s.replace(must(s, "FTimerDelegate::CreateWeakLambda(this, [this] { GrantStartRecipes(); })"),
                  "FTimerDelegate::CreateWeakLambda(this, [this] { GrantStartRecipes(); FRResearch::FillContent(); })")
    return s


rw("Private/FRRolesSubsystem.cpp", sub)

# --- module: register MAM + research hook + Controls-menu hook --------------------------------
def mod(s):
    s = s.replace(must(s, '#include "FGWorkBench.h"'), '#include "FGWorkBench.h"\n#include "FRResearch.h"\n#include "FGResearchManager.h"\n#include "FGInputLibrary.h"\n#include "UI/FRInput.h"', 1)
    hook = '''		// Division technologies in the MAM: launched only by their division, after vanilla unlocks.
		SUBSCRIBE_METHOD(AFGResearchManager::InitiateResearch, [](auto& Scope, AFGResearchManager* Manager, AFGPlayerController* Controller, TSubclassOf<UFGSchematic> Schematic, TSubclassOf<UFGResearchTree> Tree) {
			const FName Node = FRResearch::NodeOf(Schematic);
			if (Node.IsNone() || !Manager || !Manager->HasAuthority()) return;
			AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(Manager);
			const FFRTechNode* Def = FFRConfig::Get().FindNode(Node);
			const FFRPlayerRecord* Record = Roles ? Roles->FindPlayer(Controller) : nullptr;
			if (!Roles || !Def) return;
			FString Key, Arg;
			if (!Record || Record->Division != Def->Division) { Key = TEXT("deny.tech_division"); Arg = Def->Division.ToString(); }
			else
			{
				for (const FName R : Def->Requires) if (!Roles->IsResearched(R)) { Key = TEXT("deny.tech_requires"); Arg = R.ToString(); break; }
				if (Key.IsEmpty() && !Roles->IsNodeVanillaReady(Node)) { Key = TEXT("deny.tech_vanilla"); Arg = Node.ToString(); }
			}
			if (Key.IsEmpty()) return;
			Scope.Cancel();
			Roles->NotifyDenied(Controller, FFRDecision::Deny(Key, Arg, Def->Division));
		});
		// Official Controls menu: append our runtime mapping context to the rebindable list.
		SUBSCRIBE_METHOD_AFTER(UFGInputLibrary::GetPlayerRebindableMappingContexts, [](TArray<UFGInputMappingContext*>& Out) {
			if (UFGInputMappingContext* Context = FRInput::Context()) Out.AddUnique(Context);
		});
'''
    s = s.replace(must(s, "		FInternationalization::Get().OnCultureChanged()"), hook + "		FInternationalization::Get().OnCultureChanged()", 1)
    s = s.replace(must(s, "		FFRLoc::Reload();\n		UFRDisqualifier::RefreshTexts();"),
                  "		FFRLoc::Reload();\n		UFRDisqualifier::RefreshTexts();\n		FRResearch::Register(this); // before Super: SML registers schematics/trees during this phase")
    return s


rw("Private/FRModule.cpp", mod)
print("patched v8")
