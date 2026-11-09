"""Client wiring: Enhanced Input action bound, key sync, MAM-aware F7 tree, key row hint."""
import os

SRC = r"F:\FICSIT-Roles-Rebuild\Mod\FICSITRoles\Source\FICSITRoles"


def rw(rel, fn):
    p = os.path.join(SRC, rel)
    s = open(p, encoding="utf-8").read()
    open(p, "w", encoding="utf-8").write(fn(s))


def must(s, a):
    assert a in s, a[:100]
    return a


rw("Public/UI/FRLocalSubsystem.h", lambda s: s.replace(must(s, "	void FinishKeyCapture(const FKey& Key);"),
   "	void FinishKeyCapture(const FKey& Key);\n	/** True when the terminal key is a real input action (Options > Controls). */\n	bool bEnhancedInput = false;\n	void OnTerminalAction();\n	/** Pulls the key bound in the Controls menu into the displayed texts. */\n	void SyncKey();"))


def local(s):
    s = s.replace(must(s, '#include "UI/FRNames.h"'), '#include "UI/FRNames.h"\n#include "UI/FRInput.h"\n#include "FRResearch.h"\n#include "EnhancedInputComponent.h"\n#include "InputAction.h"', 1)
    old = "		if (Event.GetKey() != FRTerminalKey() || Event.IsRepeat()) return false;"
    new = '''		// With the Enhanced Input action the game handles the key in play; we only catch it while our
		// UI-only modal is open (game input is suspended there) so the same key also closes it.
		if (Local && Local->bEnhancedInput && Local->CurrentScreen() == EFRScreen::None) return false;
		if (Event.GetKey() != FRTerminalKey() || Event.IsRepeat()) return false;'''
    s = s.replace(must(s, old), new)
    old = '''	InstallOverlay();'''
    new = '''	InstallOverlay();
	if (!bEnhancedInput && FRInput::Install(PC))
	{
		if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PC->InputComponent))
		{
			Input->BindAction(FRInput::Action(), ETriggerEvent::Started, this, &AFRLocalSubsystem::OnTerminalAction);
			bEnhancedInput = true;
			SyncKey();
		}
	}'''
    s = s.replace(must(s, old), new, 1)
    old = '''		UE_LOG(LogFRUI, Display, TEXT("FICSIT Roles UI synchronized (phase=%d)"), (int32)R->Phase);'''
    s = s.replace(must(s, old), old + "\n		FRResearch::FillContent(); // client copy of the MAM nodes: names, costs, icons", 1)
    s = s.replace(must(s, "void AFRLocalSubsystem::Later()"), '''void AFRLocalSubsystem::OnTerminalAction()
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

void AFRLocalSubsystem::Later()''', 1)
    s = s.replace(must(s, "void AFRLocalSubsystem::ToggleTerminal()\n{"), "void AFRLocalSubsystem::ToggleTerminal()\n{\n	SyncKey();", 1)
    return s


rw("Private/UI/FRLocalSubsystem.cpp", local)


def screens(s):
    s = s.replace(must(s, '#include "UI/FRNames.h"'), '#include "UI/FRNames.h"\n#include "FRResearch.h"', 1)
    # tech card: researched in the MAM when MAM trees are active
    old = "			: bOpen\n				? StaticCastSharedRef<SWidget>(SNew(SBox).HeightOverride(44)"
    new = "			: (bOpen && FRResearch::IsActive())\n				? StaticCastSharedRef<SWidget>(Label(FFRLoc::T(TEXT(\"tech.in_mam\")), 14, Orange, true, true))\n			: bOpen\n				? StaticCastSharedRef<SWidget>(SNew(SBox).HeightOverride(44)"
    s = s.replace(must(s, old), new)
    # key row: official menu hint when the action is registered
    old = '''				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).HeightOverride(46)
					[
						SNew(SButton).ButtonStyle(&SecondaryButton())'''
    new = '''				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[ SNew(STextBlock).Font(Font(14)).ColorAndOpacity(FSlateColor(Muted)).Text(FFRLoc::T(TEXT("key.official")))
					.Visibility_Lambda([this] { return L() && L()->bEnhancedInput ? EVisibility::Visible : EVisibility::Collapsed; }) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).HeightOverride(46).Visibility_Lambda([this] { return L() && L()->bEnhancedInput ? EVisibility::Collapsed : EVisibility::Visible; })
					[
						SNew(SButton).ButtonStyle(&SecondaryButton())'''
    s = s.replace(must(s, old), new)
    return s


rw("Private/UI/FRScreens.cpp", screens)
print("patched v9")
