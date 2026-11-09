#include "FRRemote.h"
#include "FRRolesSubsystem.h"
#include "UI/FRLocalSubsystem.h"
#include "FGPlayerController.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogFRNet, Log, All);
#define FR_TRACE(Fmt, ...) UE_LOG(LogFRNet, Display, TEXT("RPC %s from %s: " Fmt), ANSI_TO_TCHAR(__FUNCTION__), GetOwnerPlayerController() ? *GetOwnerPlayerController()->GetName() : TEXT("<none>"), ##__VA_ARGS__)

UFRRemote* UFRRemote::Get(APlayerController* Controller)
{
	AFGPlayerController* PC = Cast<AFGPlayerController>(Controller);
	return PC ? Cast<UFRRemote>(PC->GetRemoteCallObjectOfClass(UFRRemote::StaticClass())) : nullptr;
}

void UFRRemote::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UFRRemote, bForceNetField);
}

void UFRRemote::Server_Hello_Implementation()
{
	FR_TRACE("");
	AFRRolesSubsystem* RolesForHello = AFRRolesSubsystem::Get(this);
	if (!RolesForHello) UE_LOG(LogFRNet, Warning, TEXT("Hello: roles subsystem missing"));
	if (AFRRolesSubsystem* Roles = RolesForHello) Roles->ServerHello(GetOwnerPlayerController());
}

void UFRRemote::Server_Configure_Implementation(int32 Expected)
{
	FR_TRACE("%d", Expected);
	if (AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(this)) Roles->ServerConfigure(GetOwnerPlayerController(), Expected);
}

void UFRRemote::Server_Start_Implementation()
{
	FR_TRACE("");
	if (AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(this)) Roles->ServerStart(GetOwnerPlayerController());
}

void UFRRemote::Server_Choose_Implementation(FName Division)
{
	FR_TRACE("%s", *Division.ToString());
	if (AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(this)) Roles->ServerChoose(GetOwnerPlayerController(), Division);
}

void UFRRemote::Server_Admin_Implementation(EFRAdminOp Op, const FString& TargetId, FName Arg, int32 Value, const FString& Text)
{
	if (AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(this)) Roles->ServerAdmin(GetOwnerPlayerController(), Op, TargetId, Arg, Value, Text);
}

void UFRRemote::Server_Request_Implementation(EFRRequestOp Op, int32 RequestId, FName ToDivision, const FString& Text, bool bWithLocation)
{
	if (AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(this)) Roles->ServerRequest(GetOwnerPlayerController(), Op, RequestId, ToDivision, Text, bWithLocation);
}

void UFRRemote::Server_Message_Implementation(FName ToDivision, const FString& Text)
{
	if (AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(this)) Roles->ServerMessage(GetOwnerPlayerController(), ToDivision, Text);
}

void UFRRemote::Client_Notify_Implementation(const FString& Key, const FString& A, const FString& B, uint8 Severity)
{
	if (AFRLocalSubsystem* Local = AFRLocalSubsystem::Get(this)) Local->HandleNotify(Key, A, B, Severity);
}

void UFRRemote::Server_Research_Implementation(FName Node)
{
	FR_TRACE("%s", *Node.ToString());
	if (AFRRolesSubsystem* Roles = AFRRolesSubsystem::Get(this)) Roles->ServerResearch(GetOwnerPlayerController(), Node);
}
