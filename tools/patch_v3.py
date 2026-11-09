"""Hooks for craft / collect / drop pods / equipment, research RPC, 5th disqualifier, MAM tree removal."""
import os

ROOT = r"F:\FICSIT-Roles-Rebuild\Mod\FICSITRoles\Source\FICSITRoles"


def rw(rel, fn):
    p = os.path.join(ROOT, rel)
    s = open(p, encoding="utf-8").read()
    open(p, "w", encoding="utf-8").write(fn(s))


def must(s, a):
    assert a in s, a[:90]
    return a


# --- RCO --------------------------------------------------------------------------------------
rw("Public/FRRemote.h", lambda s: s.replace(must(s, "	UFUNCTION(Server, Reliable) void Server_Message(FName ToDivision, const FString& Text);"),
   "	UFUNCTION(Server, Reliable) void Server_Message(FName ToDivision, const FString& Text);\n	UFUNCTION(Server, Reliable) void Server_Research(FName Node);"))
rw("Private/FRRemote.cpp", lambda s: s + '''
void UFRRemote::Server_Research_Implementation(FName Node)
{
	FR_TRACE("%s", *Node.ToString());
	if (AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(this)) Roles->ServerResearch(GetOwnerPlayerController(), Node);
}
''')

# --- Disqualifier for a 5th division -----------------------------------------------------------
rw("Public/FRWorldModule.h", lambda s: s.replace(must(s, "UCLASS() class FICSITROLES_API UFRDisqReservedOther"),
   "UCLASS() class FICSITROLES_API UFRDisqReserved4 : public UFRDisqualifier { GENERATED_BODY() };\nUCLASS() class FICSITROLES_API UFRDisqReservedOther"))

HOOKS = r'''		// Manual crafting (craft bench / equipment workshop): reserved recipes are greyed out and refused.
		SUBSCRIBE_METHOD(UFGWorkBench::CanProduce, [](auto& Scope, const UFGWorkBench* Bench, TSubclassOf<UFGRecipe> Recipe, UFGInventoryComponent* Inventory) {
			if (!FRHooks::CraftAllowed(Bench, Recipe, false)) Scope.Override(false);
		});
		SUBSCRIBE_METHOD(UFGWorkBench::Produce, [](auto& Scope, UFGWorkBench* Bench, float Dt) {
			if (Bench && !FRHooks::CraftAllowed(Bench, Bench->GetCurrentRecipe(), true)) Scope.Cancel();
		});
		// Collectibles (hard drives in drop pods, somersloops, mercer spheres, power slugs...).
		SUBSCRIBE_METHOD_VIRTUAL(AFGItemPickup::OnUse_Implementation, GetMutableDefault<AFGItemPickup>(), [](auto& Scope, AFGItemPickup* Pickup, AFGCharacterPlayer* By, const FUseState& State) {
			if (!Pickup) return;
			const FInventoryStack Stack = Pickup->GetPickupItems();
			const FString Target = FRShortClassName(Stack.Item.GetItemClass());
			if (!Target.IsEmpty() && !FRHooks::Allowed(By, EFRPerm::Collect, Target)) Scope.Cancel();
		});
		SUBSCRIBE_METHOD_VIRTUAL(AFGDropPod::OnUse_Implementation, GetMutableDefault<AFGDropPod>(), [](auto& Scope, AFGDropPod* Pod, AFGCharacterPlayer* By, const FUseState& State) {
			if (!FRHooks::Allowed(By, EFRPerm::Collect, TEXT("DropPod"))) Scope.Cancel();
		});
		// Equipment use.
		SUBSCRIBE_METHOD(AFGCharacterPlayer::EquipEquipment, [](auto& Scope, AFGCharacterPlayer* Character, AFGEquipment* Equipment) {
			if (Equipment && !FRHooks::Allowed(Character, EFRPerm::Equip, FRShortClassName(Equipment->GetClass()))) Scope.Cancel();
		});
'''

HELPERS = r'''
	/** Shared refusal feedback: local toast for the local player, throttled notification otherwise. */
	void Feedback(APlayerController* Controller, const FFRDecision& Decision, const UObject* Context)
	{
		if (!Controller || Decision.bAllowed) return;
		if (Controller->IsLocalController())
		{
			if (AFRLocalSubsystem* Local = AFRLocalSubsystem::Get(Context)) Local->OnLocalDenied(Decision);
		}
		else if (AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(Context))
		{
			if (Roles->HasAuthority()) Roles->NotifyDenied(Cast<AFGPlayerController>(Controller), Decision);
		}
	}

	bool Allowed(AFGCharacterPlayer* Character, EFRPerm Perm, const FString& Target)
	{
		APlayerController* Controller = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
		AFRRolesSubsystem* Roles = Character ? AFRRolesSubsystem::Get(Character) : nullptr;
		if (!Controller || !Roles) return true;
		const FFRDecision D = Roles->EvaluatePerm(Controller, Perm, Target);
		if (!D.bAllowed)
		{
			static TSet<FString> Logged;
			if (!Logged.Contains(Target)) { Logged.Add(Target); UE_LOG(LogFRHooks, Display, TEXT("Action refused: %s (%s)"), *Target, *D.ReasonKey); }
			Feedback(Controller, D, Character);
		}
		return D.bAllowed;
	}

	bool CraftAllowed(const UFGWorkBench* Bench, TSubclassOf<UFGRecipe> Recipe, bool bNotify)
	{
		if (!Bench || !Recipe) return true;
		AFGCharacterPlayer* User = const_cast<UFGWorkBench*>(Bench)->GetWorkBenchUser();
		APlayerController* Controller = User ? Cast<APlayerController>(User->GetController()) : nullptr;
		AFRRolesSubsystem* Roles = User ? AFRRolesSubsystem::Get(User) : nullptr;
		if (!Controller || !Roles) return true;
		const FFRDecision D = Roles->EvaluatePerm(Controller, EFRPerm::Craft, FRShortClassName(Recipe));
		if (!D.bAllowed && bNotify) Feedback(Controller, D, User);
		return D.bAllowed;
	}
'''


def module(s):
    s = s.replace(must(s, '#include "FRResearch.h"\n'), '')
    s = s.replace(must(s, '#include "FGResearchManager.h"'), '#include "FGWorkBench.h"\n#include "FGItemPickup.h"\n#include "FGDropPod.h"\n#include "FGCharacterPlayer.h"\n#include "FGRecipe.h"\n#include "FGInventoryComponent.h"\n#include "Equipment/FGEquipment.h"')
    a = s.index("		// MAM: division trees can only be launched")
    b = s.index("		FInternationalization::Get().OnCultureChanged()")
    s = s[:a] + HOOKS + s[b:]
    s = s.replace(must(s, "		FRResearch::Register(this); // must precede Super so SML registers the schematics/trees\n"), "")
    # helpers at the end of the FRHooks namespace (before the module class)
    marker = "// =============================================================================================\n// Module"
    i = s.index(marker)
    j = s.rindex("}", 0, i)  # closing brace of namespace FRHooks
    s = s[:j] + HELPERS + s[j:]
    # 5 divisions
    s = s.replace(must(s, "	const TSubclassOf<UFRDisqualifier> Reserved[] = { UFRDisqReserved0::StaticClass(), UFRDisqReserved1::StaticClass(), UFRDisqReserved2::StaticClass(), UFRDisqReserved3::StaticClass() };\n	for (int32 i = 0; i < 4; ++i)"),
                  "	const TSubclassOf<UFRDisqualifier> Reserved[] = { UFRDisqReserved0::StaticClass(), UFRDisqReserved1::StaticClass(), UFRDisqReserved2::StaticClass(), UFRDisqReserved3::StaticClass(), UFRDisqReserved4::StaticClass() };\n	for (int32 i = 0; i < 5; ++i)")
    s = s.replace(must(s, "	case 3: return UFRDisqReserved3::StaticClass();"), "	case 3: return UFRDisqReserved3::StaticClass();\n	case 4: return UFRDisqReserved4::StaticClass();")
    s = s.replace(must(s, '	if (Decision.ReasonKey == TEXT("deny.clearance")) return UFRDisqClearance::StaticClass();'),
                  '	if (Decision.ReasonKey == TEXT("deny.tech")) return UFRDisqClearance::StaticClass();')
    return s


rw("Private/FRModule.cpp", module)
print("patched v3")
