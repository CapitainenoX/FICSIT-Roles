#include "FRRolesSubsystem.h"
#include "FRConfig.h"
#include "FRLoc.h"
#include "FRRemote.h"
#include "FGCharacterPlayer.h"
#include "FRResearch.h"
#include "FGInventoryComponent.h"
#include "Resources/FGItemDescriptor.h"
#include "FGPlayerController.h"
#include "FGSchematicManager.h"
#include "FGChatManager.h"
#include "FGRecipeManager.h"
#include "FGRecipe.h"
#include "Subsystem/SubsystemActorManager.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogFRRoles, Log, All);

namespace
{
	constexpr int32 MaxRequests = 60;
	constexpr int32 MaxMessages = 40;
	constexpr int32 MaxLog = 80;
	constexpr int32 MaxOpenRequestsPerAuthor = 10;
}

AFRRolesSubsystem::AFRRolesSubsystem()
{
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer_Replicate;
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
	Coverage = static_cast<EFRCoverage>(FFRConfig::Get().DefaultCoverage);
}

AFRRolesSubsystem* AFRRolesSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	USubsystemActorManager* Manager = World ? World->GetSubsystem<USubsystemActorManager>() : nullptr;
	return Manager ? Manager->GetSubsystemActor<AFRRolesSubsystem>() : nullptr;
}

void AFRRolesSubsystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFRRolesSubsystem, SchemaVersion);
	DOREPLIFETIME(AFRRolesSubsystem, Phase);
	DOREPLIFETIME(AFRRolesSubsystem, ExpectedPlayers);
	DOREPLIFETIME(AFRRolesSubsystem, Coverage);
	DOREPLIFETIME(AFRRolesSubsystem, Players);
	DOREPLIFETIME(AFRRolesSubsystem, Grants);
	DOREPLIFETIME(AFRRolesSubsystem, Requests);
	DOREPLIFETIME(AFRRolesSubsystem, Messages);
	DOREPLIFETIME(AFRRolesSubsystem, Log);
	DOREPLIFETIME(AFRRolesSubsystem, Researched);
	DOREPLIFETIME(AFRRolesSubsystem, StateVersion);
	DOREPLIFETIME(AFRRolesSubsystem, ServerUtcAtChange);
}

void AFRRolesSubsystem::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) return;
	PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddUObject(this, &AFRRolesSubsystem::HandlePostLogin);
	LogoutHandle = FGameModeEvents::OnGameModeLogoutEvent().AddUObject(this, &AFRRolesSubsystem::HandleLogout);
	GetWorldTimerManager().SetTimer(HousekeepingTimer, this, &AFRRolesSubsystem::Housekeeping, 10.f, true, 2.f);
	// Loaded saves: re-apply division start recipes once the game managers exist.
	FTimerHandle Once;
	GetWorldTimerManager().SetTimer(Once, FTimerDelegate::CreateWeakLambda(this, [this] {
		GrantStartRecipes();
		FRResearch::FillContent();
		if (AFGSchematicManager* Schematics = AFGSchematicManager::Get(GetWorld()))
			Schematics->PurchasedSchematicDelegate.AddUniqueDynamic(this, &AFRRolesSubsystem::OnSchematicPurchased);
		SyncMamResearch();
	}), 3.f, false);
	UE_LOG(LogFRRoles, Display, TEXT("FICSIT Roles session state ready (phase=%d expected=%d players=%d)"), (int32)Phase, ExpectedPlayers, Players.Num());
}

void AFRRolesSubsystem::EndPlay(const EEndPlayReason::Type Reason)
{
	FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle);
	FGameModeEvents::OnGameModeLogoutEvent().Remove(LogoutHandle);
	GetWorldTimerManager().ClearTimer(HousekeepingTimer);
	OnStateChanged.Clear();
	Super::EndPlay(Reason);
}

void AFRRolesSubsystem::PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion)
{
	// Online flags are runtime facts, never trust the saved values.
	for (FFRPlayerRecord& P : Players) P.bOnline = false;
	ExpectedPlayers = FMath::Clamp(ExpectedPlayers, 1, FFRConfig::Get().MaxExpectedPlayers);
	// Forward compatibility: saves from any earlier mod version load into the current config.
	// 1) renamed ids (config "renamed") are migrated, 2) unknown ids are kept but ignored (a node that
	// comes back later is restored), 3) MAM purchases re-sync from the game's schematic manager.
	const FFRConfig& Cfg = FFRConfig::Get();
	constexpr int32 CurrentSchema = 2;
	if (SchemaVersion > CurrentSchema)
		UE_LOG(LogFRRoles, Warning, TEXT("Save written by a newer FICSIT Roles (schema %d > %d): loading what is known"), SchemaVersion, CurrentSchema);
	for (FFRPlayerRecord& P : Players) if (!P.Division.IsNone()) P.Division = Cfg.Migrate(P.Division);
	for (FFRGrant& G : Grants) G.Target = Cfg.Migrate(G.Target);
	TArray<FName> Migrated;
	for (const FName N : Researched) Migrated.AddUnique(Cfg.Migrate(N));
	Researched = MoveTemp(Migrated);
	SchemaVersion = FMath::Max(SchemaVersion, CurrentSchema);
	// Drop records pointing at divisions removed from the config: those players pick again.
	for (FFRPlayerRecord& P : Players)
		if (!P.Division.IsNone() && !FFRConfig::Get().FindDivision(P.Division))
		{
			UE_LOG(LogFRRoles, Warning, TEXT("Saved division '%s' of %s no longer exists; role cleared"), *P.Division.ToString(), *P.Name);
			P.Division = NAME_None;
		}
	MarkChanged();
}

void AFRRolesSubsystem::OnRep_State()
{
	OnStateChanged.Broadcast();
}

void AFRRolesSubsystem::GrantStartRecipes()
{
	if (!HasAuthority()) return;
	AFGRecipeManager* Recipes = AFGRecipeManager::Get(GetWorld());
	if (!Recipes) return;
	for (const FFRDivisionDef& D : FFRConfig::Get().Divisions)
	{
		if (D.StartRecipes.Num() == 0 || MembersOf(D.Id) == 0) continue;
		for (const FString& Path : D.StartRecipes)
		{
			UClass* Recipe = LoadClass<UFGRecipe>(nullptr, *Path);
			if (!Recipe) { UE_LOG(LogFRRoles, Warning, TEXT("Start recipe not found: %s"), *Path); continue; }
			if (Recipes->IsRecipeAvailable(Recipe)) continue;
			Recipes->AddAvailableRecipe(Recipe);
			UE_LOG(LogFRRoles, Display, TEXT("Start recipe %s unlocked for division %s"), *Recipe->GetName(), *D.Id.ToString());
		}
	}
}

void AFRRolesSubsystem::MarkChanged()
{
	++StateVersion;
	ServerUtcAtChange = NowUtc();
	ForceNetUpdate();
	OnStateChanged.Broadcast(); // listen host has no OnRep
}

int64 AFRRolesSubsystem::NowUtc() const
{
	return FDateTime::UtcNow().ToUnixTimestamp();
}

void AFRRolesSubsystem::AddLog(const FString& Key, const FString& A, const FString& B)
{
	FFRLogEntry& E = Log.AddDefaulted_GetRef();
	E.Utc = NowUtc(); E.Key = Key; E.A = A; E.B = B;
	if (Log.Num() > MaxLog) Log.RemoveAt(0, Log.Num() - MaxLog);
}

// --- Identity -------------------------------------------------------------------------------
FString AFRRolesSubsystem::PlayerIdOf(const APlayerState* PlayerState)
{
	if (!PlayerState) return FString();
	const FUniqueNetIdRepl& NetId = PlayerState->GetUniqueId();
	if (NetId.IsValid())
	{
		const FString Raw = NetId.ToString();
		if (!Raw.IsEmpty()) return NetId.GetType().ToString() + TEXT(":") + Raw;
	}
	// Offline/LAN sessions without an online id: fall back to the account name.
	return TEXT("name:") + PlayerState->GetPlayerName();
}

FString AFRRolesSubsystem::PlayerIdOf(const APlayerController* Controller)
{
	return Controller ? PlayerIdOf(Controller->PlayerState) : FString();
}

const FFRPlayerRecord* AFRRolesSubsystem::FindPlayer(const FString& Id) const
{
	if (Id.IsEmpty()) return nullptr;
	return Players.FindByPredicate([&Id](const FFRPlayerRecord& P) { return P.Id == Id; });
}

FFRPlayerRecord* AFRRolesSubsystem::FindPlayerMutable(const FString& Id)
{
	if (Id.IsEmpty()) return nullptr;
	return Players.FindByPredicate([&Id](const FFRPlayerRecord& P) { return P.Id == Id; });
}

const FFRPlayerRecord* AFRRolesSubsystem::FindPlayer(const APlayerController* Controller) const
{
	return FindPlayer(PlayerIdOf(Controller));
}

FFRPlayerRecord* AFRRolesSubsystem::EnsureRecord(APlayerController* Controller)
{
	const FString Id = PlayerIdOf(Controller);
	if (Id.IsEmpty()) return nullptr;
	const FString Name = Controller->PlayerState ? Controller->PlayerState->GetPlayerName() : FString();
	const bool bLocalHost = Controller->IsLocalController() && HasAuthority();
	FFRPlayerRecord* Record = FindPlayerMutable(Id);
	bool bChanged = false;
	if (!Record)
	{
		Record = &Players.AddDefaulted_GetRef();
		Record->Id = Id;
		Record->JoinedUtc = NowUtc();
		bChanged = true;
		AddLog(TEXT("log.joined"), Name);
	}
	if (!Name.IsEmpty() && Record->Name != Name) { Record->Name = Name; bChanged = true; }
	if (!Record->bOnline) { Record->bOnline = true; bChanged = true; }
	// Host = the player of the listen server, or else the first engineer ever recorded (dedicated server).
	if (bLocalHost && !Record->bIsHost)
	{
		for (FFRPlayerRecord& P : Players) P.bIsHost = false;
		Record->bIsHost = true; bChanged = true;
	}
	if (!Players.ContainsByPredicate([](const FFRPlayerRecord& P) { return P.bIsHost; }))
	{
		Record->bIsHost = true; bChanged = true;
	}
	if (bChanged) UE_LOG(LogFRRoles, Display, TEXT("Player record %s (%s) host=%d local=%d"), *Record->Name, *Record->Id, Record->bIsHost, bLocalHost);
	if (bChanged) MarkChanged();
	return Record;
}

bool AFRRolesSubsystem::IsAdmin(const APlayerController* Controller) const
{
	if (!Controller) return false;
	if (HasAuthority() && Controller->IsLocalController()) return true;
	const FFRPlayerRecord* Record = FindPlayer(Controller);
	return Record && Record->bIsHost;
}

// --- Permission engine ----------------------------------------------------------------------
int32 AFRRolesSubsystem::MembersOf(FName Division, bool bOnlineOnly) const
{
	int32 Count = 0;
	for (const FFRPlayerRecord& P : Players)
		if (P.Division == Division && (!bOnlineOnly || P.bOnline)) ++Count;
	return Count;
}

int32 AFRRolesSubsystem::Capacity() const
{
	const int32 Divisions = FMath::Max(1, FFRConfig::Get().Divisions.Num());
	return FMath::Max(1, FMath::DivideAndRoundUp(FMath::Max(1, ExpectedPlayers), Divisions));
}

bool AFRRolesSubsystem::IsCovered(FName Division) const
{
	switch (Coverage)
	{
	case EFRCoverage::Vacant: return MembersOf(Division) == 0;
	case EFRCoverage::Absent: return MembersOf(Division, true) == 0;
	default: return false;
	}
}

bool AFRRolesSubsystem::IsNodeOpen(FName Node) const
{
	const FFRTechNode* N = FFRConfig::Get().FindNode(Node);
	if (!N || IsResearched(Node)) return false;
	for (const FName R : N->Requires) if (!IsResearched(R)) return false;
	return true; // the habilitation only authorises: building still needs the vanilla HUB/MAM unlock
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
}

bool AFRRolesSubsystem::IsGrantActive(const FFRGrant& Grant) const
{
	return !Grant.bRevoked && (Grant.ExpiresUtc == 0 || Grant.ExpiresUtc > NowUtc());
}

const FFRGrant* AFRRolesSubsystem::FindActiveGrant(const FString& PlayerId, EFRAction Action, FName Division, const FString& ClassShortName) const
{
	for (const FFRGrant& G : Grants)
	{
		if (G.PlayerId != PlayerId || G.Action != Action || !IsGrantActive(G)) continue;
		if (G.Target == FName(TEXT("*")) || G.Target == Division || (!ClassShortName.IsEmpty() && G.Target == FName(*ClassShortName)))
			return &G;
	}
	return nullptr;
}

FFRDecision AFRRolesSubsystem::EvaluateBuild(const APlayerController* Controller, const UClass* BuildClass) const
{
	return BuildClass ? EvaluatePerm(Controller, EFRPerm::Build, FRShortClassName(BuildClass)) : FFRDecision::Allow();
}

FFRDecision AFRRolesSubsystem::EvaluatePerm(const APlayerController* Controller, EFRPerm Perm, const FString& Target) const
{
	const FFRPermRule* Rule = FFRConfig::Get().RuleFor(Perm, Target);
	if (!Rule) return FFRDecision::Allow(); // common target (vanilla shared, other mods, updates)

	const FName OwnerDiv = Rule->BaseOwners.Num() > 0 ? Rule->BaseOwners[0] : Rule->NodeGrants[0].Key;
	const FFRPlayerRecord* Record = FindPlayer(Controller);
	if (!Record) return FFRDecision::Deny(TEXT("deny.syncing"), FString(), OwnerDiv);
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
	if (Div.IsNone()) return FFRDecision::Deny(TEXT("deny.no_division"), FString(), OwnerDiv);
	FString List;
	for (const FName O : Owners) List += (List.IsEmpty() ? TEXT("") : TEXT(",")) + O.ToString();
	static const TCHAR* Keys[] = { TEXT("deny.reserved"), TEXT("deny.craft"), TEXT("deny.collect"), TEXT("deny.equip") };
	return FFRDecision::Deny(Keys[(int32)Perm], List, OwnerDiv);
}

FFRDecision AFRRolesSubsystem::EvaluateDivisionAction(const APlayerController* Controller, EFRAction Action, FName OwnerDivision) const
{
	if (OwnerDivision.IsNone()) return FFRDecision::Allow();
	const FFRPlayerRecord* Record = FindPlayer(Controller);
	if (!Record) return FFRDecision::Deny(TEXT("deny.syncing"), FString(), OwnerDivision);
	if (Record->Division == OwnerDivision) return FFRDecision::Allow();
	if (FindActiveGrant(Record->Id, Action, OwnerDivision, FString()) || IsCovered(OwnerDivision))
	{ FFRDecision D; D.bViaException = true; D.Owner = OwnerDivision; return D; }
	return FFRDecision::Deny(Action == EFRAction::Research ? TEXT("deny.research") : TEXT("deny.reserved"), OwnerDivision.ToString(), OwnerDivision);
}

// --- Notifications --------------------------------------------------------------------------
void AFRRolesSubsystem::Notify(APlayerController* Controller, const FString& Key, const FString& A, const FString& B, uint8 Severity) const
{
	if (UFRRemote* Remote = UFRRemote::Get(Controller)) Remote->Client_Notify(Key, A, B, Severity);
}

void AFRRolesSubsystem::NotifyAll(const FString& Key, const FString& A, const FString& B, uint8 Severity, FName OnlyDivision) const
{
	UWorld* World = GetWorld();
	if (!World) return;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		if (!OnlyDivision.IsNone())
		{
			const FFRPlayerRecord* R = FindPlayer(PC);
			if (!R || R->Division != OnlyDivision) continue;
		}
		Notify(PC, Key, A, B, Severity);
	}
}

void AFRRolesSubsystem::Broadcast(const FString& Text) const
{
	AFGChatManager* Chat = AFGChatManager::Get(GetWorld());
	if (!Chat) return;
	FChatMessageStruct Message;
	Message.MessageType = EFGChatMessageType::CMT_SystemMessage;
	Message.MessageText = FText::FromString(Text);
	Message.ServerTimeStamp = GetWorld()->GetTimeSeconds();
	Chat->BroadcastChatMessage(Message);
}

void AFRRolesSubsystem::NotifyDenied(AFGPlayerController* Controller, const FFRDecision& Decision)
{
	if (!Controller || Decision.bAllowed) return;
	const FString Key = PlayerIdOf(Controller) + Decision.ReasonKey + Decision.Arg;
	const double Now = FPlatformTime::Seconds();
	double& Last = LastDenyNotify.FindOrAdd(Key, -1000.0);
	if (Now - Last < FFRConfig::Get().DenyCooldown) return;
	Last = Now;
	Notify(Controller, Decision.ReasonKey, Decision.Arg, FString(), 2);
}

void AFRRolesSubsystem::ServerResearch(AFGPlayerController* Controller, FName NodeId)
{
	if (!HasAuthority() || !Controller) return;
	if (FRResearch::IsActive()) { Notify(Controller, TEXT("tech.use_mam"), FString(), FString(), 0); return; } // researched in the MAM
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

void AFRRolesSubsystem::OnMamResearched(FName NodeId, bool bNotify)
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

void AFRRolesSubsystem::OnSchematicPurchased(TSubclassOf<UFGSchematic> Schematic)
{
	const FName Node = FRResearch::NodeOf(Schematic);
	if (!Node.IsNone()) OnMamResearched(Node, true);
}

void AFRRolesSubsystem::SyncMamResearch()
{
	AFGSchematicManager* Schematics = AFGSchematicManager::Get(GetWorld());
	if (!Schematics || !HasAuthority()) return;
	for (const FFRTechNode& N : FFRConfig::Get().Tech)
	{
		const TSubclassOf<UFGSchematic> S = FRResearch::SchematicOf(N.Id);
		if (!S) continue;
		if (Schematics->IsSchematicPurchased(S)) OnMamResearched(N.Id, false);
		// Researched in an older version (or under an old id): mark the MAM node done so it is not paid twice.
		else if (IsResearched(N.Id)) Schematics->GiveAccessToSchematic(S, nullptr);
	}
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

// --- Login / housekeeping -------------------------------------------------------------------
void AFRRolesSubsystem::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (!NewPlayer || NewPlayer->GetWorld() != GetWorld()) return;
	if (AFGPlayerController* PC = Cast<AFGPlayerController>(NewPlayer))
		if (!PC->GetRemoteCallObjectOfClass(UFRRemote::StaticClass())) PC->RegisterRemoteCallObjectClass(UFRRemote::StaticClass());
	// PlayerState/unique id are usually ready now; ServerHello from the client covers late cases.
	if (FFRPlayerRecord* Record = EnsureRecord(NewPlayer))
	{
		if (!Record->Division.IsNone())
		{
			AddLog(TEXT("log.returned"), Record->Name, Record->Division.ToString());
			NotifyAll(TEXT("notify.returned"), Record->Name, Record->Division.ToString(), 0);
			MarkChanged();
		}
	}
}

void AFRRolesSubsystem::HandleLogout(AGameModeBase* GameMode, AController* Exiting)
{
	if (!Exiting || Exiting->GetWorld() != GetWorld()) return;
	if (FFRPlayerRecord* Record = FindPlayerMutable(PlayerIdOf(Cast<APlayerController>(Exiting))))
	{
		Record->bOnline = false;
		AddLog(TEXT("log.left"), Record->Name);
		MarkChanged();
	}
}

void AFRRolesSubsystem::RefreshOnline()
{
	UWorld* World = GetWorld();
	AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState) return;
	TSet<FString> Online;
	for (APlayerState* PS : GameState->PlayerArray)
		if (PS && !PS->IsInactive()) Online.Add(PlayerIdOf(PS));
	bool bChanged = false;
	for (FFRPlayerRecord& P : Players)
	{
		const bool bNow = Online.Contains(P.Id);
		if (P.bOnline != bNow) { P.bOnline = bNow; bChanged = true; }
	}
	if (bChanged) MarkChanged();
}

void AFRRolesSubsystem::Housekeeping()
{
	RefreshOnline();
	bool bChanged = false;
	for (FFRGrant& G : Grants)
	{
		if (!G.bRevoked && !G.bExpiredNotified && G.ExpiresUtc != 0 && G.ExpiresUtc <= NowUtc())
		{
			G.bExpiredNotified = true;
			bChanged = true;
			AddLog(TEXT("log.grant_expired"), G.PlayerName, G.Target.ToString());
			UWorld* World = GetWorld();
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				if (PlayerIdOf(It->Get()) == G.PlayerId) Notify(It->Get(), TEXT("notify.grant_expired"), G.Target.ToString(), FString(), 2);
		}
	}
	// Keep history bounded: drop revoked/expired grants older than one day.
	const int64 Cutoff = NowUtc() - 86400;
	const int32 Before = Grants.Num();
	Grants.RemoveAll([this, Cutoff](const FFRGrant& G) { return !IsGrantActive(G) && G.CreatedUtc < Cutoff; });
	if (Before != Grants.Num()) bChanged = true;
	if (bChanged) MarkChanged();
}

FString AFRRolesSubsystem::Sanitize(const FString& In, int32 MaxLen)
{
	FString Out;
	Out.Reserve(FMath::Min(In.Len(), MaxLen));
	for (const TCHAR C : In)
	{
		if (Out.Len() >= MaxLen) break;
		if (C >= 32 && C != 127) Out.AppendChar(C);
	}
	Out.TrimStartAndEndInline();
	return Out;
}

// --- Server mutations -----------------------------------------------------------------------
void AFRRolesSubsystem::ServerHello(AFGPlayerController* Controller)
{
	if (!HasAuthority() || !Controller) return;
	if (!bCoverageInitialized)
	{
		bCoverageInitialized = true;
		Coverage = static_cast<EFRCoverage>(FFRConfig::Get().DefaultCoverage);
	}
	EnsureRecord(Controller);
	MarkChanged(); // guarantees the caller receives a fresh state version
}

void AFRRolesSubsystem::ServerConfigure(AFGPlayerController* Controller, int32 Expected)
{
	if (!HasAuthority()) return;
	EnsureRecord(Controller);
	if (!IsAdmin(Controller)) { UE_LOG(LogFRRoles, Warning, TEXT("Configure refused: not host")); Notify(Controller, TEXT("deny.admin"), FString(), FString(), 2); return; }
	if (Phase == EFRPhase::Running) { Notify(Controller, TEXT("deny.already_running"), FString(), FString(), 2); return; }
	ExpectedPlayers = FMath::Clamp(Expected, 1, FFRConfig::Get().MaxExpectedPlayers);
	Phase = EFRPhase::Lobby;
	UE_LOG(LogFRRoles, Display, TEXT("Session configured for %d player(s)"), ExpectedPlayers);
	AddLog(TEXT("log.configured"), FString::FromInt(ExpectedPlayers));
	MarkChanged();
}

void AFRRolesSubsystem::ServerStart(AFGPlayerController* Controller)
{
	if (!HasAuthority()) return;
	if (!IsAdmin(Controller)) { Notify(Controller, TEXT("deny.admin"), FString(), FString(), 2); return; }
	if (Phase == EFRPhase::Running) return; // idempotent
	Phase = EFRPhase::Running;
	AddLog(TEXT("log.started"));
	MarkChanged();
	NotifyAll(TEXT("notify.started"), FString(), FString(), 1);
}

void AFRRolesSubsystem::ServerChoose(AFGPlayerController* Controller, FName Division)
{
	if (!HasAuthority() || !Controller) return;
	FFRPlayerRecord* Record = EnsureRecord(Controller);
	if (!Record) return;
	if (!Record->Division.IsNone())
	{
		// Idempotent repeat of the same choice, or an attempt to switch: never changes the role.
		if (Record->Division != Division) Notify(Controller, TEXT("deny.locked"), Record->Division.ToString(), FString(), 2);
		return;
	}
	if (!FFRConfig::Get().FindDivision(Division)) { Notify(Controller, TEXT("deny.unknown_division"), FString(), FString(), 2); return; }
	if (Phase == EFRPhase::Setup && (ExpectedPlayers > 1 || !IsAdmin(Controller))) { UE_LOG(LogFRRoles, Warning, TEXT("Choose refused: setup pending (admin=%d)"), IsAdmin(Controller)); Notify(Controller, TEXT("deny.setup_pending"), FString(), FString(), 2); return; }
	if (MembersOf(Division) >= Capacity()) { Notify(Controller, TEXT("deny.taken"), Division.ToString(), FString(), 2); return; }

	Record->Division = Division;
	AddLog(TEXT("log.chose"), Record->Name, Division.ToString());
	// The session starts by itself once every expected engineer holds a division (solo: immediately).
	int32 Chosen = 0;
	for (const FFRPlayerRecord& P : Players) Chosen += P.Division.IsNone() ? 0 : 1;
	const bool bStart = Phase != EFRPhase::Running && Chosen >= ExpectedPlayers;
	if (bStart) { Phase = EFRPhase::Running; AddLog(TEXT("log.started")); }
	MarkChanged();
	if (bStart) NotifyAll(TEXT("notify.started"), FString(), FString(), 1);
	Notify(Controller, TEXT("notify.assigned"), Division.ToString(), FString(), 1);
	UE_LOG(LogFRRoles, Display, TEXT("Division %s assigned to %s"), *Division.ToString(), *Record->Name);
	GrantStartRecipes();
	GiveStartKit(Controller, *Record);
}

void AFRRolesSubsystem::ServerAdmin(AFGPlayerController* Controller, EFRAdminOp Op, const FString& TargetId, FName Arg, int32 Value, const FString& InText)
{
	if (!HasAuthority() || !Controller) return;
	EnsureRecord(Controller);
	if (!IsAdmin(Controller)) { Notify(Controller, TEXT("deny.admin"), FString(), FString(), 2); return; }
	const FFRPlayerRecord* Self = FindPlayer(Controller);
	const FString AdminName = Self ? Self->Name : FString(TEXT("Host"));
	const FString Text = Sanitize(InText, 200);
	FFRPlayerRecord* Target = FindPlayerMutable(TargetId);

	auto NotifyTarget = [this](const FString& Id, const FString& Key, const FString& A, uint8 Severity) {
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			if (PlayerIdOf(It->Get()) == Id) Notify(It->Get(), Key, A, FString(), Severity);
	};

	switch (Op)
	{
	case EFRAdminOp::ResetRole:
		if (!Target || Target->Division.IsNone()) return;
		AddLog(TEXT("log.reset"), Target->Name, Target->Division.ToString());
		Target->Division = NAME_None;
		NotifyTarget(Target->Id, TEXT("notify.reset"), FString(), 2);
		break;
	case EFRAdminOp::AssignDivision:
		if (!Target || !FFRConfig::Get().FindDivision(Arg) || Target->Division == Arg) return;
		if (MembersOf(Arg) >= Capacity() && Value == 0) { Notify(Controller, TEXT("deny.taken"), Arg.ToString(), FString(), 2); return; }
		AddLog(TEXT("log.assigned"), Target->Name, Arg.ToString());
		Target->Division = Arg;
		NotifyTarget(Target->Id, TEXT("notify.assigned"), Arg.ToString(), 1);
		GrantStartRecipes();
		break;
	case EFRAdminOp::SetCoverage:
		Coverage = static_cast<EFRCoverage>(FMath::Clamp(Value, 0, 2));
		bCoverageInitialized = true;
		AddLog(TEXT("log.coverage"), FString::FromInt(Value));
		break;
	case EFRAdminOp::Grant:
	{
		if (!Target) return;
		const bool bValidTarget = Arg == FName(TEXT("*")) || FFRConfig::Get().FindDivision(Arg) || FFRConfig::Get().RuleFor(EFRPerm::Build, Arg.ToString()) != nullptr;
		if (!bValidTarget) { Notify(Controller, TEXT("deny.bad_target"), FString(), FString(), 2); return; }
		FFRGrant& G = Grants.AddDefaulted_GetRef();
		G.Id = NextId++;
		G.PlayerId = Target->Id; G.PlayerName = Target->Name;
		G.Action = EFRAction::Build; G.Target = Arg;
		G.GrantedBy = AdminName; G.Reason = Text;
		G.CreatedUtc = NowUtc();
		G.ExpiresUtc = Value > 0 ? NowUtc() + int64(FMath::Clamp(Value, 1, 10080)) * 60 : 0;
		AddLog(TEXT("log.grant"), Target->Name, Arg.ToString());
		NotifyTarget(Target->Id, TEXT("notify.grant"), Arg.ToString(), 1);
		break;
	}
	case EFRAdminOp::Revoke:
	{
		FFRGrant* G = Grants.FindByPredicate([Value](const FFRGrant& X) { return X.Id == Value; });
		if (!G || G->bRevoked) return;
		G->bRevoked = true;
		AddLog(TEXT("log.revoke"), G->PlayerName, G->Target.ToString());
		NotifyTarget(G->PlayerId, TEXT("notify.revoked"), G->Target.ToString(), 2);
		break;
	}
	case EFRAdminOp::Announce:
	{
		if (Text.IsEmpty()) return;
		FFRMessage& M = Messages.AddDefaulted_GetRef();
		M.Utc = NowUtc(); M.AuthorName = AdminName; M.Text = Text; M.bAnnouncement = true;
		if (Messages.Num() > MaxMessages) Messages.RemoveAt(0, Messages.Num() - MaxMessages);
		Broadcast(FString::Printf(TEXT("[FICSIT] %s"), *Text));
		NotifyAll(TEXT("notify.announcement"), AdminName, Text, 0);
		break;
	}
	case EFRAdminOp::SetExpected:
		ExpectedPlayers = FMath::Clamp(Value, 1, FFRConfig::Get().MaxExpectedPlayers);
		AddLog(TEXT("log.configured"), FString::FromInt(ExpectedPlayers));
		break;
	case EFRAdminOp::BackToSetup:
		if (Phase == EFRPhase::Running) return;
		Phase = EFRPhase::Setup;
		break;
	}
	MarkChanged();
}

void AFRRolesSubsystem::ServerRequest(AFGPlayerController* Controller, EFRRequestOp Op, int32 RequestId, FName ToDivision, const FString& InText, bool bWithLocation)
{
	if (!HasAuthority() || !Controller) return;
	const FFRPlayerRecord* Self = EnsureRecord(Controller);
	if (!Self) return;
	const FString SelfId = Self->Id, SelfName = Self->Name;
	const FName SelfDivision = Self->Division;

	if (Op == EFRRequestOp::Create)
	{
		const FString Text = Sanitize(InText, 160);
		if (Text.IsEmpty() || !FFRConfig::Get().FindDivision(ToDivision)) { Notify(Controller, TEXT("deny.bad_request"), FString(), FString(), 2); return; }
		int32 Open = 0;
		for (const FFRRequest& R : Requests) if (R.AuthorId == SelfId && R.Status <= EFRRequestStatus::Taken) ++Open;
		if (Open >= MaxOpenRequestsPerAuthor) { Notify(Controller, TEXT("deny.too_many_requests"), FString(), FString(), 2); return; }
		FFRRequest& R = Requests.AddDefaulted_GetRef();
		R.Id = NextId++; R.AuthorId = SelfId; R.AuthorName = SelfName;
		R.FromDivision = SelfDivision; R.ToDivision = ToDivision; R.Text = Text;
		R.CreatedUtc = NowUtc();
		if (bWithLocation && Controller->GetPawn()) { R.bHasLocation = true; R.Location = Controller->GetPawn()->GetActorLocation(); }
		// Bounded history: drop the oldest closed requests first.
		while (Requests.Num() > MaxRequests)
		{
			const int32 Closed = Requests.IndexOfByPredicate([](const FFRRequest& X) { return X.Status >= EFRRequestStatus::Done; });
			Requests.RemoveAt(Closed != INDEX_NONE ? Closed : 0);
		}
		AddLog(TEXT("log.request"), SelfName, ToDivision.ToString());
		NotifyAll(TEXT("notify.request"), SelfName, Text, 0, ToDivision);
		MarkChanged();
		return;
	}

	FFRRequest* R = Requests.FindByPredicate([RequestId](const FFRRequest& X) { return X.Id == RequestId; });
	if (!R) return;
	const bool bAuthor = R->AuthorId == SelfId;
	const bool bAdmin = IsAdmin(Controller);
	switch (Op)
	{
	case EFRRequestOp::Take:
		if (R->Status != EFRRequestStatus::Open) return;
		if (SelfDivision != R->ToDivision && !IsCovered(R->ToDivision) && !bAdmin) { Notify(Controller, TEXT("deny.not_recipient"), R->ToDivision.ToString(), FString(), 2); return; }
		R->Status = EFRRequestStatus::Taken; R->HandlerName = SelfName;
		break;
	case EFRRequestOp::Done:
		if (R->Status != EFRRequestStatus::Taken && R->Status != EFRRequestStatus::Open) return;
		if (!bAuthor && R->HandlerName != SelfName && !bAdmin) return;
		R->Status = EFRRequestStatus::Done;
		if (R->HandlerName.IsEmpty()) R->HandlerName = SelfName;
		break;
	case EFRRequestOp::Cancel:
		if (R->Status >= EFRRequestStatus::Done || (!bAuthor && !bAdmin)) return;
		R->Status = EFRRequestStatus::Cancelled;
		break;
	default: return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (PlayerIdOf(It->Get()) == R->AuthorId && R->AuthorId != SelfId)
			Notify(It->Get(), TEXT("notify.request_update"), SelfName, FString::FromInt((int32)R->Status), 0);
	MarkChanged();
}

void AFRRolesSubsystem::ServerMessage(AFGPlayerController* Controller, FName ToDivision, const FString& InText)
{
	if (!HasAuthority() || !Controller) return;
	const FFRPlayerRecord* Self = EnsureRecord(Controller);
	if (!Self) return;
	const FString Text = Sanitize(InText, 200);
	if (Text.IsEmpty()) return;
	const double Now = FPlatformTime::Seconds();
	double& Last = LastMessageTime.FindOrAdd(Self->Id, -1000.0);
	if (Now - Last < 1.5) { Notify(Controller, TEXT("deny.slow_down"), FString(), FString(), 2); return; }
	Last = Now;
	if (!ToDivision.IsNone() && !FFRConfig::Get().FindDivision(ToDivision)) return;
	FFRMessage& M = Messages.AddDefaulted_GetRef();
	M.Utc = NowUtc(); M.AuthorName = Self->Name; M.FromDivision = Self->Division; M.ToDivision = ToDivision; M.Text = Text;
	if (Messages.Num() > MaxMessages) Messages.RemoveAt(0, Messages.Num() - MaxMessages);
	NotifyAll(TEXT("notify.message"), Self->Name, Text, 0, ToDivision);
	MarkChanged();
}
