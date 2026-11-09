// Client-side presentation of FICSIT Roles: onboarding flow, FICSIT terminal, toasts and HUD badge.
// One owner for the modal (UFRShellWidget added to the viewport at Z 10000), one non-interactive
// overlay (HUD badge + toasts). Input is captured only while the modal is open and is always
// restored on close, including world teardown.
#pragma once

#include "CoreMinimal.h"
#include "Subsystem/ModSubsystem.h"
#include "UI/FGInteractWidget.h"
#include "FRTypes.h"
#include "FRLocalSubsystem.generated.h"

class AFRRolesSubsystem;
class UFRRemote;
class AFGPlayerController;
class SFRRoot;
class SFROverlay;
class FFRInputProcessor;

UENUM()
enum class EFRScreen : uint8 { None, Setup, Lobby, Division, Terminal };

UCLASS()
class FICSITROLES_API UFRShellWidget : public UFGInteractWidget
{
	GENERATED_BODY()
public:
	UFRShellWidget(const FObjectInitializer& Initializer);
	TWeakObjectPtr<class AFRLocalSubsystem> Local;
	TSharedPtr<SFRRoot> Root;

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SetInputMode_Implementation() override;
	virtual void OnEscapePressed_Implementation() override;
	void ApplyInput();
	void ReleaseInput();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	bool bInputActive = false;
	bool bOwnLook = false;
	bool bOwnMove = false;
};

UCLASS()
class FICSITROLES_API AFRLocalSubsystem : public AModSubsystem
{
	GENERATED_BODY()
public:
	AFRLocalSubsystem();
	static AFRLocalSubsystem* Get(const UObject* WorldContext);

	// Accessors used by the Slate screens.
	AFRRolesSubsystem* Roles() const;
	UFRRemote* Remote() const;
	AFGPlayerController* Controller() const;
	const FFRPlayerRecord* Me() const;
	bool IsAdmin() const;
	bool IsSynced() const;
	EFRScreen CurrentScreen() const { return Screen; }

	// Flow
	void OpenScreen(EFRScreen NewScreen);
	void CloseMenu();
	void ToggleTerminal();
	void OnEscape();
	void Later();

	// Feedback
	void HandleNotify(const FString& Key, const FString& A, const FString& B, uint8 Severity);
	void OnLocalDenied(const FFRDecision& Decision);
	void PushToast(const FText& Text, uint8 Severity, FName Division = NAME_None);
	/** Localizes a notification / refusal key with its raw arguments. */
	static FText Explain(const FString& Key, const FString& A, const FString& B);
	FString TerminalKeyLabel() const;

	/** Pending flag set by screens while a server answer is awaited (cleared on next state). */
	bool bAwaitingServer = false;
	/** True while the terminal waits for the new hotkey. */
	bool bCapturingKey = false;
	void FinishKeyCapture(const FKey& Key);
	/** True when the terminal key is a real input action (Options > Controls). */
	bool bEnhancedInput = false;
	void OnTerminalAction();
	/** Pulls the key bound in the Controls menu into the displayed texts. */
	void SyncKey();
	void MarkAwaiting();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void TryInit();
	void OnRolesState();
	void AutoOpen();
	void InstallOverlay();
	void RemoveOverlay();

	UPROPERTY(Transient) TObjectPtr<UFRShellWidget> Shell;
	TSharedPtr<SFROverlay> Overlay;
	TSharedPtr<FFRInputProcessor> InputProcessor;
	TWeakObjectPtr<AFRRolesSubsystem> BoundRoles;
	FDelegateHandle StateHandle;
	FTimerHandle InitTimer;
	FTimerHandle ReopenTimer;
	FTimerHandle AwaitTimer;
	FTimerHandle IntroTimer;
	EFRScreen Screen = EFRScreen::None;
	int32 InitTries = 0;
	double HelloSentAt = -1.0;
	bool bDismissed = false;
	bool bAnnouncedRole = false;
	EFRPhase LastPhase = EFRPhase::Setup;
	FName LastDivision;
	bool bHadRecord = false;
	TMap<FString, double> LocalDenyTimes;
};
