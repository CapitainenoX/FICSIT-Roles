// Remote Call Object: the only client -> server channel of FICSIT Roles.
// The server never trusts an identity sent by the client: the caller is always the
// player controller that owns this object.
#pragma once

#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "FRTypes.h"
#include "FRRemote.generated.h"

UCLASS()
class FICSITROLES_API UFRRemote : public UFGRemoteCallObject
{
	GENERATED_BODY()

public:
	static UFRRemote* Get(APlayerController* Controller);

	UFUNCTION(Server, Reliable) void Server_Hello();
	UFUNCTION(Server, Reliable) void Server_Configure(int32 Expected);
	UFUNCTION(Server, Reliable) void Server_Start();
	UFUNCTION(Server, Reliable) void Server_Choose(FName Division);
	UFUNCTION(Server, Reliable) void Server_Admin(EFRAdminOp Op, const FString& TargetId, FName Arg, int32 Value, const FString& Text);
	UFUNCTION(Server, Reliable) void Server_Request(EFRRequestOp Op, int32 RequestId, FName ToDivision, const FString& Text, bool bWithLocation);
	UFUNCTION(Server, Reliable) void Server_Message(FName ToDivision, const FString& Text);
	UFUNCTION(Server, Reliable) void Server_Research(FName Node);

	/** Localized on the receiving client. Severity: 0 info, 1 success, 2 warning/refusal. */
	UFUNCTION(Client, Reliable) void Client_Notify(const FString& Key, const FString& A, const FString& B, uint8 Severity);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** An RCO needs at least one replicated property to be replicated at all. */
	UPROPERTY(Replicated) bool bForceNetField = true;
};
