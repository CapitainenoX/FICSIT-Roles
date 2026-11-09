"""Rebindable terminal key, saved per user (Saved/FICSITRoles/UserSettings.json), editable from F7."""
import os
import re

ROOT = r"F:\FICSIT-Roles-Rebuild\Mod\FICSITRoles\Source\FICSITRoles"


def rw(rel, fn):
    p = os.path.join(ROOT, rel)
    s = open(p, encoding="utf-8").read()
    open(p, "w", encoding="utf-8").write(fn(s))


def must(s, a):
    assert a in s, a[:90]
    return a


rw("Public/FRConfig.h", lambda s: s.replace(must(s, '#include "Containers/IndirectArray.h"'), '#include "Containers/IndirectArray.h"\n#include "InputCoreTypes.h"') + '''
/** Terminal hotkey: per-user override (Saved/FICSITRoles/UserSettings.json), else config default. */
FICSITROLES_API FKey FRTerminalKey();
FICSITROLES_API void FRSetTerminalKey(const FKey& Key);
FICSITROLES_API FText FRTerminalKeyName();
''')


def cfg(s):
    return s + r'''
// --- Per-user terminal key --------------------------------------------------------------------
namespace
{
	FString UserSettingsPath() { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FICSITRoles"), TEXT("UserSettings.json")); }
	FKey& CachedKey() { static FKey Key; return Key; }
}

FKey FRTerminalKey()
{
	FKey& Key = CachedKey();
	if (Key.IsValid()) return Key;
	FString Text;
	TSharedPtr<FJsonObject> Root;
	FString Name;
	if (FFileHelper::LoadFileToString(Text, *UserSettingsPath()) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid())
		Root->TryGetStringField(TEXT("terminalKey"), Name);
	Key = FKey(*(Name.IsEmpty() ? FFRConfig::Get().TerminalKey : Name));
	if (!Key.IsValid()) Key = EKeys::F7;
	return Key;
}

void FRSetTerminalKey(const FKey& Key)
{
	if (!Key.IsValid()) return;
	CachedKey() = Key;
	const FString Json = FString::Printf(TEXT("{\n  \"terminalKey\": \"%s\"\n}\n"), *Key.GetFName().ToString());
	FFileHelper::SaveStringToFile(Json, *UserSettingsPath());
}

FText FRTerminalKeyName()
{
	return FRTerminalKey().GetDisplayName();
}
'''


rw("Private/FRConfig.cpp", cfg)


def generic(s):
    s = s.replace("FText::FromString(FFRConfig::Get().TerminalKey)", "FRTerminalKeyName()")
    s = s.replace("FText::FromString(Config.TerminalKey)", "FRTerminalKeyName()")
    s = s.replace("FText::FromString(TerminalKeyLabel())", "FRTerminalKeyName()")
    return s


for f in ("Private/FRModule.cpp", "Private/UI/FRLocalSubsystem.cpp", "Private/UI/FRScreens.cpp"):
    rw(f, generic)


def local(s):
    s = s.replace(must(s, "FString AFRLocalSubsystem::TerminalKeyLabel() const { return FFRConfig::Get().TerminalKey; }"),
                  "FString AFRLocalSubsystem::TerminalKeyLabel() const { return FRTerminalKeyName().ToString(); }")
    old = '''	explicit FFRInputProcessor(AFRLocalSubsystem* InOwner) : Owner(InOwner)
	{
		Key = FKey(*FFRConfig::Get().TerminalKey);
		if (!Key.IsValid()) Key = EKeys::F7;
	}'''
    s = s.replace(must(s, old), "	explicit FFRInputProcessor(AFRLocalSubsystem* InOwner) : Owner(InOwner) {}")
    old = '''		if (Event.GetKey() != Key || Event.IsRepeat()) return false;
		AFRLocalSubsystem* Local = Owner.Get();'''
    new = '''		AFRLocalSubsystem* Local = Owner.Get();
		// Rebinding: the next key pressed becomes the terminal key (Escape cancels).
		if (Local && Local->bCapturingKey)
		{
			if (Event.IsRepeat()) return true;
			Local->FinishKeyCapture(Event.GetKey());
			return true;
		}
		if (Event.GetKey() != FRTerminalKey() || Event.IsRepeat()) return false;'''
    s = s.replace(must(s, old), new)
    s = s.replace(must(s, "	TWeakObjectPtr<AFRLocalSubsystem> Owner;\n	FKey Key;"), "	TWeakObjectPtr<AFRLocalSubsystem> Owner;")
    s = s.replace(must(s, "void AFRLocalSubsystem::Later()"), '''void AFRLocalSubsystem::FinishKeyCapture(const FKey& Key)
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

void AFRLocalSubsystem::Later()''')
    s = s.replace(must(s, '#include "FRRolesSubsystem.h"'), '#include "FRRolesSubsystem.h"\n#include "FRWorldModule.h"', 1)
    # Escape pressed while capturing must not close the terminal
    s = s.replace(must(s, "void AFRLocalSubsystem::OnEscape()\n{"), "void AFRLocalSubsystem::OnEscape()\n{\n	if (bCapturingKey) { FinishKeyCapture(EKeys::Escape); return; }")
    return s


rw("Private/UI/FRLocalSubsystem.cpp", local)
rw("Public/UI/FRLocalSubsystem.h", lambda s: s.replace(must(s, "	bool bAwaitingServer = false;"),
   "	bool bAwaitingServer = false;\n	/** True while the terminal waits for the new hotkey. */\n	bool bCapturingKey = false;\n	void FinishKeyCapture(const FKey& Key);"))


def screens(s):
    # live HUD hint
    s = s.replace('.Text(FText::Format(FText::FromString(TEXT("[{0}]")), FRTerminalKeyName()))',
                  '.Text_Lambda([] { return FText::Format(FText::FromString(TEXT("[{0}]")), FRTerminalKeyName()); })')
    # key setting row in the overview tab, under the team
    old = '''			+ SVerticalBox::Slot().AutoHeight().Padding(0, 30, 0, 10)[ Label(FFRLoc::T(TEXT("term.team")).ToUpper(), 15, Orange, true) ]
			+ SVerticalBox::Slot().AutoHeight()[ Team ];'''
    new = '''			+ SVerticalBox::Slot().AutoHeight().Padding(0, 30, 0, 10)[ Label(FFRLoc::T(TEXT("term.team")).ToUpper(), 15, Orange, true) ]
			+ SVerticalBox::Slot().AutoHeight()[ Team ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 26, 0, 0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)[ Label(FFRLoc::T(TEXT("key.label")).ToUpper(), 15, Orange, true) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)
				[ SNew(STextBlock).Font(Font(18, true)).ColorAndOpacity(FSlateColor(FRStyle::Text)).Text_Lambda([this] {
					return L() && L()->bCapturingKey ? FFRLoc::T(TEXT("key.press")) : FText::Format(FText::FromString(TEXT("[ {0} ]")), FRTerminalKeyName()); }) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).HeightOverride(46)
					[
						SNew(SButton).ButtonStyle(&SecondaryButton()).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.OnClicked_Lambda([this] { if (L()) L()->bCapturingKey = !L()->bCapturingKey; return FReply::Handled(); })
						[ SNew(STextBlock).Font(Font(15, true)).ColorAndOpacity(FSlateColor(FRStyle::Text)).Text_Lambda([this] {
							return FFRLoc::T(L() && L()->bCapturingKey ? TEXT("key.cancel") : TEXT("key.change")).ToUpper(); }) ]
					]
				]
			];'''
    s = s.replace(must(s, old), new)
    return s


rw("Private/UI/FRScreens.cpp", screens)
print("patched v7")
