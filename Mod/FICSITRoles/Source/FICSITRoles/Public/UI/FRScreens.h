// Native Slate screens of FICSIT Roles. Built in code (no cooked assets), localized at runtime.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBox.h"
#include "UI/FRLocalSubsystem.h"

class SVerticalBox;

/** Base for screens hosted by SFRRoot: Refresh() is called after every replicated state change. */
class SFRScreen : public SCompoundWidget
{
public:
	virtual void Refresh() {}
	virtual FText Title() const = 0;
	virtual FText Subtitle() const { return FText::GetEmpty(); }
};

/** Modal frame: backdrop, chamfered panel, header and the active screen. */
class SFRRoot : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFRRoot) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AFRLocalSubsystem>, Local)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetScreen(EFRScreen Screen);
	void Refresh();

private:
	TWeakObjectPtr<AFRLocalSubsystem> Local;
	TSharedPtr<SBox> Content;
	TSharedPtr<SFRScreen> Active;
	EFRScreen Current = EFRScreen::None;
	double OpenedAt = 0.0;
};

/** Non-interactive HUD layer: division badge and toast stack. */
class SFROverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFROverlay) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AFRLocalSubsystem>, Local)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void PushToast(const FText& Text, uint8 Severity, FName Division);
	virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override;

private:
	struct FToast { FText Text; uint8 Severity = 0; FName Division; double Born = 0.0; TSharedPtr<SWidget> Widget; };
	TWeakObjectPtr<AFRLocalSubsystem> Local;
	TSharedPtr<SVerticalBox> Stack;
	TArray<TSharedPtr<FToast>> Toasts;
	void RebuildStack();
};
