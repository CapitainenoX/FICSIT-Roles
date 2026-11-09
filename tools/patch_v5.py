"""Division screen layout: generalist alone on top (centred), specialists 2x2 below, names never clipped."""
import os

P = r"F:\FICSIT-Roles-Rebuild\Mod\FICSITRoles\Source\FICSITRoles\Private\UI\FRScreens.cpp"
s = open(P, encoding="utf-8").read()

a = s.index("		TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(14, 12));\n		const int32 Columns = Config.Divisions.Num() <= 4 ? 2 : 3;")
b = s.index("		ChildSlot", a)

NEW = r'''		// Generalist (or the odd one out) alone on top, centred; specialists in a 2-column grid below.
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

'''
s = s[:a] + NEW + s[b:]
# the hint line under the grid: tighter so the footer status is never cut
s = s.replace("			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 10, 0, 0)\n			[ SNew(STextBlock).Font(Font(18))",
              "			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 2, 0, 0)\n			[ SNew(STextBlock).Font(Font(17))")
s = s.replace("			+ SVerticalBox::Slot().AutoHeight().Padding(0, 22, 0, 0)\n			[\n				Footer(FFRLoc::T(TEXT(\"ui.back\")),\n					SNew(SBox).Visibility_Lambda",
              "			+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)\n			[\n				Footer(FFRLoc::T(TEXT(\"ui.back\")),\n					SNew(SBox).Visibility_Lambda")
open(P, "w", encoding="utf-8").write(s)
print("patched v5")
