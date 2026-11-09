"""F7 terminal: overview + tech tree tabs; footer back button inset; explain() knows nodes/items."""
import os

ROOT = r"F:\FICSIT-Roles-Rebuild\Mod\FICSITRoles\Source\FICSITRoles"


def rw(rel, fn):
    p = os.path.join(ROOT, rel)
    s = open(p, encoding="utf-8").read()
    open(p, "w", encoding="utf-8").write(fn(s))


def must(s, a):
    assert a in s, a[:90]
    return a


TERMINAL = r'''// =============================================================================================
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
		auto Tab = [this](int32 Index, const TCHAR* Key) {
			return SNew(SButton).ButtonStyle(&FlatButton()).OnClicked_Lambda([this, Index] { Page = Index; Refresh(); return FReply::Handled(); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(18, 0)[ SNew(STextBlock).Text(FFRLoc::T(Key).ToUpper()).Font(Font(20, true, true)).ColorAndOpacity_Lambda([this, Index] { return FSlateColor(Page == Index ? FRStyle::Text : Muted); }) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(18, 6, 18, 0)[ SNew(SBox).HeightOverride(3)[ SNew(SImage).Image(White()).ColorAndOpacity_Lambda([this, Index] { return FSlateColor(Page == Index ? Orange : FLinearColor::Transparent); }) ] ]
				];
		};
		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 18)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[ Tab(0, TEXT("term.tab.overview")) ]
				+ SHorizontalBox::Slot().AutoWidth()[ Tab(1, TEXT("term.tab.tech")) ]
			]
			+ SVerticalBox::Slot().FillHeight(1).VAlign(VAlign_Center)[ SAssignNew(Body, SBox) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 0)
			[ Footer(FFRLoc::T(TEXT("ui.close")), SNullWidget::NullWidget, FFRLoc::F(TEXT("term.key_hint"), FText::FromString(FFRConfig::Get().TerminalKey))) ]
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
		Body->SetContent(Page == 0 ? Overview(*Roles, *Self) : TechTree(*Roles, *Self));
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
			+ SVerticalBox::Slot().AutoHeight()[ Team ];
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
			: bOpen
				? StaticCastSharedRef<SWidget>(SNew(SBox).HeightOverride(44)
					[
						SNew(SButton).ButtonStyle(&PrimaryButton()).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.IsEnabled_Lambda([this, Id] { const FFRTechNode* X = FFRConfig::Get().FindNode(Id); return X && Affordable(*X) && !Waiting(); })
						.OnClicked_Lambda([this, Id] { if (UFRRemote* Remote = RCO()) { Sent(); Remote->Server_Research(Id); } return FReply::Handled(); })
						[ SNew(STextBlock).Text(FFRLoc::T(TEXT("tech.unlock")).ToUpper()).Font(Font(16, true, true)).ColorAndOpacity(FSlateColor(FRStyle::Ink)) ]
					])
				: StaticCastSharedRef<SWidget>(Label(FFRLoc::T(TEXT("tech.locked")), 13, Muted, true));
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

'''


def screens(s):
    s = s.replace(must(s, '#include "FRResearch.h"\n'), '#include "UI/FRNames.h"\n#include "FGCharacterPlayer.h"\n#include "FGInventoryComponent.h"\n#include "Widgets/Layout/SScrollBox.h"\n')
    a = s.index("// =============================================================================================\n// F7: my division at a glance")
    b = s.index("// =============================================================================================\n// Root: dim backdrop")
    s = s[:a] + TERMINAL + s[b:]
    # back button: not glued to the panel edge
    s = s.replace(must(s, "			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center)[ Back(BackText, FOnClicked::CreateLambda([this] { return LaterClicked(); })) ]"),
                  "			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(36, 0, 0, 0)[ Back(BackText, FOnClicked::CreateLambda([this] { return LaterClicked(); })) ]")
    return s


rw("Private/UI/FRScreens.cpp", screens)


def local(s):
    s = s.replace(must(s, '#include "FRRolesSubsystem.h"'), '#include "FRRolesSubsystem.h"\n#include "UI/FRNames.h"')
    s = s.replace(must(s, "		if (Config.FindClearance(*Arg)) return FFRLoc::T(FString::Printf(TEXT(\"clr.%s.name\"), *Arg));"),
                  "		if (Config.FindNode(*Arg)) return FRNames::Node(*Arg);\n		if (Arg.StartsWith(TEXT(\"Desc_\")) || Arg.StartsWith(TEXT(\"BP_\"))) return FRNames::Item(Arg);")
    a = s.index('	if (Key == TEXT("deny.clearance"))')
    b = s.index('	if (Key == TEXT("log.coverage"))')
    s = s[:a] + '''	if (Key == TEXT("deny.tech"))
	{
		const FFRTechNode* N = FFRConfig::Get().FindNode(*A);
		return FFRLoc::F(Key, FRNames::Node(*A), FText::FromString(FFRConfig::Get().TerminalKey));
	}
''' + s[b:]
    return s


rw("Private/UI/FRLocalSubsystem.cpp", local)
print("patched v4")
