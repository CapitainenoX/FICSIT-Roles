// Module entry: native hooks (build + research enforcement), world module, disqualifiers, chat command.
#include "FRWorldModule.h"
#include "FRConfig.h"
#include "FRLoc.h"
#include "FRRemote.h"
#include "FRRolesSubsystem.h"
#include "UI/FRLocalSubsystem.h"
#include "FGGameMode.h"
#include "FGPlayerController.h"
#include "FGWorkBench.h"
#include "FRResearch.h"
#include "FGResearchManager.h"
#include "FGInputLibrary.h"
#include "UI/FRInput.h"
#include "FGResearchTree.h"
#include "FGResearchTreeNode.h"
#include "FGSchematic.h"
#include "Unlocks/FGUnlock.h"
#include "UI/FGInteractWidget.h"
#include "FGItemPickup.h"
#include "FGDropPod.h"
#include "FGCharacterPlayer.h"
#include "FGRecipe.h"
#include "FGInventoryComponent.h"
#include "Equipment/FGEquipment.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Command/CommandSender.h"
#include "Patching/NativeHookManager.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/Pawn.h"
#include "Internationalization/Internationalization.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogFRHooks, Log, All);

// =============================================================================================
// Disqualifiers
// =============================================================================================
UFRDisqualifier::UFRDisqualifier()
{
	mIsSoftDisqualifier = false;
}

void UFRDisqualifier::SetText(TSubclassOf<UFRDisqualifier> Class, const FText& Text)
{
	if (Class) Class->GetDefaultObject<UFRDisqualifier>()->mDisqfualifyingText = Text;
}

void UFRDisqualifier::RefreshTexts()
{
	const FFRConfig& Config = FFRConfig::Get();
	const TSubclassOf<UFRDisqualifier> Reserved[] = { UFRDisqReserved0::StaticClass(), UFRDisqReserved1::StaticClass(), UFRDisqReserved2::StaticClass(), UFRDisqReserved3::StaticClass(), UFRDisqReserved4::StaticClass() };
	for (int32 i = 0; i < 5; ++i)
	{
		const FName Div = Config.Divisions.IsValidIndex(i) ? Config.Divisions[i].Id : NAME_None;
		SetText(Reserved[i], FFRLoc::F(TEXT("disq.reserved"), FFRLoc::Division(Div)));
	}
	SetText(UFRDisqReservedOther::StaticClass(), FFRLoc::T(TEXT("disq.reserved_other")));
	SetText(UFRDisqNoDivision::StaticClass(), FFRLoc::F(TEXT("disq.no_division"), FRTerminalKeyName()));
	SetText(UFRDisqClearance::StaticClass(), FFRLoc::T(TEXT("disq.clearance")));
	SetText(UFRDisqSyncing::StaticClass(), FFRLoc::T(TEXT("disq.syncing")));
}

TSubclassOf<UFGConstructDisqualifier> UFRDisqualifier::ForDecision(const FFRDecision& Decision)
{
	if (Decision.ReasonKey == TEXT("deny.no_division")) return UFRDisqNoDivision::StaticClass();
	if (Decision.ReasonKey == TEXT("deny.tech")) return UFRDisqClearance::StaticClass();
	if (Decision.ReasonKey == TEXT("deny.syncing")) return UFRDisqSyncing::StaticClass();
	switch (FFRConfig::Get().DivisionIndex(Decision.Owner))
	{
	case 0: return UFRDisqReserved0::StaticClass();
	case 1: return UFRDisqReserved1::StaticClass();
	case 2: return UFRDisqReserved2::StaticClass();
	case 3: return UFRDisqReserved3::StaticClass();
	case 4: return UFRDisqReserved4::StaticClass();
	default: return UFRDisqReservedOther::StaticClass();
	}
}

// =============================================================================================
// Build enforcement
// =============================================================================================
namespace FRHooks
{
	/** Every building class a hologram would create: itself, its children, blueprint contents. */
	void CollectClasses(const AFGHologram* Hologram, TSet<const UClass*>& Out, TSet<const AFGHologram*>& Seen)
	{
		if (!Hologram || Seen.Contains(Hologram)) return;
		Seen.Add(Hologram);
		if (const UClass* BuildClass = Hologram->GetBuildClass()) Out.Add(BuildClass);
		if (const AFGBlueprintHologram* Blueprint = Cast<AFGBlueprintHologram>(Hologram))
		{
			// The buildables of a blueprint live in a protected map; read its keys via reflection.
			static FMapProperty* MapProp = FindFProperty<FMapProperty>(AFGBlueprintHologram::StaticClass(), TEXT("mBuildableToNewRoot"));
			if (MapProp)
			{
				if (const FObjectPropertyBase* KeyProp = CastField<FObjectPropertyBase>(MapProp->KeyProp))
				{
					FScriptMapHelper Helper(MapProp, MapProp->ContainerPtrToValuePtr<void>(Blueprint));
					for (FScriptMapHelper::FIterator It(Helper); It; ++It)
						if (const UObject* Buildable = KeyProp->GetObjectPropertyValue(Helper.GetKeyPtr(It)))
							Out.Add(Buildable->GetClass());
				}
			}
		}
		for (const AFGHologram* Child : Hologram->GetHologramChildren()) CollectClasses(Child, Out, Seen);
	}

	/** Player behind a hologram. The construction instigator is not always set on local holograms,
	 *  so fall back to the actor instigator, then to the owner chain (hologram -> build gun -> character). */
	APlayerController* InstigatorOf(const AFGHologram* Hologram)
	{
		if (!Hologram) return nullptr;
		const APawn* Pawn = Hologram->GetConstructionInstigator();
		if (!Pawn) Pawn = Hologram->GetInstigator();
		const AActor* Owner = Hologram->GetOwner();
		for (int32 Depth = 0; !Pawn && Owner && Depth < 6; ++Depth, Owner = Owner->GetOwner())
		{
			if (const APlayerController* PC = Cast<APlayerController>(Owner)) return const_cast<APlayerController*>(PC);
			if (const APawn* AsPawn = Cast<APawn>(Owner)) Pawn = AsPawn;
			else if (Owner->GetInstigator()) Pawn = Owner->GetInstigator();
		}
		APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		if (!PC)
		{
			static TSet<FString> Logged;
			const FString Name = FRShortClassName(Hologram->GetBuildClass());
			if (!Logged.Contains(Name)) { Logged.Add(Name); UE_LOG(LogFRHooks, Warning, TEXT("No player resolved for hologram %s (%s)"), *Name, *Hologram->GetName()); }
		}
		return PC;
	}

	FFRDecision Evaluate(const AFGHologram* Hologram, APlayerController*& OutController)
	{
		OutController = InstigatorOf(Hologram);
		if (!OutController) return FFRDecision::Allow(); // non-player holograms are out of scope
		const AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(Hologram);
		if (!Roles)
		{
			// Server without state cannot happen after init; a client simply waits for replication.
			return FFRDecision::Allow();
		}
		TSet<const UClass*> Classes;
		TSet<const AFGHologram*> Seen;
		CollectClasses(Hologram, Classes, Seen);
		for (const UClass* Class : Classes)
		{
			const FFRDecision D = Roles->EvaluateBuild(OutController, Class);
			if (!D.bAllowed) return D;
		}
		return FFRDecision::Allow();
	}

	void AfterValidate(AFGHologram* Hologram)
	{
		if (!Hologram || !Hologram->GetWorld() || !Hologram->GetWorld()->IsGameWorld()) return;
		APlayerController* Controller = nullptr;
		const FFRDecision Decision = Evaluate(Hologram, Controller);
		if (Decision.bAllowed) return;
		Hologram->AddConstructDisqualifier(UFRDisqualifier::ForDecision(Decision));
		{
			static TSet<FString> Logged;
			const FString Key = FString::Printf(TEXT("%s|%s|%d"), *FRShortClassName(Hologram->GetBuildClass()), *Decision.ReasonKey, Hologram->HasAuthority() ? 1 : 0);
			if (!Logged.Contains(Key)) { Logged.Add(Key); UE_LOG(LogFRHooks, Display, TEXT("Build refused: %s (%s) authority=%d"), *FRShortClassName(Hologram->GetBuildClass()), *Decision.ReasonKey, Hologram->HasAuthority() ? 1 : 0); }
		}
		if (Controller && Controller->IsLocalController())
		{
			if (AFRLocalSubsystem* Local = AFRLocalSubsystem::Get(Hologram)) Local->OnLocalDenied(Decision);
		}
		else if (Controller && Hologram->HasAuthority())
		{
			if (AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(Hologram)) Roles->NotifyDenied(Cast<AFGPlayerController>(Controller), Decision);
		}
	}

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
		if (!User) User = Cast<AFGCharacterPlayer>(Bench->GetOwner()); // workbench component owned by the character
		APlayerController* Controller = User ? Cast<APlayerController>(User->GetController()) : nullptr;
		AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(Bench);
		if (bNotify)
		{
			static TSet<FString> Logged;
			const FString Key = FRShortClassName(Recipe);
			if (!Logged.Contains(Key)) { Logged.Add(Key); UE_LOG(LogFRHooks, Display, TEXT("Craft check %s user=%s controller=%s"), *Key, User ? *User->GetName() : TEXT("none"), Controller ? *Controller->GetName() : TEXT("none")); }
		}
		if (!Controller || !Roles) return true;
		const FFRDecision D = Roles->EvaluatePerm(Controller, EFRPerm::Craft, FRShortClassName(Recipe));
		if (!D.bAllowed && bNotify) Feedback(Controller, D, User);
		return D.bAllowed;
	}
}

// =============================================================================================
// Module
// =============================================================================================
class FFICSITRolesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if !WITH_EDITOR // game code is only stubs in the editor: hooks cannot be installed there
		// Server RPC path: ValidatePlacementAndCost -> CanConstruct -> cost removal + construction.
		// The disqualifier is added after validation, so a refusal happens before any item is taken.
		SUBSCRIBE_METHOD_AFTER(AFGHologram::ValidatePlacementAndCost, [](AFGHologram* Hologram, UFGInventoryComponent*) {
			FRHooks::AfterValidate(Hologram);
		});
		// Defence in depth on the authority: even if a disqualifier was cleared, construction stays refused.
		SUBSCRIBE_METHOD(AFGHologram::CanConstruct, [](auto& Scope, const AFGHologram* Hologram) {
			if (!Hologram || !Hologram->HasAuthority() || !Hologram->GetWorld() || Hologram->GetWorld()->GetNetMode() == NM_Client) return;
			APlayerController* Controller = nullptr;
			if (!FRHooks::Evaluate(Hologram, Controller).bAllowed) Scope.Override(false);
		});
		// Manual crafting (craft bench / equipment workshop): reserved recipes are greyed out and refused.
		SUBSCRIBE_METHOD(UFGWorkBench::CanProduce, [](auto& Scope, const UFGWorkBench* Bench, TSubclassOf<UFGRecipe> Recipe, UFGInventoryComponent* Inventory) {
			if (!FRHooks::CraftAllowed(Bench, Recipe, false)) Scope.Override(false);
		});
		SUBSCRIBE_METHOD(UFGWorkBench::Produce, [](auto& Scope, UFGWorkBench* Bench, float Dt) {
			if (Bench && !FRHooks::CraftAllowed(Bench, Bench->GetCurrentRecipe(), true)) Scope.Cancel();
		});
		// Final step of every manual craft: no ingredient is taken and nothing is produced when refused.
		SUBSCRIBE_METHOD(UFGWorkBench::RemoveIngredientsAndAwardRewards, [](auto& Scope, UFGWorkBench* Bench, UFGInventoryComponent* Inventory, TSubclassOf<UFGRecipe> Recipe) {
			if (Bench && !FRHooks::CraftAllowed(Bench, Recipe, true)) Scope.Cancel();
		});
		// Collectibles (hard drives in drop pods, somersloops, mercer spheres, power slugs...).
		SUBSCRIBE_METHOD(AFGCharacterPlayer::PickUpItem, [](auto& Scope, AFGCharacterPlayer* Character, AFGItemPickup* Pickup) {
			if (!Pickup) return;
			const FString Target = FRShortClassName(Pickup->GetPickupItems().Item.GetItemClass());
			if (!Target.IsEmpty() && !FRHooks::Allowed(Character, EFRPerm::Collect, Target)) Scope.Cancel();
		});
		// Drop pods have no non-virtual open function: their hook is installed on the first game world
		// (see UFRWorldModule) so no game CDO is created while the main menu loads.
		// Equipment use.
		SUBSCRIBE_METHOD(AFGCharacterPlayer::EquipEquipment, [](auto& Scope, AFGCharacterPlayer* Character, AFGEquipment* Equipment) {
			if (Equipment && !FRHooks::Allowed(Character, EFRPerm::Equip, FRShortClassName(Equipment->GetClass()))) Scope.Cancel();
		});
		// Division technologies in the MAM: launched only by their division, after vanilla unlocks.
		SUBSCRIBE_METHOD(AFGResearchManager::InitiateResearch, [](auto& Scope, AFGResearchManager* Manager, AFGPlayerController* Controller, TSubclassOf<UFGSchematic> Schematic, TSubclassOf<UFGResearchTree> Tree) {
			const FName Node = FRResearch::NodeOf(Schematic);
			if (Node.IsNone() || !Manager || !Manager->HasAuthority()) return;
			AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(Manager);
			const FFRTechNode* Def = FFRConfig::Get().FindNode(Node);
			const FFRPlayerRecord* Record = Roles ? Roles->FindPlayer(Controller) : nullptr;
			if (!Roles || !Def) return;
			FString Key; FString Arg; // no top-level comma inside a hook macro
			if (!Record || Record->Division != Def->Division) { Key = TEXT("deny.tech_division"); Arg = Def->Division.ToString(); }
			else
			{
				for (const FName R : Def->Requires) if (!Roles->IsResearched(R)) { Key = TEXT("deny.tech_requires"); Arg = R.ToString(); break; }
			}
			if (Key.IsEmpty()) return;
			Scope.Cancel();
			Roles->NotifyDenied(Controller, FFRDecision::Deny(Key, Arg, Def->Division));
		});
		// The vanilla "fully researched" walk crashes on runtime-built trees: answer it ourselves for FICSIT trees.
		SUBSCRIBE_METHOD(AFGResearchManager::IsTreeFullyResearched, [](auto& Scope, UObject* WorldContext, TSubclassOf<UFGResearchTree> Tree, TSubclassOf<UFGSchematic> Ignore) {
			if (FRResearch::IsOurTree(Tree)) Scope.Override(FRResearch::IsTreeComplete(WorldContext, Tree, Ignore));
		});
		// Official Controls menu: append our runtime mapping context to the rebindable list.
		SUBSCRIBE_METHOD_AFTER(UFGInputLibrary::GetPlayerRebindableMappingContexts, [](TArray<UFGInputMappingContext*>& Out) {
			if (UFGInputMappingContext* Context = FRInput::Context()) Out.AddUnique(Context);
		});
#endif
		FInternationalization::Get().OnCultureChanged().AddLambda([] { FFRLoc::Reload(); UFRDisqualifier::RefreshTexts(); });
		UE_LOG(LogFRHooks, Display, TEXT("FICSIT Roles 1.0.0 loaded: build/research enforcement hooks installed"));
		// Layout check: header size vs size registered by the game DLL. A mismatch means native
		// subclassing of that class corrupts memory (decides how MAM content must be created).
		auto Check = [](const TCHAR* Name, int32 Header, UClass* Class) {
			UE_LOG(LogFRHooks, Display, TEXT("Layout %s header=%d runtime=%d %s"), Name, Header, Class->GetPropertiesSize(), Header == Class->GetPropertiesSize() ? TEXT("OK") : TEXT("MISMATCH"));
		};
		Check(TEXT("UFGResearchTree"), sizeof(UFGResearchTree), UFGResearchTree::StaticClass());
		Check(TEXT("UFGResearchTreeNode"), sizeof(UFGResearchTreeNode), UFGResearchTreeNode::StaticClass());
		Check(TEXT("UFGSchematic"), sizeof(UFGSchematic), UFGSchematic::StaticClass());
		Check(TEXT("UFGUnlock"), sizeof(UFGUnlock), UFGUnlock::StaticClass());
		Check(TEXT("UFGConstructDisqualifier"), sizeof(UFGConstructDisqualifier), UFGConstructDisqualifier::StaticClass());
		Check(TEXT("UFGInteractWidget"), sizeof(UFGInteractWidget), UFGInteractWidget::StaticClass());
	}
};

IMPLEMENT_MODULE(FFICSITRolesModule, FICSITRoles)

// =============================================================================================
// World module
// =============================================================================================
UFRWorldModule::UFRWorldModule()
{
	bRootModule = true;
	ModSubsystems.Add(AFRRolesSubsystem::StaticClass());
	ModSubsystems.Add(AFRLocalSubsystem::StaticClass());
	mChatCommands.Add(AFRChatCommand::StaticClass());
}

void UFRWorldModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	if (Phase == ELifecyclePhase::INITIALIZATION)
	{
		static bool bDropPodHooked = false;
		if (!bDropPodHooked && !GIsEditor)
		{
			bDropPodHooked = true;
			SUBSCRIBE_METHOD_VIRTUAL(AFGDropPod::OnUse_Implementation, GetMutableDefault<AFGDropPod>(), [](auto& Scope, AFGDropPod* Pod, AFGCharacterPlayer* By, const FUseState& State) {
				if (Pod && !Pod->HasBeenOpened() && !FRHooks::Allowed(By, EFRPerm::Collect, TEXT("DropPod"))) Scope.Cancel();
			});
		}
		FFRLoc::Reload();
		UFRDisqualifier::RefreshTexts();
		FRResearch::Register(this); // before Super: SML registers schematics/trees during this phase
	}
	Super::DispatchLifecycleEvent(Phase);
	if (Phase == ELifecyclePhase::INITIALIZATION)
	{
		UWorld* World = GetWorld();
		if (AFGGameMode* GameMode = World ? World->GetAuthGameMode<AFGGameMode>() : nullptr)
		{
			GameMode->RegisterRemoteCallObjectClass(UFRRemote::StaticClass());
			// Controllers that logged in before this point (listen host) get their RCO now.
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				if (AFGPlayerController* PC = Cast<AFGPlayerController>(It->Get()))
					if (!PC->GetRemoteCallObjectOfClass(UFRRemote::StaticClass())) PC->RegisterRemoteCallObjectClass(UFRRemote::StaticClass());
		}
	}
}

// =============================================================================================
// /roles chat command (fallback administration, also usable from a dedicated server console)
// =============================================================================================
AFRChatCommand::AFRChatCommand()
{
	CommandName = TEXT("roles");
	MinNumberOfArguments = 0;
	bOnlyUsableByPlayer = false;
	Usage = FText::FromString(TEXT("/roles [status|start]"));
}

EExecutionStatus AFRChatCommand::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Args, const FString& Label)
{
	AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(this);
	if (!Roles || !Sender) return EExecutionStatus::UNCOMPLETED;
	AFGPlayerController* Player = Sender->IsPlayerSender() ? Sender->GetPlayer() : nullptr;
	const FString Verb = Args.Num() > 0 ? Args[0].ToLower() : FString(TEXT("status"));

	auto FindByName = [Roles](const FString& Name) -> const FFRPlayerRecord* {
		return Roles->Players.FindByPredicate([&Name](const FFRPlayerRecord& P) { return P.Name.Equals(Name, ESearchCase::IgnoreCase) || P.Id == Name; });
	};

	if (Verb == TEXT("status"))
	{
		Sender->SendChatMessage(FFRLoc::F(TEXT("cmd.status"), FText::AsNumber(Roles->ExpectedPlayers), FText::AsNumber(Roles->Players.Num())).ToString());
		for (const FFRPlayerRecord& P : Roles->Players)
			Sender->SendChatMessage(FString::Printf(TEXT("  %s%s - %s"), *P.Name, P.bOnline ? TEXT("") : TEXT(" (offline)"), *FFRLoc::Division(P.Division).ToString()));
		Sender->SendChatMessage(FFRLoc::F(TEXT("cmd.terminal_hint"), FRTerminalKeyName()).ToString());
		return EExecutionStatus::COMPLETED;
	}

	if (Verb == TEXT("start") && Player && Roles->IsAdmin(Player))
	{
		Roles->ServerStart(Player); // emergency: start even if an expected player never arrives
		return EExecutionStatus::COMPLETED;
	}
	Sender->SendChatMessage(Usage.ToString());
	return EExecutionStatus::BAD_ARGUMENTS;
}
