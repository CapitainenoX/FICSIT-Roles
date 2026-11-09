// World module (entry point registered by SML), construction disqualifiers and the /roles chat command.
#pragma once

#include "CoreMinimal.h"
#include "Module/GameWorldModule.h"
#include "Command/ChatCommandInstance.h"
#include "FGConstructDisqualifier.h"
#include "FRWorldModule.generated.h"

UCLASS()
class FICSITROLES_API UFRWorldModule : public UGameWorldModule
{
	GENERATED_BODY()
public:
	UFRWorldModule();
	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
};

UCLASS()
class FICSITROLES_API AFRChatCommand : public AChatCommandInstance
{
	GENERATED_BODY()
public:
	AFRChatCommand();
	virtual EExecutionStatus ExecuteCommand_Implementation(class UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};

/** Base of all FICSIT Roles disqualifiers; texts are set at runtime in the player's language. */
UCLASS(Abstract)
class FICSITROLES_API UFRDisqualifier : public UFGConstructDisqualifier
{
	GENERATED_BODY()
public:
	UFRDisqualifier();
	static void SetText(TSubclassOf<UFRDisqualifier> Class, const FText& Text);
	/** Refresh every disqualifier text from the current localization. */
	static void RefreshTexts();
	/** Disqualifier class for a refused decision. */
	static TSubclassOf<UFGConstructDisqualifier> ForDecision(const struct FFRDecision& Decision);
};

UCLASS() class FICSITROLES_API UFRDisqReserved0 : public UFRDisqualifier { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRDisqReserved1 : public UFRDisqualifier { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRDisqReserved2 : public UFRDisqualifier { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRDisqReserved3 : public UFRDisqualifier { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRDisqReserved4 : public UFRDisqualifier { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRDisqReservedOther : public UFRDisqualifier { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRDisqNoDivision : public UFRDisqualifier { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRDisqClearance : public UFRDisqualifier { GENERATED_BODY() };
UCLASS() class FICSITROLES_API UFRDisqSyncing : public UFRDisqualifier { GENERATED_BODY() };
