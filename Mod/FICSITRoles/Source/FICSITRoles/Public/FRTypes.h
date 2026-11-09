// Shared data model for FICSIT Roles. Everything here is replicated and/or saved,
// so field order and names are part of the save format: append, never rename.
#pragma once

#include "CoreMinimal.h"
#include "FRTypes.generated.h"

/** Action categories evaluated by the permission engine. */
UENUM()
enum class EFRAction : uint8
{
	Build, Dismantle, Interact, Configure, Craft, Equip, Use, Collect, Research, Drive, Manage
};

/** Session flow: host picks player count, players gather, then the session runs. */
UENUM()
enum class EFRPhase : uint8 { Setup, Lobby, Running };

/** How domains without an active specialist are handled. */
UENUM()
enum class EFRCoverage : uint8
{
	Off,      // reserved buildings stay reserved even if nobody holds the division
	Vacant,   // anyone may cover a division that has no member at all
	Absent    // anyone may cover a division whose members are all offline
};

UENUM()
enum class EFRRequestStatus : uint8 { Open, Taken, Done, Cancelled };

UENUM()
enum class EFRAdminOp : uint8
{
	ResetRole, SetCoverage, Grant, Revoke, AssignDivision, Announce, SetExpected, BackToSetup
};

UENUM()
enum class EFRRequestOp : uint8 { Create, Take, Done, Cancel };

/** One known player. Identity = stable online id, never the display name. */
USTRUCT()
struct FFRPlayerRecord
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FString Id;
	UPROPERTY(SaveGame) FString Name;
	UPROPERTY(SaveGame) FName Division;
	UPROPERTY() bool bOnline = false;
	UPROPERTY(SaveGame) bool bIsHost = false;
	UPROPERTY(SaveGame) int64 JoinedUtc = 0;
	UPROPERTY(SaveGame) bool bKitGiven = false;
};

/** Temporary or session-long exception granted by the host. */
USTRUCT()
struct FFRGrant
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) int32 Id = 0;
	UPROPERTY(SaveGame) FString PlayerId;
	UPROPERTY(SaveGame) FString PlayerName;
	UPROPERTY(SaveGame) EFRAction Action = EFRAction::Build;
	/** Division id, building class short name (Build_X) or "*". */
	UPROPERTY(SaveGame) FName Target;
	UPROPERTY(SaveGame) FString GrantedBy;
	/** Unix UTC seconds; 0 = no expiry. */
	UPROPERTY(SaveGame) int64 ExpiresUtc = 0;
	UPROPERTY(SaveGame) int64 CreatedUtc = 0;
	UPROPERTY(SaveGame) FString Reason;
	UPROPERTY(SaveGame) bool bRevoked = false;
	UPROPERTY(SaveGame) bool bExpiredNotified = false;
};

/** Optional, unpaid assistance request between divisions. */
USTRUCT()
struct FFRRequest
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) int32 Id = 0;
	UPROPERTY(SaveGame) FString AuthorId;
	UPROPERTY(SaveGame) FString AuthorName;
	UPROPERTY(SaveGame) FName FromDivision;
	UPROPERTY(SaveGame) FName ToDivision;
	UPROPERTY(SaveGame) FString Text;
	UPROPERTY(SaveGame) bool bHasLocation = false;
	UPROPERTY(SaveGame) FVector Location = FVector::ZeroVector;
	UPROPERTY(SaveGame) EFRRequestStatus Status = EFRRequestStatus::Open;
	UPROPERTY(SaveGame) FString HandlerName;
	UPROPERTY(SaveGame) int64 CreatedUtc = 0;
};

/** Division message or host announcement. ToDivision None = everyone. */
USTRUCT()
struct FFRMessage
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) int64 Utc = 0;
	UPROPERTY(SaveGame) FString AuthorName;
	UPROPERTY(SaveGame) FName FromDivision;
	UPROPERTY(SaveGame) FName ToDivision;
	UPROPERTY(SaveGame) FString Text;
	UPROPERTY(SaveGame) bool bAnnouncement = false;
};

/** Event journal entry. Stored as a localization key + args so every client reads it in its own language. */
USTRUCT()
struct FFRLogEntry
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) int64 Utc = 0;
	UPROPERTY(SaveGame) FString Key;
	UPROPERTY(SaveGame) FString A;
	UPROPERTY(SaveGame) FString B;
};

/** Result of a permission evaluation, with a player-facing reason. */
struct FFRDecision
{
	bool bAllowed = true;
	/** Localization key of the refusal reason (empty when allowed). */
	FString ReasonKey;
	/** Argument for the reason (division name ids, clearance id...). */
	FString Arg;
	/** Division that owns the target, for UI/disqualifier selection. */
	FName Owner;
	/** True when the decision came from a host exception or vacant-division coverage. */
	bool bViaException = false;

	static FFRDecision Allow() { return FFRDecision(); }
	static FFRDecision Deny(const FString& Key, const FString& InArg = FString(), FName InOwner = NAME_None)
	{
		FFRDecision D; D.bAllowed = false; D.ReasonKey = Key; D.Arg = InArg; D.Owner = InOwner; return D;
	}
};
