"""Tech nodes gated by vanilla unlocks; craft interception at ingredient removal; generic class lookup."""
import os

ROOT = r"F:\FICSIT-Roles-Rebuild\Mod\FICSITRoles\Source\FICSITRoles"


def rw(rel, fn):
    p = os.path.join(ROOT, rel)
    s = open(p, encoding="utf-8").read()
    open(p, "w", encoding="utf-8").write(fn(s))


def must(s, a):
    assert a in s, a[:90]
    return a


# --- generic class lookup -------------------------------------------------------------------
rw("Public/FRConfig.h", lambda s: s.replace(must(s, "FICSITROLES_API UClass* FRFindItem(const FString& NameOrPath);"),
   "FICSITROLES_API UClass* FRFindItem(const FString& NameOrPath);\n/** Any loaded class by short name (no _C) deriving from Base. Cached. */\nFICSITROLES_API UClass* FRFindClass(const FString& ShortName, UClass* Base);"))


def cfg(s):
    return s.replace(must(s, "const FFRConfig& FFRConfig::Get()"), '''UClass* FRFindClass(const FString& ShortName, UClass* Base)
{
	static TMap<FString, TWeakObjectPtr<UClass>> Cache;
	if (ShortName.IsEmpty() || !Base) return nullptr;
	const FString Key = Base->GetName() + TEXT(":") + ShortName;
	if (const TWeakObjectPtr<UClass>* Found = Cache.Find(Key)) if (Found->IsValid()) return Found->Get();
	const FString Wanted = ShortName + TEXT("_C");
	for (TObjectIterator<UClass> It; It; ++It)
		if (It->GetName() == Wanted && It->IsChildOf(Base)) { Cache.Add(Key, *It); return *It; }
	return nullptr;
}

const FFRConfig& FFRConfig::Get()''', 1)


rw("Private/FRConfig.cpp", cfg)

# --- vanilla gating ---------------------------------------------------------------------------
rw("Public/FRRolesSubsystem.h", lambda s: s.replace(must(s, "	bool IsNodeOpen(FName Node) const;"),
   "	bool IsNodeOpen(FName Node) const;\n	/** Every building/recipe a node authorises is already unlocked by the vanilla progression. */\n	bool IsNodeVanillaReady(FName Node) const;"))


def sub(s):
    s = s.replace(must(s, '#include "FGRecipeManager.h"'), '#include "FGRecipeManager.h"')
    old = '''	for (const FName R : N->Requires) if (!IsResearched(R)) return false;
	return true;
}'''
    new = '''	for (const FName R : N->Requires) if (!IsResearched(R)) return false;
	return IsNodeVanillaReady(Node);
}

bool AFRRolesSubsystem::IsNodeVanillaReady(FName Node) const
{
	const FFRTechNode* N = FFRConfig::Get().FindNode(Node);
	const AFGRecipeManager* Recipes = AFGRecipeManager::Get(GetWorld());
	if (!N || !Recipes) return true;
	auto Ready = [Recipes](const FString& RecipeName) {
		UClass* Recipe = FRFindClass(RecipeName, UFGRecipe::StaticClass());
		return !Recipe || Recipes->IsRecipeAvailable(Recipe); // unknown recipe: never block
	};
	for (FString B : N->Grants.Of(EFRPerm::Build))
	{
		B.RemoveFromEnd(TEXT("*"));
		if (B.RemoveFromStart(TEXT("Build_")) && !Ready(TEXT("Recipe_") + B)) return false;
	}
	for (const FString& C : N->Grants.Of(EFRPerm::Craft))
		if (!C.EndsWith(TEXT("*")) && !Ready(C)) return false;
	return true;
}'''
    s = s.replace(must(s, old), new)
    old2 = '''	for (const FName R : Node->Requires)
	{
		if (!IsResearched(R)) { Notify(Controller, TEXT("deny.tech_requires"), R.ToString(), FString(), 2); return; }
	}'''
    s = s.replace(must(s, old2), old2 + '''
	if (!IsNodeVanillaReady(NodeId)) { Notify(Controller, TEXT("deny.tech_vanilla"), NodeId.ToString(), FString(), 2); return; }''')
    return s


rw("Private/FRRolesSubsystem.cpp", sub)

# --- craft interception -----------------------------------------------------------------------
def mod(s):
    old = '''		SUBSCRIBE_METHOD(UFGWorkBench::Produce, [](auto& Scope, UFGWorkBench* Bench, float Dt) {
			if (Bench && !FRHooks::CraftAllowed(Bench, Bench->GetCurrentRecipe(), true)) Scope.Cancel();
		});'''
    new = old + '''
		// Final step of every manual craft: no ingredient is taken and nothing is produced when refused.
		SUBSCRIBE_METHOD(UFGWorkBench::RemoveIngredientsAndAwardRewards, [](auto& Scope, UFGWorkBench* Bench, UFGInventoryComponent* Inventory, TSubclassOf<UFGRecipe> Recipe) {
			if (Bench && !FRHooks::CraftAllowed(Bench, Recipe, true)) Scope.Cancel();
		});'''
    s = s.replace(must(s, old), new)
    old3 = '''		AFGCharacterPlayer* User = const_cast<UFGWorkBench*>(Bench)->GetWorkBenchUser();
		APlayerController* Controller = User ? Cast<APlayerController>(User->GetController()) : nullptr;
		AFRRolesSubsystem* Roles = User ? AFRRolesSubsystem::Get(User) : nullptr;
		if (!Controller || !Roles) return true;'''
    new3 = '''		AFGCharacterPlayer* User = const_cast<UFGWorkBench*>(Bench)->GetWorkBenchUser();
		if (!User) User = Cast<AFGCharacterPlayer>(Bench->GetOwner()); // workbench component owned by the character
		APlayerController* Controller = User ? Cast<APlayerController>(User->GetController()) : nullptr;
		AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(Bench);
		if (bNotify)
		{
			static TSet<FString> Logged;
			const FString Key = FRShortClassName(Recipe);
			if (!Logged.Contains(Key)) { Logged.Add(Key); UE_LOG(LogFRHooks, Display, TEXT("Craft check %s user=%s controller=%s"), *Key, User ? *User->GetName() : TEXT("none"), Controller ? *Controller->GetName() : TEXT("none")); }
		}
		if (!Controller || !Roles) return true;'''
    s = s.replace(must(s, old3), new3)
    return s


rw("Private/FRModule.cpp", mod)

# --- UI: state text for nodes waiting on vanilla ----------------------------------------------
def ui(s):
    old = "				: StaticCastSharedRef<SWidget>(Label(FFRLoc::T(TEXT(\"tech.locked\")), 13, Muted, true));"
    new = "				: StaticCastSharedRef<SWidget>(Label(FFRLoc::T(Roles && !Roles->IsNodeVanillaReady(Id) ? TEXT(\"tech.vanilla\") : TEXT(\"tech.locked\")), 13, Muted, true, true));"
    return s.replace(must(s, old), new)


rw("Private/UI/FRScreens.cpp", ui)
print("patched v6")
