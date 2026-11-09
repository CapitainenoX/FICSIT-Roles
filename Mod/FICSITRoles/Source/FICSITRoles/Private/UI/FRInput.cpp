#include "UI/FRInput.h"
#include "FRConfig.h"
#include "FRLoc.h"
#include "InputAction.h"
#include "PlayerMappableKeySettings.h"
#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "Input/FGInputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogFRInput, Log, All);

namespace
{
	UInputAction* GAction = nullptr;
	UFGInputMappingContext* GContext = nullptr;

	void Create()
	{
		if (GAction) return;
		GAction = NewObject<UInputAction>(GetTransientPackage(), TEXT("IA_FICSITRoles_Terminal"));
		GAction->AddToRoot();
		// Player-mappable settings make the binding appear (and persist) in the Controls menu.
		UPlayerMappableKeySettings* Settings = NewObject<UPlayerMappableKeySettings>(GAction, TEXT("TerminalKeySettings"));
		Settings->Name = FName(TEXT("FICSITRoles_Terminal"));
		Settings->DisplayName = FFRLoc::T(TEXT("key.action"));
		Settings->DisplayCategory = FText::FromString(TEXT("FICSIT Roles"));
		if (FObjectProperty* Prop = FindFProperty<FObjectProperty>(UInputAction::StaticClass(), TEXT("PlayerMappableKeySettings")))
			Prop->SetObjectPropertyValue_InContainer(GAction, Settings);

		GContext = NewObject<UFGInputMappingContext>(GetTransientPackage(), TEXT("IMC_FICSITRoles"));
		GContext->AddToRoot();
		GContext->mDisplayName = FText::FromString(TEXT("FICSIT Roles"));
		GContext->mMenuPriority = 50.f;
		GContext->MapKey(GAction, FRTerminalKey());
		UE_LOG(LogFRInput, Display, TEXT("Terminal input action created (default %s)"), *FRTerminalKey().ToString());
	}
}

UInputAction* FRInput::Action() { Create(); return GAction; }
UFGInputMappingContext* FRInput::Context() { Create(); return GContext; }

bool FRInput::Install(APlayerController* Controller)
{
	Create();
	ULocalPlayer* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input) return false;
	if (!Input->HasMappingContext(GContext)) Input->AddMappingContext(GContext, 1);
	if (UEnhancedInputUserSettings* User = Input->GetUserSettings())
	{
		const bool bRegistered = User->RegisterInputMappingContext(GContext);
		UE_LOG(LogFRInput, Display, TEXT("Terminal action installed (user settings registered=%d)"), bRegistered);
	}
	else UE_LOG(LogFRInput, Display, TEXT("Terminal action installed (no Enhanced Input user settings)"));
	return true;
}

FKey FRInput::CurrentKey(APlayerController* Controller)
{
	ULocalPlayer* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Input && GAction)
	{
		const TArray<FKey> Keys = Input->QueryKeysMappedToAction(GAction);
		for (const FKey& K : Keys) if (K.IsValid() && !K.IsGamepadKey()) return K;
	}
	return FRTerminalKey();
}
