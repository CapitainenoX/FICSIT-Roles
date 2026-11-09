// Authoritative session state of FICSIT Roles.
// Spawned on the server, replicated to every client, saved inside the world save.
// All mutations go through the Server* functions (called by UFRRemote RPCs) and are
// validated here; clients only read the replicated copy to explain decisions in the UI.
#pragma once

#include "CoreMinimal.h"
#include "Subsystem/ModSubsystem.h"
#include "FGSaveInterface.h"
#include "FRTypes.h"
#include "FRConfig.h"
#include "FRRolesSubsystem.generated.h"

class AFGPlayerController;
class APlayerController;
class APlayerState;
class AGameModeBase;

UCLASS()
class FICSITROLES_API AFRRolesSubsystem : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	AFRRolesSubsystem();

	static AFRRolesSubsystem* Get(const UObject* WorldContext);

	// --- Replicated + saved state -------------------------------------------------
	UPROPERTY(ReplicatedUsing = OnRep_State, SaveGame) int32 SchemaVersion = 1; // save layout version, see CurrentSchema in PostLoadGame
	UPROPERTY(ReplicatedUsing = OnRep_State, SaveGame) EFRPhase Phase = EFRPhase::Setup;
	UPROPERTY(ReplicatedUsing = OnRep_State, SaveGame) int32 ExpectedPlayers = 1;
	UPROPERTY(ReplicatedUsing = OnRep_State, SaveGame) EFRCoverage Coverage = EFRCoverage::Off;
	UPROPERTY(ReplicatedUsing = OnRep_State, SaveGame) TArray<FFRPlayerRecord> Players;
	UPROPERTY(ReplicatedUsing = OnRep_State, SaveGame) TArray<FFRGrant> Grants;
	UPROPERTY(ReplicatedUsing = OnRep_State, SaveGame) TArray<FFRRequest> Requests;
	UPROPERTY(ReplicatedUsing = OnRep_State, SaveGame) TArray<FFRMessage> Messages;
	UPROPERTY(ReplicatedUsing = OnRep_State, SaveGame) TArray<FFRLogEntry> Log;
	/** Division tech nodes researched in this world. */
	UPROPERTY(ReplicatedUsing = OnRep_State, SaveGame) TArray<FName> Researched;
	UPROPERTY(SaveGame) int32 NextId = 1;
	UPROPERTY(SaveGame) bool bCoverageInitialized = false;
	/** Bumped on every change; lets the UI rebuild only when something moved. */
	UPROPERTY(ReplicatedUsing = OnRep_State) int32 StateVersion = 0;
	/** Server clock (unix seconds) at the last change, used by clients to show expiries. */
	UPROPERTY(Replicated) int64 ServerUtcAtChange = 0;

	/** Fires on server and clients after any state change (UI listens to this). */
	FSimpleMulticastDelegate OnStateChanged;

	// --- Queries (valid on server and clients) ------------------------------------
	static FString PlayerIdOf(const APlayerState* PlayerState);
	static FString PlayerIdOf(const APlayerController* Controller);
	const FFRPlayerRecord* FindPlayer(const FString& Id) const;
	const FFRPlayerRecord* FindPlayer(const APlayerController* Controller) const;
	int32 MembersOf(FName Division, bool bOnlineOnly = false) const;
	/** Maximum members per division for the configured player count. */
	int32 Capacity() const;
	bool IsCovered(FName Division) const;
	bool IsResearched(FName Node) const { return Researched.Contains(Node); }
	/** Node researchable now (requirements met, not researched yet). */
	bool IsNodeOpen(FName Node) const;
	/** Every building/recipe a node authorises is already unlocked by the vanilla progression. */
	bool IsNodeVanillaReady(FName Node) const;
	const FFRGrant* FindActiveGrant(const FString& PlayerId, EFRAction Action, FName Division, const FString& ClassShortName) const;
	bool IsGrantActive(const FFRGrant& Grant) const;
	int64 NowUtc() const;

	/** Central permission decision for a building class. */
	FFRDecision EvaluateBuild(const APlayerController* Controller, const UClass* BuildClass) const;
	/** Central decision for any restricted action family. */
	FFRDecision EvaluatePerm(const APlayerController* Controller, EFRPerm Perm, const FString& Target) const;
	/** Permission for an action owned by a division (MAM division research...). */
	FFRDecision EvaluateDivisionAction(const APlayerController* Controller, EFRAction Action, FName OwnerDivision) const;
	bool IsAdmin(const APlayerController* Controller) const;

	// --- Server mutations (validated, idempotent) ---------------------------------
	void ServerHello(AFGPlayerController* Controller);
	void ServerConfigure(AFGPlayerController* Controller, int32 Expected);
	void ServerStart(AFGPlayerController* Controller);
	void ServerChoose(AFGPlayerController* Controller, FName Division);
	void ServerAdmin(AFGPlayerController* Controller, EFRAdminOp Op, const FString& TargetId, FName Arg, int32 Value, const FString& Text);
	void ServerRequest(AFGPlayerController* Controller, EFRRequestOp Op, int32 RequestId, FName ToDivision, const FString& Text, bool bWithLocation);
	void ServerMessage(AFGPlayerController* Controller, FName ToDivision, const FString& Text);
	/** Throttled player feedback for a refused action. */
	void NotifyDenied(AFGPlayerController* Controller, const FFRDecision& Decision);
	void ServerResearch(AFGPlayerController* Controller, FName Node);
	/** A FICSIT MAM schematic completed (bNotify=false when re-applied on load). */
	void OnMamResearched(FName Node, bool bNotify);
	/** Marks every already-purchased FICSIT MAM schematic as researched (save load). */
	void SyncMamResearch();

	// --- Engine / save hooks ------------------------------------------------------
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual bool ShouldSave_Implementation() const override { return true; }
	virtual bool NeedTransform_Implementation() override { return false; }
	virtual void PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override;

private:
	UFUNCTION() void OnRep_State();
	UFUNCTION() void OnSchematicPurchased(TSubclassOf<UFGSchematic> Schematic);

	void MarkChanged();
	/** Makes the start recipes of every held division available (idempotent, server). */
	void GrantStartRecipes();
	void GiveStartKit(APlayerController* Controller, FFRPlayerRecord& Record);
	void AddLog(const FString& Key, const FString& A = FString(), const FString& B = FString());
	FFRPlayerRecord* EnsureRecord(APlayerController* Controller);
	FFRPlayerRecord* FindPlayerMutable(const FString& Id);
	void Notify(APlayerController* Controller, const FString& Key, const FString& A = FString(), const FString& B = FString(), uint8 Severity = 0) const;
	void NotifyAll(const FString& Key, const FString& A = FString(), const FString& B = FString(), uint8 Severity = 0, FName OnlyDivision = NAME_None) const;
	void Broadcast(const FString& Text) const;
	void RefreshOnline();
	void Housekeeping();
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void HandleLogout(AGameModeBase* GameMode, AController* Exiting);
	static FString Sanitize(const FString& In, int32 MaxLen);

	FTimerHandle HousekeepingTimer;
	FDelegateHandle PostLoginHandle, LogoutHandle;
	TMap<FString, double> LastDenyNotify;
	TMap<FString, double> LastMessageTime;
};
