#include "UI/FRLocalSubsystem.h"
#include "UI/FRScreens.h"
#include "FRConfig.h"
#include "FRLoc.h"
#include "FRRemote.h"
#include "FRRolesSubsystem.h"
#include "FRWorldModule.h"
#include "UI/FRNames.h"
#include "UI/FRInput.h"
#include "FRResearch.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "FGPlayerController.h"
#include "UI/FGGameUI.h"
#include "Subsystem/SubsystemActorManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFRUI, Log, All);

// =============================================================================================
// Terminal hotkey (configurable). Ignored while typing or while another game menu is open.
// =============================================================================================
class FFRInputProcessor : public IInputProcessor
{
public:
	explicit FFRInputProcessor(AFRLocalSubsystem* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
	{
		AFRLocalSubsystem* Local = Owner.Get();
		// Rebinding: the next key pressed becomes the terminal key (Escape cancels).
		if (Local && Local->bCapturingKey)
		{
			if (Event.IsRepeat()) return true;
			Local->FinishKeyCapture(Event.GetKey());
			return true;
		}
		// With the Enhanced Input action the game handles the key in play; we only catch it while our
		// UI-only modal is open (game input is suspended there) so the same key also closes it.
		if (Local && Local->bEnhancedInput && Local->CurrentScreen() == EFRScreen::None) return false;
		if (Event.GetKey() != FRTerminalKey() || Event.IsRepeat()) return false;
		AFGPlayerController* PC = Local ? Local->Controller() : nullptr;
		if (!PC || PC->IsPauseMenuOpen()) return false;
		const TSharedPtr<SWidget> Focused = SlateApp.GetKeyboardFocusedWidget();
		if (Focused.IsValid() && Focused->GetTypeAsString().Contains(TEXT("EditableText"))) return false;
		UFGGameUI* GameUI = PC->GetGameUI();
		if (Local->CurrentScreen() == EFRScreen::None && GameUI && GameUI->HasActiveInteractWidget()) return false;
		Local->ToggleTerminal();
		return true;
	}

private:
	TWeakObjectPtr<AFRLocalSubsystem> Owner;
};

// =============================================================================================
// Shell widget: single owner of the modal and of input capture.
// =============================================================================================
UFRShellWidget::UFRShellWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
	mUseKeyboard = true;
	mUseMouse = true;
	mHideCursorDuringCapture = false;
	mMouseLockMode = EMouseLockMode::DoNotLock;
	mRestoreFocusWhenLost = false;
	mSupportsStacking = true;
	mCustomTickRate = 0.f;
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
}

TSharedRef<SWidget> UFRShellWidget::RebuildWidget()
{
	return SAssignNew(Root, SFRRoot).Local(Local);
}

void UFRShellWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Root.Reset();
}

void UFRShellWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyInput();
}

void UFRShellWidget::NativeDestruct()
{
	Super::NativeDestruct();
	ReleaseInput();
}

void UFRShellWidget::SetInputMode_Implementation()
{
	ApplyInput();
}

void UFRShellWidget::OnEscapePressed_Implementation()
{
	if (Local.IsValid()) Local->OnEscape();
}

FReply UFRShellWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape)
	{
		if (Local.IsValid()) Local->OnEscape();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UFRShellWidget::ApplyInput()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	bInputActive = true;
	PC->bShowMouseCursor = true;
	if (!PC->IsLookInputIgnored()) { PC->SetIgnoreLookInput(true); bOwnLook = true; }
	if (!PC->IsMoveInputIgnored()) { PC->SetIgnoreMoveInput(true); bOwnMove = true; }
	FInputModeUIOnly Mode;
	const TSharedPtr<SWidget> Focus = GetCachedWidget();
	if (Focus.IsValid()) Mode.SetWidgetToFocus(Focus);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	if (ULocalPlayer* LP = PC->GetLocalPlayer())
		if (UGameViewportClient* View = LP->ViewportClient.Get())
		{
			View->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
			View->SetMouseLockMode(EMouseLockMode::DoNotLock);
		}
	FSlateApplication::Get().ReleaseAllPointerCapture();
	if (Focus.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(Focus, EFocusCause::SetDirectly);
		if (ULocalPlayer* LP = PC->GetLocalPlayer()) FSlateApplication::Get().SetUserFocus(LP->GetControllerId(), Focus, EFocusCause::SetDirectly);
	}
}

void UFRShellWidget::ReleaseInput()
{
	if (!bInputActive) return;
	bInputActive = false;
	AFGPlayerController* PC = Cast<AFGPlayerController>(GetOwningPlayer());
	if (!IsValid(PC)) return;
	if (bOwnLook) PC->SetIgnoreLookInput(false);
	if (bOwnMove) PC->SetIgnoreMoveInput(false);
	bOwnLook = bOwnMove = false;
	UFGGameUI* GameUI = PC->GetGameUI();
	if (!GameUI || !GameUI->HasActiveInteractWidget())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

// =============================================================================================
// Local subsystem
// =============================================================================================
AFRLocalSubsystem::AFRLocalSubsystem()
{
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnLocal;
	PrimaryActorTick.bCanEverTick = false;
}

AFRLocalSubsystem* AFRLocalSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	USubsystemActorManager* Manager = World ? World->GetSubsystem<USubsystemActorManager>() : nullptr;
	return Manager ? Manager->GetSubsystemActor<AFRLocalSubsystem>() : nullptr;
}

AFRRolesSubsystem* AFRLocalSubsystem::Roles() const { return AFRRolesSubsystem::Get(this); }
UFRRemote* AFRLocalSubsystem::Remote() const { return UFRRemote::Get(Controller()); }

AFGPlayerController* AFRLocalSubsystem::Controller() const
{
	UWorld* World = GetWorld();
	AFGPlayerController* PC = World ? Cast<AFGPlayerController>(World->GetFirstPlayerController()) : nullptr;
	return PC && PC->IsLocalController() ? PC : nullptr;
}

const FFRPlayerRecord* AFRLocalSubsystem::Me() const
{
	const AFRRolesSubsystem* R = Roles();
	return R ? R->FindPlayer(Controller()) : nullptr;
}

bool AFRLocalSubsystem::IsAdmin() const
{
	const AFRRolesSubsystem* R = Roles();
	return R && R->IsAdmin(Controller());
}

bool AFRLocalSubsystem::IsSynced() const { return Me() != nullptr; }

FString AFRLocalSubsystem::TerminalKeyLabel() const { return FRTerminalKeyName().ToString(); }

void AFRLocalSubsystem::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) return;
	GetWorldTimerManager().SetTimer(InitTimer, this, &AFRLocalSubsystem::TryInit, 0.5f, true, 0.5f);
}

void AFRLocalSubsystem::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(InitTimer);
	GetWorldTimerManager().ClearTimer(ReopenTimer);
	GetWorldTimerManager().ClearTimer(IntroTimer);
	if (BoundRoles.IsValid()) BoundRoles->OnStateChanged.Remove(StateHandle);
	BoundRoles.Reset();
	CloseMenu();
	RemoveOverlay();
	if (InputProcessor.IsValid() && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	InputProcessor.Reset();
	Super::EndPlay(Reason);
}

void AFRLocalSubsystem::TryInit()
{
	if (++InitTries > 1200) // 10 minutes: give up quietly, the hotkey still works once ready
	{
		GetWorldTimerManager().ClearTimer(InitTimer);
		UE_LOG(LogFRUI, Warning, TEXT("FICSIT Roles UI could not synchronize with the session"));
		return;
	}
	AFGPlayerController* PC = Controller();
	if (!PC || !PC->PlayerState || !PC->GetGameUI()) return;

	InstallOverlay();
	if (!bEnhancedInput && FRInput::Install(PC))
	{
		if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PC->InputComponent))
		{
			Input->BindAction(FRInput::Action(), ETriggerEvent::Started, this, &AFRLocalSubsystem::OnTerminalAction);
			bEnhancedInput = true;
			SyncKey();
		}
	}
	if (!InputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		InputProcessor = MakeShared<FFRInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
	}

	const double Now = FPlatformTime::Seconds();
	if (UFRRemote* RCO = Remote())
	{
		if (HelloSentAt < 0.0 || (!Me() && Now - HelloSentAt > 4.0))
		{
			RCO->Server_Hello();
			UE_LOG(LogFRUI, Display, TEXT("Hello sent (admin=%d)"), IsAdmin());
			HelloSentAt = Now;
		}
	}
	AFRRolesSubsystem* R = Roles();
	if (R && !BoundRoles.IsValid())
	{
		StateHandle = R->OnStateChanged.AddUObject(this, &AFRLocalSubsystem::OnRolesState);
		BoundRoles = R;
	}
	if (R && Me())
	{
		GetWorldTimerManager().ClearTimer(InitTimer);
		UE_LOG(LogFRUI, Display, TEXT("FICSIT Roles UI synchronized (phase=%d)"), (int32)R->Phase);
		FRResearch::FillContent(); // client copy of the MAM nodes: names, costs, icons
		OnRolesState();
	}
}

void AFRLocalSubsystem::InstallOverlay()
{
	if (Overlay.IsValid()) return;
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!Viewport) return;
	SAssignNew(Overlay, SFROverlay).Local(this);
	Viewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 9000);
}

void AFRLocalSubsystem::RemoveOverlay()
{
	if (!Overlay.IsValid()) return;
	if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	Overlay.Reset();
}

void AFRLocalSubsystem::OnRolesState()
{
	bAwaitingServer = false;
	const AFRRolesSubsystem* R = Roles();
	const FFRPlayerRecord* Self = Me();
	if (!R || !Self) return;

	if (R->Phase != LastPhase) { bDismissed = false; LastPhase = R->Phase; }
	if (bHadRecord && !LastDivision.IsNone() && Self->Division.IsNone()) bDismissed = false; // role reset by the host
	if (bHadRecord && LastDivision.IsNone() && !Self->Division.IsNone() && !bAnnouncedRole)
	{
		bAnnouncedRole = true; // the server also sends notify.assigned; this one welcomes into the session
		PushToast(FFRLoc::F(TEXT("toast.welcome"), FFRLoc::Division(Self->Division), FRTerminalKeyName()), 1, Self->Division);
	}
	if (Self->Division.IsNone()) bAnnouncedRole = false;
	LastDivision = Self->Division;
	bHadRecord = true;

	if (Shell && Shell->Root.IsValid()) Shell->Root->Refresh();
	AutoOpen();
}

void AFRLocalSubsystem::AutoOpen()
{
	const AFRRolesSubsystem* R = Roles();
	const FFRPlayerRecord* Self = Me();
	AFGPlayerController* PC = Controller();
	if (!R || !Self || !PC) return;
	if (Screen == EFRScreen::Terminal) return;
	if (PC->IsPauseMenuOpen()) return;
	// Never interrupt the landing-pod intro: wait for it to end, then open.
	if (PC->IsPlayingIntroSequence())
	{
		if (!GetWorldTimerManager().IsTimerActive(IntroTimer))
			GetWorldTimerManager().SetTimer(IntroTimer, FTimerDelegate::CreateWeakLambda(this, [this] {
				AFGPlayerController* Owner = Controller();
				if (Owner && Owner->IsPlayingIntroSequence()) return;
				GetWorldTimerManager().ClearTimer(IntroTimer);
				AutoOpen();
			}), 1.f, true);
		return;
	}

	// Setup: the host picks the player count; everyone else already sees the divisions (choice opens after).
	// Lobby: every engineer chooses once; the screen stays as a waiting room until all expected players chose.
	// Running: only players still without a division get the choice.
	const bool bNoDivision = Self->Division.IsNone();
	EFRScreen Desired = EFRScreen::None;
	switch (R->Phase)
	{
	case EFRPhase::Setup: Desired = IsAdmin() ? EFRScreen::Setup : EFRScreen::Division; break;
	case EFRPhase::Lobby: Desired = EFRScreen::Division; break;
	case EFRPhase::Running: Desired = bNoDivision ? EFRScreen::Division : EFRScreen::None; break;
	}

	if (Desired == EFRScreen::None)
	{
		if (Screen != EFRScreen::None) CloseMenu();
		return;
	}
	if (bDismissed && Screen == EFRScreen::None) return;
	if (Screen != Desired) OpenScreen(Desired);
}

void AFRLocalSubsystem::OpenScreen(EFRScreen NewScreen)
{
	if (NewScreen == EFRScreen::None) { CloseMenu(); return; }
	AFGPlayerController* PC = Controller();
	if (!PC) return;
	GetWorldTimerManager().ClearTimer(ReopenTimer);
	if (!Shell)
	{
		Shell = CreateWidget<UFRShellWidget>(PC, UFRShellWidget::StaticClass());
		if (!Shell) return;
		Shell->Local = this;
		Shell->mInteractObject = PC;
		// Global viewport layer (above the player canvas) so the modal really receives input.
		Shell->AddToViewport(10000);
		if (!Shell->IsInViewport()) { Shell->ReleaseInput(); Shell = nullptr; return; }
		Shell->ApplyInput();
		const TWeakObjectPtr<UFRShellWidget> Weak = Shell;
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [Weak] {
			if (Weak.IsValid() && Weak->IsInViewport()) Weak->ApplyInput();
		}));
	}
	Screen = NewScreen;
	if (Shell->Root.IsValid()) Shell->Root->SetScreen(NewScreen);
}

void AFRLocalSubsystem::CloseMenu()
{
	Screen = EFRScreen::None;
	UFRShellWidget* Previous = Shell;
	Shell = nullptr;
	if (IsValid(Previous))
	{
		Previous->RemoveFromParent();
		Previous->ReleaseInput();
	}
}

void AFRLocalSubsystem::ToggleTerminal()
{
	SyncKey();
	if (Screen != EFRScreen::None) { CloseMenu(); return; }
	const AFRRolesSubsystem* R = Roles();
	const FFRPlayerRecord* Self = Me();
	if (!R || !Self) { PushToast(FFRLoc::T(TEXT("deny.syncing")), 2); return; }
	// Without a role F7 leads to the one-time choice; with a role it only shows the read-only overview.
	if (Self->Division.IsNone()) { bDismissed = false; OpenScreen(R->Phase == EFRPhase::Setup && IsAdmin() ? EFRScreen::Setup : EFRScreen::Division); return; }
	OpenScreen(EFRScreen::Terminal);
}

void AFRLocalSubsystem::OnEscape()
{
	if (bCapturingKey) { FinishKeyCapture(EKeys::Escape); return; }
	if (Screen == EFRScreen::Terminal) { CloseMenu(); return; }
	// Onboarding is required to build reserved machines, but Escape must always reach the pause menu.
	CloseMenu();
	AFGPlayerController* PC = Controller();
	if (PC && PC->GetGameUI()) PC->GetGameUI()->Native_HandlePauseGamePressed();
	GetWorldTimerManager().SetTimer(ReopenTimer, FTimerDelegate::CreateWeakLambda(this, [this] {
		AFGPlayerController* Owner = Controller();
		if (!Owner) { GetWorldTimerManager().ClearTimer(ReopenTimer); return; }
		if (Owner->IsPauseMenuOpen()) return;
		GetWorldTimerManager().ClearTimer(ReopenTimer);
		AutoOpen();
	}), 0.4f, true);
}

void AFRLocalSubsystem::MarkAwaiting()
{
	bAwaitingServer = true;
	// Safety net: a refused or lost request never leaves the buttons disabled.
	GetWorldTimerManager().SetTimer(AwaitTimer, FTimerDelegate::CreateWeakLambda(this, [this] { bAwaitingServer = false; }), 3.f, false);
}

void AFRLocalSubsystem::FinishKeyCapture(const FKey& Key)
{
	bCapturingKey = false;
	if (Key == EKeys::Escape || !Key.IsValid() || Key.IsMouseButton() || Key.IsGamepadKey())
	{
		PushToast(FFRLoc::T(TEXT("key.cancelled")), 0);
	}
	else
	{
		FRSetTerminalKey(Key);
		UFRDisqualifier::RefreshTexts();
		PushToast(FFRLoc::F(TEXT("key.saved"), Key.GetDisplayName()), 1);
	}
	if (Shell && Shell->Root.IsValid()) Shell->Root->Refresh();
}

void AFRLocalSubsystem::OnTerminalAction()
{
	AFGPlayerController* PC = Controller();
	if (!PC || PC->IsPauseMenuOpen()) return;
	if (Screen == EFRScreen::None && PC->GetGameUI() && PC->GetGameUI()->HasActiveInteractWidget()) return;
	ToggleTerminal();
}

void AFRLocalSubsystem::SyncKey()
{
	const FKey Key = FRInput::CurrentKey(Controller());
	if (Key.IsValid() && Key != FRTerminalKey())
	{
		FRSetTerminalKey(Key);
		UFRDisqualifier::RefreshTexts();
	}
}

void AFRLocalSubsystem::Later()
{
	bDismissed = true;
	CloseMenu();
	PushToast(FFRLoc::F(TEXT("toast.later"), FRTerminalKeyName()), 0);
}

// =============================================================================================
// Feedback
// =============================================================================================
namespace
{
	FText SmartArg(const FString& Arg)
	{
		if (Arg.IsEmpty()) return FText::GetEmpty();
		if (Arg == TEXT("*")) return FFRLoc::T(TEXT("grant.all"));
		if (Arg.Contains(TEXT(",")))
		{
			TArray<FString> Parts;
			Arg.ParseIntoArray(Parts, TEXT(","));
			FString Joined;
			for (const FString& P : Parts) Joined += (Joined.IsEmpty() ? TEXT("") : TEXT(" / ")) + SmartArg(P).ToString();
			return FText::FromString(Joined);
		}
		const FFRConfig& Config = FFRConfig::Get();
		if (Config.FindDivision(*Arg)) return FFRLoc::Division(*Arg);
		if (Config.FindNode(*Arg)) return FRNames::Node(*Arg);
		if (Arg.StartsWith(TEXT("Desc_")) || Arg.StartsWith(TEXT("BP_"))) return FRNames::Item(Arg);
		return FText::FromString(Arg);
	}
}

FText AFRLocalSubsystem::Explain(const FString& Key, const FString& A, const FString& B)
{
	if (Key == TEXT("deny.no_division")) return FFRLoc::F(Key, FRTerminalKeyName());
	if (Key == TEXT("deny.tech"))
	{
		const FFRTechNode* N = FFRConfig::Get().FindNode(*A);
		return FFRLoc::F(Key, FRNames::Node(*A), FRTerminalKeyName());
	}
	if (Key == TEXT("log.coverage")) return FFRLoc::F(Key, FFRLoc::T(TEXT("coverage.") + A));
	if (Key == TEXT("notify.request_update")) return FFRLoc::F(Key, FText::FromString(A), FFRLoc::T(TEXT("req.status.") + B));
	return FFRLoc::F(Key, SmartArg(A), SmartArg(B));
}

void AFRLocalSubsystem::HandleNotify(const FString& Key, const FString& A, const FString& B, uint8 Severity)
{
	FName Division;
	if (FFRConfig::Get().FindDivision(*A)) Division = *A;
	PushToast(Explain(Key, A, B), Severity, Division);
}

void AFRLocalSubsystem::OnLocalDenied(const FFRDecision& Decision)
{
	const FString Key = Decision.ReasonKey + Decision.Arg;
	const double Now = FPlatformTime::Seconds();
	double& Last = LocalDenyTimes.FindOrAdd(Key, -1000.0);
	if (Now - Last < 8.0) return; // the red hologram text stays visible; the toast is a one-time explanation
	Last = Now;
	PushToast(Explain(Decision.ReasonKey, Decision.Arg, FString()), 2, Decision.Owner);
}

void AFRLocalSubsystem::PushToast(const FText& Text, uint8 Severity, FName Division)
{
	if (Overlay.IsValid()) Overlay->PushToast(Text, Severity, Division);
}
