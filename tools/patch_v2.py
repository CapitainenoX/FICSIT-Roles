"""One-shot source patch: permission families + division tech tree + start kits (replaces MAM trees)."""
import os

ROOT = r"F:\FICSIT-Roles-Rebuild\Mod\FICSITRoles\Source\FICSITRoles"


def rw(rel, fn):
    p = os.path.join(ROOT, rel)
    s = open(p, encoding="utf-8").read()
    s2 = fn(s)
    open(p, "w", encoding="utf-8").write(s2)


def must(s, a):
    assert a in s, a[:80]
    return a


for f in ("Private/FRResearch.cpp", "Public/FRResearch.h"):
    p = os.path.join(ROOT, f)
    if os.path.exists(p):
        os.remove(p)

rw("Public/FRTypes.h", lambda s: s.replace(must(s, "	UPROPERTY(SaveGame) int64 JoinedUtc = 0;"),
   "	UPROPERTY(SaveGame) int64 JoinedUtc = 0;\n	UPROPERTY(SaveGame) bool bKitGiven = false;"))


def header(s):
    s = s.replace(must(s, '#include "FRTypes.h"'), '#include "FRTypes.h"\n#include "FRConfig.h"')
    s = s.replace(must(s, "	UPROPERTY(SaveGame) int32 NextId = 1;"),
                  "	/** Division tech nodes researched in this world. */\n	UPROPERTY(ReplicatedUsing = OnRep_State, SaveGame) TArray<FName> Researched;\n	UPROPERTY(SaveGame) int32 NextId = 1;")
    s = s.replace(must(s, "	bool IsClearanceResearched(FName Clearance) const;"),
                  "	bool IsResearched(FName Node) const { return Researched.Contains(Node); }\n	/** Node researchable now (requirements met, not researched yet). */\n	bool IsNodeOpen(FName Node) const;")
    s = s.replace(must(s, "	FFRDecision EvaluateBuild(const APlayerController* Controller, const UClass* BuildClass) const;"),
                  "	FFRDecision EvaluateBuild(const APlayerController* Controller, const UClass* BuildClass) const;\n	/** Central decision for any restricted action family. */\n	FFRDecision EvaluatePerm(const APlayerController* Controller, EFRPerm Perm, const FString& Target) const;")
    s = s.replace(must(s, "	void NotifyClearanceResearched(FName Clearance);"),
                  "	void ServerResearch(AFGPlayerController* Controller, FName Node);")
    s = s.replace(must(s, "	void GrantStartRecipes();"),
                  "	void GrantStartRecipes();\n	void GiveStartKit(APlayerController* Controller, FFRPlayerRecord& Record);")
    return s


rw("Public/FRRolesSubsystem.h", header)

EVAL = r'''FFRDecision AFRRolesSubsystem::EvaluateBuild(const APlayerController* Controller, const UClass* BuildClass) const
{
	return BuildClass ? EvaluatePerm(Controller, EFRPerm::Build, FRShortClassName(BuildClass)) : FFRDecision::Allow();
}

FFRDecision AFRRolesSubsystem::EvaluatePerm(const APlayerController* Controller, EFRPerm Perm, const FString& Target) const
{
	const FFRPermRule* Rule = FFRConfig::Get().RuleFor(Perm, Target);
	if (!Rule) return FFRDecision::Allow(); // common target (vanilla shared, other mods, updates)

	const FName Owner = Rule->BaseOwners.Num() > 0 ? Rule->BaseOwners[0] : Rule->NodeGrants[0].Key;
	const FFRPlayerRecord* Record = FindPlayer(Controller);
	if (!Record) return FFRDecision::Deny(TEXT("deny.syncing"), FString(), Owner);
	const FName Div = Record->Division;

	if (!Div.IsNone() && Rule->BaseOwners.Contains(Div)) return FFRDecision::Allow();
	FName Pending;
	for (const TPair<FName, FName>& G : Rule->NodeGrants)
	{
		if (Div.IsNone() || G.Key != Div) continue;
		if (IsResearched(G.Value)) return FFRDecision::Allow();
		if (Pending.IsNone()) Pending = G.Value;
	}
	if (!Pending.IsNone()) return FFRDecision::Deny(TEXT("deny.tech"), Pending.ToString(), Div);

	TArray<FName> Owners = Rule->BaseOwners;
	for (const TPair<FName, FName>& G : Rule->NodeGrants) Owners.AddUnique(G.Key);
	for (const FName O : Owners)
	{
		if (FindActiveGrant(Record->Id, EFRAction::Build, O, Target) || (Coverage != EFRCoverage::Off && IsCovered(O)))
		{
			FFRDecision D; D.bViaException = true; D.Owner = O; return D;
		}
	}
	if (Div.IsNone()) return FFRDecision::Deny(TEXT("deny.no_division"), FString(), Owner);
	FString List;
	for (const FName O : Owners) List += (List.IsEmpty() ? TEXT("") : TEXT(",")) + O.ToString();
	static const TCHAR* Keys[] = { TEXT("deny.reserved"), TEXT("deny.craft"), TEXT("deny.collect"), TEXT("deny.equip") };
	return FFRDecision::Deny(Keys[(int32)Perm], List, Owner);
}

'''

RESEARCH = r'''void AFRRolesSubsystem::ServerResearch(AFGPlayerController* Controller, FName NodeId)
{
	if (!HasAuthority() || !Controller) return;
	FFRPlayerRecord* Record = EnsureRecord(Controller);
	const FFRTechNode* Node = FFRConfig::Get().FindNode(NodeId);
	if (!Record || !Node || IsResearched(NodeId)) return; // idempotent
	if (Record->Division != Node->Division) { Notify(Controller, TEXT("deny.tech_division"), Node->Division.ToString(), FString(), 2); return; }
	for (const FName R : Node->Requires)
	{
		if (!IsResearched(R)) { Notify(Controller, TEXT("deny.tech_requires"), R.ToString(), FString(), 2); return; }
	}
	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(Controller->GetPawn());
	UFGInventoryComponent* Inventory = Character ? Character->GetInventory() : nullptr;
	if (!Inventory) { Notify(Controller, TEXT("deny.syncing"), FString(), FString(), 2); return; }
	TArray<TPair<UClass*, int32>> Costs;
	for (const TPair<FString, int32>& C : Node->Cost)
	{
		UClass* Item = FRFindItem(C.Key);
		if (!Item) { UE_LOG(LogFRRoles, Warning, TEXT("Tech %s: cost item %s unknown, ignored"), *NodeId.ToString(), *C.Key); continue; }
		if (!Inventory->HasItems(Item, C.Value)) { Notify(Controller, TEXT("deny.cost"), FRShortClassName(Item), FString::FromInt(C.Value), 2); return; }
		Costs.Emplace(Item, C.Value);
	}
	for (const TPair<UClass*, int32>& C : Costs) Inventory->Remove(C.Key, C.Value);
	Researched.Add(NodeId);
	AddLog(TEXT("log.tech"), Record->Name, NodeId.ToString());
	UE_LOG(LogFRRoles, Display, TEXT("Tech %s researched by %s"), *NodeId.ToString(), *Record->Name);
	MarkChanged();
	NotifyAll(TEXT("notify.tech"), NodeId.ToString(), Record->Name, 1, Node->Division);
}

void AFRRolesSubsystem::GiveStartKit(APlayerController* Controller, FFRPlayerRecord& Record)
{
	if (Record.bKitGiven || Record.Division.IsNone()) return;
	const FFRDivisionDef* Div = FFRConfig::Get().FindDivision(Record.Division);
	AFGCharacterPlayer* Character = Controller ? Cast<AFGCharacterPlayer>(Controller->GetPawn()) : nullptr;
	UFGInventoryComponent* Inventory = Character ? Character->GetInventory() : nullptr;
	if (!Div || !Inventory) return;
	Record.bKitGiven = true;
	for (const TPair<FString, int32>& K : Div->StartKit)
	{
		UClass* Item = FRFindItem(K.Key);
		if (!Item) continue;
		const int32 Added = Inventory->AddStack(FInventoryStack(K.Value, Item), true);
		UE_LOG(LogFRRoles, Display, TEXT("Start kit: %d/%d %s to %s"), Added, K.Value, *K.Key, *Record.Name);
	}
	Notify(Controller, TEXT("notify.kit"), Record.Division.ToString(), FString(), 1);
}

'''


def cpp(s):
    s = s.replace(must(s, '#include "FRResearch.h"\n'), '#include "FGCharacterPlayer.h"\n#include "FGInventoryComponent.h"\n#include "Resources/FGItemDescriptor.h"\n')
    s = s.replace(must(s, "	DOREPLIFETIME(AFRRolesSubsystem, Log);"), "	DOREPLIFETIME(AFRRolesSubsystem, Log);\n	DOREPLIFETIME(AFRRolesSubsystem, Researched);")
    a = s.index("bool AFRRolesSubsystem::IsClearanceResearched")
    b = s.index("bool AFRRolesSubsystem::IsGrantActive")
    s = s[:a] + '''bool AFRRolesSubsystem::IsNodeOpen(FName Node) const
{
	const FFRTechNode* N = FFRConfig::Get().FindNode(Node);
	if (!N || IsResearched(Node)) return false;
	for (const FName R : N->Requires) if (!IsResearched(R)) return false;
	return true;
}

''' + s[b:]
    a = s.index("FFRDecision AFRRolesSubsystem::EvaluateBuild")
    b = s.index("FFRDecision AFRRolesSubsystem::EvaluateDivisionAction")
    s = s[:a] + EVAL + s[b:]
    a = s.index("void AFRRolesSubsystem::NotifyClearanceResearched")
    b = s.index("// --- Login / housekeeping")
    s = s[:a] + RESEARCH + s[b:]
    old = '	UE_LOG(LogFRRoles, Display, TEXT("Division %s assigned to %s"), *Division.ToString(), *Record->Name);\n	GrantStartRecipes();'
    s = s.replace(must(s, old), old + "\n	GiveStartKit(Controller, *Record);")
    return s


rw("Private/FRRolesSubsystem.cpp", cpp)
print("patched")
