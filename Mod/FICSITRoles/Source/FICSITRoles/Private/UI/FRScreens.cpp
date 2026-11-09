// FICSIT Roles - native Slate screens (Satisfactory-style: metal panel, big cards, orange CTA, little text).
#include "UI/FRScreens.h"
#include "UI/FRStyle.h"
#include "FRConfig.h"
#include "FRLoc.h"
#include "FRRemote.h"
#include "UI/FRNames.h"
#include "FRResearch.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "Widgets/Layout/SScrollBox.h"
#include "FRRolesSubsystem.h"
#include "FGPlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"

// Palette names (Text, Line...) are short on purpose; local parameters may shadow them.
#pragma warning(disable : 4459 4458 4456)
using namespace FRStyle;

namespace FRUI
{
	static double Now() { return FSlateApplication::Get().GetCurrentTime(); }
	static float EaseOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - FMath::Pow(1.f - T, 3.f); }

	static TSharedRef<STextBlock> Label(const TAttribute<FText>& Txt, int32 Size, const FLinearColor& Color = FRStyle::Text, bool bBold = false, bool bWrap = false, bool bCondensed = false)
	{
		return SNew(STextBlock).Text(Txt).Font(Font(Size, bBold, bCondensed)).ColorAndOpacity(FSlateColor(Color)).AutoWrapText(bWrap);
	}

	static TSharedRef<SWidget> Icon(const FSlateBrush* Brush, float Size, const FLinearColor& Tint = FLinearColor::White)
	{
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)[ SNew(SImage).Image(Brush).ColorAndOpacity(FSlateColor(Tint)) ];
	}

	static TSharedRef<SWidget> Bar(float W, float H, const FLinearColor& C)
	{
		return SNew(SBox).WidthOverride(W).HeightOverride(H)[ SNew(SImage).Image(White()).ColorAndOpacity(FSlateColor(C)) ];
	}

	/** Image if the asset exists, else the fallback brush. */
	static const FSlateBrush* Pick(const TCHAR* File, const FSlateBrush* Fallback) { return HasImage(File) ? Image(File) : Fallback; }

	/** Big orange CTA with check mark (reference: CONFIRMER). */
	static TSharedRef<SWidget> Confirm(const TAttribute<FText>& Txt, FOnClicked OnClicked, const TAttribute<bool>& Enabled)
	{
		const bool bCheck = HasImage(TEXT("icon_check.png"));
		return SNew(SBox).WidthOverride(440).HeightOverride(76)
		[
			SNew(SButton).ButtonStyle(&PrimaryButton()).OnClicked(OnClicked).IsEnabled(Enabled).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, bCheck ? 14 : 0, 0)[ Icon(Pick(TEXT("icon_check.png"), White()), bCheck ? 30 : 0, FRStyle::Ink) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(STextBlock).Text(Txt).Font(Font(26, true, true)).ColorAndOpacity(FSlateColor(FRStyle::Ink)) ]
			]
		];
	}

	/** Dark "Retour" button with chevron. */
	static TSharedRef<SWidget> Back(const FText& Txt, FOnClicked OnClicked)
	{
		const bool bChevron = HasImage(TEXT("icon_back.png"));
		return SNew(SBox).MinDesiredWidth(190).HeightOverride(58)
		[
			SNew(SButton).ButtonStyle(&SecondaryButton()).OnClicked(OnClicked).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, bChevron ? 12 : 0, 0)[ Icon(Pick(TEXT("icon_back.png"), White()), bChevron ? 22 : 0) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(STextBlock).Text(Txt).Font(Font(18, true)).ColorAndOpacity(FSlateColor(FRStyle::Text)) ]
			]
		];
	}
}
using namespace FRUI;

// =============================================================================================
// SFRCard: card with entrance, hover lift, press, and idle/selected frame images crossfading.
// =============================================================================================
class SFRCard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFRCard) : _Selected(false), _Delay(0.f), _FrameIdle(nullptr), _FrameSel(nullptr), _Accent(FRStyle::Orange), _FrameOnTop(false) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ATTRIBUTE(bool, Selected)
		SLATE_EVENT(FOnClicked, OnClicked)
		SLATE_ARGUMENT(float, Delay)
		SLATE_ARGUMENT(const FSlateBrush*, FrameIdle)
		SLATE_ARGUMENT(const FSlateBrush*, FrameSel)
		SLATE_ARGUMENT(FLinearColor, Accent)
		SLATE_ARGUMENT(bool, FrameOnTop)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Selected = InArgs._Selected;
		Delay = InArgs._Delay;
		Born = Now();
		// Procedural fallback when the image frames are missing: chamfer frame tinted by state.
		const bool bImages = InArgs._FrameIdle && InArgs._FrameIdle->GetResourceObject() && InArgs._FrameSel && InArgs._FrameSel->GetResourceObject();
		TSharedRef<SWidget> Idle = bImages ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(InArgs._FrameIdle))
			: StaticCastSharedRef<SWidget>(SNew(SBorder).BorderImage(Frame()).BorderBackgroundColor(FSlateColor(FRStyle::Line)));
		TSharedRef<SWidget> Sel = bImages ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(InArgs._FrameSel))
			: StaticCastSharedRef<SWidget>(SNew(SBorder).BorderImage(Frame()).BorderBackgroundColor(FSlateColor(InArgs._Accent)));
		Idle->SetVisibility(EVisibility::HitTestInvisible);
		TSharedRef<SWidget> SelFade = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBorder")).Padding(0).Visibility(EVisibility::HitTestInvisible)
			.ColorAndOpacity_Lambda([this] { return FLinearColor(1, 1, 1, Selected.Get() ? 1.f : Hover * 0.55f); })[ Sel ];

		TSharedRef<SOverlay> Stack = SNew(SOverlay);
		if (!bImages && !InArgs._FrameOnTop) Stack->AddSlot()[ SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(FSlateColor(FRStyle::Panel2)) ];
		if (!InArgs._FrameOnTop) { Stack->AddSlot()[ Idle ]; Stack->AddSlot()[ SelFade ]; }
		Stack->AddSlot()[ InArgs._Content.Widget ];
		if (InArgs._FrameOnTop) { Stack->AddSlot()[ Idle ]; Stack->AddSlot()[ SelFade ]; }

		ChildSlot
		[
			SAssignNew(Button, SButton).ButtonStyle(&FlatButton()).ContentPadding(0).OnClicked(InArgs._OnClicked)[ Stack ]
		];
		SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		SetRenderTransform(TAttribute<TOptional<FSlateRenderTransform>>::Create(TAttribute<TOptional<FSlateRenderTransform>>::FGetter::CreateRaw(this, &SFRCard::Transform)));
		SetColorAndOpacity(TAttribute<FLinearColor>::Create(TAttribute<FLinearColor>::FGetter::CreateRaw(this, &SFRCard::Opacity)));
	}

	virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override
	{
		SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
		Hover = FMath::FInterpTo(Hover, (IsHovered() && IsEnabled()) ? 1.f : 0.f, DeltaTime, 14.f);
	}

private:
	TAttribute<bool> Selected;
	TSharedPtr<SButton> Button;
	float Delay = 0.f;
	double Born = 0.0;
	float Hover = 0.f;

	float Entrance() const { return EaseOut(float((Now() - Born - Delay) / 0.24)); }
	TOptional<FSlateRenderTransform> Transform() const
	{
		const float Scale = (Button.IsValid() && Button->IsPressed() ? 0.985f : 1.f) * (1.f + 0.012f * Hover);
		return FSlateRenderTransform(FScale2D(Scale), FVector2D(0.f, 16.f * (1.f - Entrance()) - 4.f * Hover));
	}
	FLinearColor Opacity() const { return FLinearColor(1, 1, 1, Entrance() * (IsEnabled() ? 1.f : 0.45f)); }
};

// =============================================================================================
// Screen base
// =============================================================================================
class SFRScreenBase : public SFRScreen
{
protected:
	TWeakObjectPtr<AFRLocalSubsystem> Local;
	AFRLocalSubsystem* L() const { return Local.Get(); }
	AFRRolesSubsystem* R() const { return Local.IsValid() ? Local->Roles() : nullptr; }
	UFRRemote* RCO() const { return Local.IsValid() ? Local->Remote() : nullptr; }
	const FFRPlayerRecord* Me() const { return Local.IsValid() ? Local->Me() : nullptr; }
	bool Admin() const { return Local.IsValid() && Local->IsAdmin(); }
	bool Waiting() const { return Local.IsValid() && Local->bAwaitingServer; }
	// Must be called BEFORE the RPC: on the host the RPC runs synchronously and its state change clears the flag.
	void Sent() const { if (Local.IsValid()) Local->MarkAwaiting(); }
	FReply LaterClicked() { if (L()) L()->Later(); return FReply::Handled(); }

	/** Footer like the references: back on the left, big CTA centred, status under the CTA. */
	TSharedRef<SWidget> Footer(const FText& BackText, TSharedRef<SWidget> Center, const TAttribute<FText>& Status)
	{
		return SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(36, 0, 0, 0)[ Back(BackText, FOnClicked::CreateLambda([this] { return LaterClicked(); })) ]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Center ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 8, 0, 0)[ SNew(STextBlock).Text(Status).Font(Font(14)).ColorAndOpacity(FSlateColor(Muted)) ]
			];
	}
};

// =============================================================================================
// Number of players (host)
// =============================================================================================
class SFRSetupScreen : public SFRScreenBase
{
public:
	SLATE_BEGIN_ARGS(SFRSetupScreen) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AFRLocalSubsystem>, Local)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Local = InArgs._Local;
		const AFRRolesSubsystem* Roles = R();
		Choice = Roles ? FMath::Clamp(Roles->ExpectedPlayers, 1, 4) : 1;
		const FSlateBrush* FrameIdle = Image(TEXT("card_tall.png"));
		const FSlateBrush* FrameSel = Image(TEXT("card_tall_sel.png"));
		const bool bPerson = HasImage(TEXT("icon_person.png"));

		TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(14, 0));
		for (int32 N = 1; N <= 4; ++N)
		{
			TSharedRef<SHorizontalBox> People = SNew(SHorizontalBox);
			for (int32 k = 0; k < N && bPerson; ++k)
				People->AddSlot().AutoWidth().Padding(k == 0 ? 0 : -6, 0, 0, 0)[ Icon(Image(TEXT("icon_person.png")), 34, FLinearColor(0.85f, 0.85f, 0.85f)) ];
			Grid->AddSlot(N - 1, 0)
			[
				SNew(SBox).WidthOverride(300).HeightOverride(480)
				[
					SNew(SFRCard).Delay(0.06f * N).FrameIdle(FrameIdle).FrameSel(FrameSel).FrameOnTop(true)
					.Selected_Lambda([this, N] { return Choice == N; })
					.OnClicked_Lambda([this, N] { Choice = N; return FReply::Handled(); })
					[
						SNew(SOverlay)
						+ SOverlay::Slot().Padding(8)
						[ SNew(SScaleBox).Stretch(EStretch::ScaleToFill).Clipping(EWidgetClipping::ClipToBounds)[ SNew(SImage).Image(Image(FString::Printf(TEXT("players_%d.png"), N))) ] ]
						+ SOverlay::Slot().Padding(8)[ SNew(SImage).Image(Image(TEXT("shade.png"))) ]
						+ SOverlay::Slot().VAlign(VAlign_Top).HAlign(HAlign_Center).Padding(0, 26, 0, 0)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Label(FText::AsNumber(N), 92, FRStyle::Text, true, false, true) ]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Label(FFRLoc::T(N == 1 ? TEXT("setup.player") : TEXT("setup.players")).ToUpper(), 22, FLinearColor(0.82f, 0.82f, 0.82f), true) ]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 14, 0, 0)[ People ]
						]
					]
				]
			];
		}

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Center).VAlign(VAlign_Center)[ Grid ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 24, 0, 0)
			[
				Footer(FFRLoc::T(TEXT("ui.back")),
					Confirm(FFRLoc::T(TEXT("ui.confirm")).ToUpper(), FOnClicked::CreateLambda([this] {
						if (UFRRemote* Remote = RCO()) { Sent(); Remote->Server_Configure(Choice); }
						return FReply::Handled();
					}), TAttribute<bool>::CreateLambda([this] { return !Waiting(); })),
					TAttribute<FText>::CreateLambda([this] { return Waiting() ? FFRLoc::T(TEXT("ui.sync_pending")) : FText::GetEmpty(); }))
			]
		];
	}

	virtual FText Title() const override { return FFRLoc::T(TEXT("setup.title")); }
	virtual FText Subtitle() const override { return FFRLoc::T(TEXT("setup.subtitle")); }

private:
	int32 Choice = 1;
};

// =============================================================================================
// Division choice (once, server-confirmed, locked afterwards)
// =============================================================================================
class SFRDivisionScreen : public SFRScreenBase
{
public:
	SLATE_BEGIN_ARGS(SFRDivisionScreen) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AFRLocalSubsystem>, Local)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Local = InArgs._Local;
		const FFRConfig& Config = FFRConfig::Get();
		const FSlateBrush* FrameIdle = Image(TEXT("card_wide.png"));
		const FSlateBrush* FrameSel = Image(TEXT("card_wide_sel.png"));
		// Generalist (or the odd one out) alone on top, centred; specialists in a 2-column grid below.
		TArray<FName> Order;
		for (const FFRDivisionDef& D : Config.Divisions) Order.Add(D.Id);
		FName TopId;
		if (Order.Num() % 2 == 1)
		{
			TopId = Order.Contains(FName(TEXT("general"))) ? FName(TEXT("general")) : Order.Last();
			Order.Remove(TopId);
		}
		auto MakeCard = [this, &Config, FrameIdle, FrameSel](FName Id, int32 Index) -> TSharedRef<SWidget>
		{
			const FFRDivisionDef* Def = Config.FindDivision(Id);
			return SNew(SBox).WidthOverride(560).HeightOverride(150)
			[
				SNew(SFRCard).Delay(0.05f * Index).FrameIdle(FrameIdle).FrameSel(FrameSel).Accent(Def ? Def->Color : Orange)
				.Selected_Lambda([this, Id] { return Shown() == Id; })
				.IsEnabled_Lambda([this, Id] { return CanPick(Id) || Mine() == Id; })
				.OnClicked_Lambda([this, Id] { if (Mine().IsNone()) Selected = Id; return FReply::Handled(); })
				[
					SNew(SOverlay)
					+ SOverlay::Slot().VAlign(VAlign_Center).Padding(40, 0, 36, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Icon(DivisionIcon(Id), 84) ]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(26, 0)[ Bar(4, 74, Orange) ]
						+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
						[
							// Long names (other languages) shrink instead of being clipped.
							SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly).HAlign(HAlign_Left)
							[ Label(FFRLoc::T(FString::Printf(TEXT("div.%s.short"), *Id.ToString())).ToUpper(), 36, FRStyle::Text, true, false, true) ]
						]
					]
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(40, 0, 0, 14)
					[ SNew(STextBlock).Font(Font(12, true)).Text_Lambda([this, Id] { return Badge(Id); }).ColorAndOpacity_Lambda([this, Id] { return FSlateColor(Mine() == Id ? Orange : Muted); }) ]
				]
			];
		};
		TSharedRef<SVerticalBox> Grid = SNew(SVerticalBox);
		int32 Index = 0;
		if (!TopId.IsNone()) Grid->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 14)[ MakeCard(TopId, Index++) ];
		for (int32 i = 0; i < Order.Num(); i += 2)
		{
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
			Row->AddSlot().AutoWidth().Padding(12, 0)[ MakeCard(Order[i], Index++) ];
			if (Order.IsValidIndex(i + 1)) Row->AddSlot().AutoWidth().Padding(12, 0)[ MakeCard(Order[i + 1], Index++) ];
			Grid->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 14)[ Row ];
		}

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Center).VAlign(VAlign_Center)[ Grid ]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 2, 0, 0)
			[ SNew(STextBlock).Font(Font(17)).ColorAndOpacity(FSlateColor(Muted)).Text_Lambda([this] {
				const FName Id = Shown();
				return Id.IsNone() ? FFRLoc::T(TEXT("division.hint")) : FFRLoc::T(FString::Printf(TEXT("div.%s.domain"), *Id.ToString()));
			}) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
			[
				Footer(FFRLoc::T(TEXT("ui.back")),
					SNew(SBox).Visibility_Lambda([this] { return Mine().IsNone() ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						Confirm(FFRLoc::T(TEXT("ui.confirm")).ToUpper(), FOnClicked::CreateLambda([this] {
							if (UFRRemote* Remote = RCO()) { Sent(); Remote->Server_Choose(Selected); }
							return FReply::Handled();
						}), TAttribute<bool>::CreateLambda([this] { return !Selected.IsNone() && CanPick(Selected) && !Waiting() && CanChooseNow(); }))
					],
					TAttribute<FText>::CreateLambda([this] { return Status(); }))
			]
		];
	}

	virtual FText Title() const override { return Mine().IsNone() ? FFRLoc::T(TEXT("division.title")) : FFRLoc::T(TEXT("division.waiting_title")); }
	virtual FText Subtitle() const override { return FFRLoc::T(TEXT("division.subtitle")); }
	virtual void Refresh() override { if (!Selected.IsNone() && !CanPick(Selected)) Selected = NAME_None; }

private:
	FName Selected;

	FName Mine() const { const FFRPlayerRecord* M = Me(); return M ? M->Division : NAME_None; }
	FName Shown() const { return Mine().IsNone() ? Selected : Mine(); }
	bool CanChooseNow() const
	{
		const AFRRolesSubsystem* Roles = R();
		return Roles && (Roles->Phase != EFRPhase::Setup || (Admin() && Roles->ExpectedPlayers <= 1));
	}
	bool CanPick(FName Id) const
	{
		const AFRRolesSubsystem* Roles = R();
		return Roles && Mine().IsNone() && Roles->MembersOf(Id) < Roles->Capacity();
	}
	FText Badge(FName Id) const
	{
		const AFRRolesSubsystem* Roles = R();
		if (!Roles) return FText::GetEmpty();
		if (Mine() == Id) return FFRLoc::T(TEXT("division.yours")).ToUpper();
		const FFRPlayerRecord* Holder = Roles->Players.FindByPredicate([Id](const FFRPlayerRecord& P) { return P.Division == Id; });
		if (Holder && Roles->MembersOf(Id) >= Roles->Capacity()) return FFRLoc::F(TEXT("division.taken_by"), FText::FromString(Holder->Name)).ToUpper();
		return FText::GetEmpty();
	}
	FText Status() const
	{
		const AFRRolesSubsystem* Roles = R();
		if (!Roles) return FFRLoc::T(TEXT("deny.syncing"));
		if (Waiting()) return FFRLoc::T(TEXT("ui.sync_pending"));
		if (!CanChooseNow()) return FFRLoc::T(TEXT("lobby.wait_setup"));
		if (!Mine().IsNone())
		{
			int32 Chosen = 0;
			for (const FFRPlayerRecord& P : Roles->Players) Chosen += P.Division.IsNone() ? 0 : 1;
			return FFRLoc::F(TEXT("division.waiting"), FText::AsNumber(Chosen), FText::AsNumber(Roles->ExpectedPlayers));
		}
		return FFRLoc::T(TEXT("division.locked_note"));
	}
};

// =============================================================================================
// F7: my division (read-only) + division tech tree
// =============================================================================================
class SFRTerminalScreen : public SFRScreenBase
{
public:
	SLATE_BEGIN_ARGS(SFRTerminalScreen) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AFRLocalSubsystem>, Local)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Local = InArgs._Local;
		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 18)
			[
				// Technology tab only when division research is not in the MAM (stable mode).
				SNew(SHorizontalBox).Visibility(FRResearch::IsActive() ? EVisibility::Collapsed : EVisibility::Visible)
				+ SHorizontalBox::Slot().AutoWidth().Padding(12, 0)
				[ SNew(SButton).ButtonStyle(&FlatButton()).OnClicked_Lambda([this] { Page = 0; Refresh(); return FReply::Handled(); })
					[ SNew(STextBlock).Text(FFRLoc::T(TEXT("term.tab.overview")).ToUpper()).Font(Font(20, true, true)).ColorAndOpacity_Lambda([this] { return FSlateColor(Page == 0 ? FRStyle::Text : Muted); }) ] ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(12, 0)
				[ SNew(SButton).ButtonStyle(&FlatButton()).OnClicked_Lambda([this] { Page = 1; Refresh(); return FReply::Handled(); })
					[ SNew(STextBlock).Text(FFRLoc::T(TEXT("term.tab.tech")).ToUpper()).Font(Font(20, true, true)).ColorAndOpacity_Lambda([this] { return FSlateColor(Page == 1 ? FRStyle::Text : Muted); }) ] ]
			]
			+ SVerticalBox::Slot().FillHeight(1).VAlign(VAlign_Center)[ SAssignNew(Body, SBox) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 0)
			[ Footer(FFRLoc::T(TEXT("ui.close")), SNullWidget::NullWidget, FFRLoc::F(TEXT("term.key_hint"), FRTerminalKeyName())) ]
		];
		Refresh();
	}

	virtual FText Title() const override { return FFRLoc::T(TEXT("term.title")); }
	virtual FText Subtitle() const override { const FFRPlayerRecord* M = Me(); return M ? FText::FromString(M->Name) : FText::GetEmpty(); }

	virtual void Refresh() override
	{
		const AFRRolesSubsystem* Roles = R();
		const FFRPlayerRecord* Self = Me();
		if (!Roles || !Self || !Body.IsValid()) return;
		Body->SetContent(Page == 1 && !FRResearch::IsActive() ? TechTree(*Roles, *Self) : Overview(*Roles, *Self));
	}

private:
	TSharedPtr<SBox> Body;
	int32 Page = 0;

	int32 Have(const FString& Item) const
	{
		const AFGPlayerController* PC = L() ? L()->Controller() : nullptr;
		const AFGCharacterPlayer* Character = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
		UClass* Class = FRFindItem(Item);
		return Character && Character->GetInventory() && Class ? Character->GetInventory()->GetNumItems(Class) : 0;
	}

	bool Affordable(const FFRTechNode& N) const
	{
		for (const TPair<FString, int32>& C : N.Cost) if (FRFindItem(C.Key) && Have(C.Key) < C.Value) return false;
		return true;
	}

	TSharedRef<SWidget> Overview(const AFRRolesSubsystem& Roles, const FFRPlayerRecord& Self)
	{
		const FName Div = Self.Division;
		const FLinearColor Color = DivisionColor(Div);
		TSharedRef<SVerticalBox> Can = SNew(SVerticalBox);
		TArray<FString> Lines;
		FFRLoc::Str(FString::Printf(TEXT("div.%s.can"), *Div.ToString())).ParseIntoArray(Lines, TEXT("|"));
		for (const FString& Line : Lines)
			Can->AddSlot().AutoHeight().Padding(0, 5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)[ Bar(10, 10, Orange) ]
				+ SHorizontalBox::Slot().FillWidth(1)[ Label(FText::FromString(Line.TrimStartAndEnd()), 17, FRStyle::Text, false, true) ]
			];
		int32 Done = 0, Total = 0;
		for (const FFRTechNode& N : FFRConfig::Get().Tech) if (N.Division == Div) { ++Total; Done += Roles.IsResearched(N.Id) ? 1 : 0; }
		TSharedRef<SWrapBox> Team = SNew(SWrapBox).UseAllottedSize(true);
		for (const FFRPlayerRecord& P : Roles.Players)
			Team->AddSlot().Padding(0, 0, 18, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[ Icon(P.Division.IsNone() ? Image(TEXT("logo_mark.png")) : DivisionIcon(P.Division), 26) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Label(FText::FromString(P.Name), 15, P.bOnline ? FRStyle::Text : Muted, true) ]
			];
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 36, 0)[ Icon(DivisionIcon(Div), 150) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 36, 0)[ Bar(4, 130, Orange) ]
				+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ Label(FFRLoc::Division(Div).ToUpper(), 46, Color, true, false, true) ]
					+ SVerticalBox::Slot().AutoHeight()[ Label(FFRLoc::T(FString::Printf(TEXT("div.%s.domain"), *Div.ToString())), 19, Muted, false, true) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)[ Label(FFRLoc::F(TEXT("tech.progress"), FText::AsNumber(Done), FText::AsNumber(Total)), 16, Orange, true) ]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 34, 0, 10)[ Label(FFRLoc::T(TEXT("div.abilities")).ToUpper(), 15, Orange, true) ]
			+ SVerticalBox::Slot().AutoHeight()[ Can ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 30, 0, 10)[ Label(FFRLoc::T(TEXT("term.team")).ToUpper(), 15, Orange, true) ]
			+ SVerticalBox::Slot().AutoHeight()[ Team ]
			;
	}

	TSharedRef<SWidget> NodeCard(const FFRTechNode& N, const FLinearColor& Color)
	{
		const FName Id = N.Id;
		TSharedRef<SVerticalBox> Costs = SNew(SVerticalBox);
		for (const TPair<FString, int32>& C : N.Cost)
		{
			const FString Item = C.Key;
			const int32 Need = C.Value;
			Costs->AddSlot().AutoHeight().Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[ SNew(SBox).WidthOverride(28).HeightOverride(28)[ SNew(SImage).Image(FRNames::ItemIcon(Item) ? FRNames::ItemIcon(Item) : White()).ColorAndOpacity(FSlateColor(FRNames::ItemIcon(Item) ? FLinearColor::White : FLinearColor::Transparent)) ] ]
				+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[ Label(FRNames::Item(Item), 13, Muted) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[ SNew(STextBlock).Font(Font(13, true)).Text_Lambda([this, Item, Need] { return FText::Format(FText::FromString(TEXT("{0}/{1}")), FText::AsNumber(FMath::Min(Have(Item), 99999)), FText::AsNumber(Need)); })
					.ColorAndOpacity_Lambda([this, Item, Need] { return FSlateColor(Have(Item) >= Need ? Ok : Danger); }) ]
			];
		}
		const AFRRolesSubsystem* Roles = R();
		const bool bDone = Roles && Roles->IsResearched(Id);
		const bool bOpen = Roles && Roles->IsNodeOpen(Id);
		const FLinearColor Edge = bDone ? Ok : bOpen ? Orange : FRStyle::Line;
		const FSlateBrush* NodeIcon = FRNames::NodeIcon(Id);
		TSharedRef<SWidget> Action = bDone
			? StaticCastSharedRef<SWidget>(Label(FFRLoc::T(TEXT("tech.done")).ToUpper(), 14, Ok, true))
			: (bOpen && FRResearch::IsActive())
				? StaticCastSharedRef<SWidget>(Label(FFRLoc::T(TEXT("tech.in_mam")), 14, Orange, true, true))
			: bOpen
				? StaticCastSharedRef<SWidget>(SNew(SBox).HeightOverride(44)
					[
						SNew(SButton).ButtonStyle(&PrimaryButton()).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.IsEnabled_Lambda([this, Id] { const FFRTechNode* X = FFRConfig::Get().FindNode(Id); return X && Affordable(*X) && !Waiting(); })
						.OnClicked_Lambda([this, Id] { if (UFRRemote* Remote = RCO()) { Sent(); Remote->Server_Research(Id); } return FReply::Handled(); })
						[ SNew(STextBlock).Text(FFRLoc::T(TEXT("tech.unlock")).ToUpper()).Font(Font(16, true, true)).ColorAndOpacity(FSlateColor(FRStyle::Ink)) ]
					])
				: StaticCastSharedRef<SWidget>(Label(FFRLoc::T(TEXT("tech.locked")), 13, Muted, true, true));
		return SNew(SBox).WidthOverride(300)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[ SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(FSlateColor(FRStyle::Panel2)) ]
			+ SOverlay::Slot()[ SNew(SBorder).BorderImage(Frame()).BorderBackgroundColor(FSlateColor(Edge)).Visibility(EVisibility::HitTestInvisible) ]
			+ SOverlay::Slot().Padding(16, 14)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, NodeIcon ? 10 : 0, 0)[ Icon(NodeIcon ? NodeIcon : White(), NodeIcon ? 40 : 0) ]
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[ Label(FRNames::Node(Id), 17, bDone ? Ok : FRStyle::Text, true, true) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 10)[ Costs ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)[ Action ]
			]
		];
	}

	TSharedRef<SWidget> TechTree(const AFRRolesSubsystem& Roles, const FFRPlayerRecord& Self)
	{
		const FName Div = Self.Division;
		int32 MaxTier = 0;
		for (const FFRTechNode& N : FFRConfig::Get().Tech) if (N.Division == Div) MaxTier = FMath::Max(MaxTier, N.Tier);
		if (MaxTier == 0) return Label(FFRLoc::T(TEXT("tech.none")), 18, Muted);
		TSharedRef<SHorizontalBox> Columns = SNew(SHorizontalBox);
		for (int32 Tier = 1; Tier <= MaxTier; ++Tier)
		{
			TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
			Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 12)[ Label(FText::Format(FText::FromString(TEXT("TIER {0}")), FText::AsNumber(Tier)), 16, Orange, true, false, true) ];
			for (const FFRTechNode& N : FFRConfig::Get().Tech)
				if (N.Division == Div && N.Tier == Tier) Col->AddSlot().AutoHeight().Padding(0, 0, 0, 14)[ NodeCard(N, DivisionColor(Div)) ];
			Columns->AddSlot().AutoWidth().Padding(10, 0)[ Col ];
		}
		return SNew(SScrollBox).Orientation(Orient_Horizontal) + SScrollBox::Slot()[ SNew(SScrollBox) + SScrollBox::Slot()[ Columns ] ];
	}
};

// =============================================================================================
// Root: dim backdrop + metal panel + centred header
// =============================================================================================
void SFRRoot::Construct(const FArguments& InArgs)
{
	Local = InArgs._Local;
	OpenedAt = Now();
	const bool bPanel = HasImage(TEXT("panel.png"));
	const bool bOrnament = HasImage(TEXT("header_ornament.png"));
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[ SNew(SImage).Image(White()).ColorAndOpacity_Lambda([this] { return FSlateColor(Backdrop * FLinearColor(1, 1, 1, EaseOut(float((Now() - OpenedAt) / 0.18)))); }) ]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(24)
		[
			SNew(SBox).WidthOverride(1520).HeightOverride(940)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					bPanel ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Sliced(TEXT("panel.png"), FMargin(110))))
					: StaticCastSharedRef<SWidget>(SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(FSlateColor(FRStyle::Panel)))
				]
				+ SOverlay::Slot().Padding(FMargin(70, 30, 70, 64))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[ SNew(SBox).WidthOverride(760).HeightOverride(64).Visibility(bOrnament ? EVisibility::HitTestInvisible : EVisibility::Collapsed)[ SNew(SImage).Image(Image(TEXT("header_ornament.png"))) ] ]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)
					[ SNew(STextBlock).Font(Font(54, true, true)).ColorAndOpacity(FSlateColor(FRStyle::Text)).Text_Lambda([this] { return Active.IsValid() ? Active->Title().ToUpper() : FText::GetEmpty(); }) ]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[ SNew(STextBlock).Font(Font(20)).ColorAndOpacity(FSlateColor(Muted)).Text_Lambda([this] { return Active.IsValid() ? Active->Subtitle() : FText::GetEmpty(); }) ]
					+ SVerticalBox::Slot().FillHeight(1).Padding(0, 24, 0, 0)[ SAssignNew(Content, SBox) ]
				]
			]
		]
	];
	SetColorAndOpacity(TAttribute<FLinearColor>::CreateLambda([this] { return FLinearColor(1, 1, 1, EaseOut(float((Now() - OpenedAt) / 0.16))); }));
}

void SFRRoot::SetScreen(EFRScreen Screen)
{
	if (Screen == Current && Active.IsValid()) { Active->Refresh(); return; }
	Current = Screen;
	switch (Screen)
	{
	case EFRScreen::Setup: Active = SNew(SFRSetupScreen).Local(Local); break;
	case EFRScreen::Division: Active = SNew(SFRDivisionScreen).Local(Local); break;
	case EFRScreen::Terminal: Active = SNew(SFRTerminalScreen).Local(Local); break;
	default: Active.Reset(); break;
	}
	if (!Content.IsValid()) return;
	if (Active.IsValid()) Content->SetContent(Active.ToSharedRef());
	else Content->SetContent(SNullWidget::NullWidget);
}

void SFRRoot::Refresh()
{
	if (Active.IsValid()) Active->Refresh();
}

// =============================================================================================
// Overlay: HUD badge + toasts
// =============================================================================================
void SFROverlay::Construct(const FArguments& InArgs)
{
	Local = InArgs._Local;
	SetVisibility(EVisibility::SelfHitTestInvisible);
	ChildSlot
	[
		SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
		// Role badge, vanilla HUD style: dark translucent plate, coloured accent bar, small caps label + value,
		// keycap hint. Sits below the game's build-version line (top-left).
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(20, 62)
		[
			SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(FSlateColor(FLinearColor(0.02f, 0.02f, 0.025f, 0.72f))).Padding(0)
			.Visibility_Lambda([this] { return Local.IsValid() && Local->IsSynced() && Local->CurrentScreen() == EFRScreen::None ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(3)
					[ SNew(SImage).Image(White()).ColorAndOpacity_Lambda([this] {
						const FFRPlayerRecord* Me = Local.IsValid() ? Local->Me() : nullptr;
						return FSlateColor(Me && !Me->Division.IsNone() ? DivisionColor(Me->Division) : Orange); }) ]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 6, 8, 6)
				[
					SNew(SBox).WidthOverride(28).HeightOverride(28)
					[ SNew(SImage).Image_Lambda([this]() -> const FSlateBrush* {
						const FFRPlayerRecord* Me = Local.IsValid() ? Local->Me() : nullptr;
						return Me && !Me->Division.IsNone() ? DivisionIcon(Me->Division) : Image(TEXT("logo_mark.png")); }) ]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 5, 0, 5)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[ SNew(STextBlock).Font(Font(9, true)).ColorAndOpacity(FSlateColor(Muted)).Text(FText::FromString(TEXT("FICSIT ROLES"))) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font(13, true))
						.Text_Lambda([this] {
							const FFRPlayerRecord* Me = Local.IsValid() ? Local->Me() : nullptr;
							if (!Me) return FText::GetEmpty();
							return Me->Division.IsNone() ? FFRLoc::F(TEXT("hud.no_division"), FRTerminalKeyName()) : FFRLoc::Division(Me->Division).ToUpper();
						})
						.ColorAndOpacity(FSlateColor(FRStyle::Text))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12, 0, 10, 0)
				[
					SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(FSlateColor(FLinearColor(1, 1, 1, 0.10f))).Padding(FMargin(6, 1))
					[ SNew(STextBlock).Font(Font(10, true)).ColorAndOpacity(FSlateColor(FRStyle::Text)).Text_Lambda([] { return FRTerminalKeyName(); }) ]
				]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0, 150, 28, 0)
		[ SNew(SBox).WidthOverride(420)[ SAssignNew(Stack, SVerticalBox) ] ]
	];
}

void SFROverlay::PushToast(const FText& Txt, uint8 Severity, FName Division)
{
	const double T = Now();
	for (const TSharedPtr<FToast>& Existing : Toasts)
		if (Existing->Text.EqualTo(Txt) && T - Existing->Born < 3.0) return; // de-duplicate bursts
	TSharedPtr<FToast> Toast = MakeShared<FToast>();
	Toast->Text = Txt; Toast->Severity = Severity; Toast->Division = Division; Toast->Born = T;
	const FLinearColor Accent = Severity == 2 ? Danger : Severity == 1 ? Ok : (Division.IsNone() ? Orange : DivisionColor(Division));
	const TWeakPtr<FToast> Weak = Toast;
	Toast->Widget = SNew(SBorder).BorderImage(Fill()).BorderBackgroundColor(FSlateColor(FLinearColor(0.02f, 0.02f, 0.025f, 0.85f))).Padding(0)
		.RenderTransform_Lambda([Weak]() -> TOptional<FSlateRenderTransform> {
			const TSharedPtr<FToast> P = Weak.Pin();
			const float In = P.IsValid() ? EaseOut(float((Now() - P->Born) / 0.18)) : 1.f;
			return FSlateRenderTransform(FVector2D(16.f * (1.f - In), 0.f));
		})
		.ColorAndOpacity_Lambda([Weak] {
			const TSharedPtr<FToast> P = Weak.Pin();
			if (!P.IsValid()) return FLinearColor(1, 1, 1, 1);
			const double Age = Now() - P->Born;
			const float In = EaseOut(float(Age / 0.18));
			const float Out = Age > 4.2 ? 1.f - float((Age - 4.2) / 0.3) : 1.f;
			return FLinearColor(1, 1, 1, FMath::Clamp(FMath::Min(In, Out), 0.f, 1.f));
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(3)[ SNew(SImage).Image(White()).ColorAndOpacity(FSlateColor(Accent)) ] ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12, 10, 0, 10)
			[ Icon(Division.IsNone() ? Image(TEXT("logo_mark.png")) : DivisionIcon(Division), 26) ]
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(12, 10, 14, 10)
			[ SNew(STextBlock).Text(Txt).Font(Font(13, true)).ColorAndOpacity(FSlateColor(FRStyle::Text)).AutoWrapText(true) ]
		];
	Toasts.Add(Toast);
	while (Toasts.Num() > 4) Toasts.RemoveAt(0);
	RebuildStack();
}

void SFROverlay::Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime)
{
	SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
	const double T = Now();
	const int32 Before = Toasts.Num();
	Toasts.RemoveAll([T](const TSharedPtr<FToast>& X) { return T - X->Born > 4.5; });
	if (Toasts.Num() != Before) RebuildStack();
}

void SFROverlay::RebuildStack()
{
	if (!Stack.IsValid()) return;
	Stack->ClearChildren();
	for (const TSharedPtr<FToast>& T : Toasts)
		Stack->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[ T->Widget.ToSharedRef() ];
}
