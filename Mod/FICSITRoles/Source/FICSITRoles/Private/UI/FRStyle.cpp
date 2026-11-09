#include "UI/FRStyle.h"
#include "FRConfig.h"
#include "Styling/CoreStyle.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "UObject/StrongObjectPtr.h"

namespace FRStyle
{
	const FLinearColor Backdrop  = FLinearColor(FColor(8, 9, 10, 205));
	const FLinearColor Panel     = FLinearColor(FColor::FromHex(TEXT("16181BFF")));
	const FLinearColor Panel2    = FLinearColor(FColor::FromHex(TEXT("1F2226FF")));
	const FLinearColor Panel3    = FLinearColor(FColor::FromHex(TEXT("2A2E33FF")));
	const FLinearColor Line      = FLinearColor(FColor::FromHex(TEXT("3C4148FF")));
	const FLinearColor Text      = FLinearColor(FColor::FromHex(TEXT("ECEAE6FF")));
	const FLinearColor Muted     = FLinearColor(FColor::FromHex(TEXT("9BA1A8FF")));
	const FLinearColor Orange    = FLinearColor(FColor::FromHex(TEXT("F49A2AFF")));
	const FLinearColor OrangeHot = FLinearColor(FColor::FromHex(TEXT("FFB24DFF")));
	const FLinearColor Ok        = FLinearColor(FColor::FromHex(TEXT("5CC27AFF")));
	const FLinearColor Danger    = FLinearColor(FColor::FromHex(TEXT("E0533CFF")));
	const FLinearColor Ink       = FLinearColor(FColor::FromHex(TEXT("15120EFF")));

	namespace
	{
		struct FImageEntry
		{
			TStrongObjectPtr<UTexture2D> Texture;
			TSharedPtr<FSlateBrush> Brush;
		};
		TMap<FString, FImageEntry>& Images() { static TMap<FString, FImageEntry> Map; return Map; }

		FSlateBrush MakeBrush(const FLinearColor& Color, ESlateBrushDrawType::Type DrawAs = ESlateBrushDrawType::Image)
		{
			FSlateBrush B;
			B.DrawAs = DrawAs;
			B.TintColor = FSlateColor(Color);
			return B;
		}

		const FSlateBrush* NineSlice(const FString& File, float Margin)
		{
			static TMap<FString, TSharedPtr<FSlateBrush>> Cache;
			if (const TSharedPtr<FSlateBrush>* Found = Cache.Find(File)) return Found->Get();
			const FSlateBrush* Source = Image(File);
			TSharedPtr<FSlateBrush> B = MakeShared<FSlateBrush>(*Source);
			if (B->GetResourceObject())
			{
				B->DrawAs = ESlateBrushDrawType::Box;
				B->Margin = FMargin(Margin);
			}
			else
			{
				*B = MakeBrush(FLinearColor::White); // fallback: flat rectangle
			}
			Cache.Add(File, B);
			return B.Get();
		}

		FButtonStyle MakeButton(const FLinearColor& Normal, const FLinearColor& Hover, const FLinearColor& Pressed, const FLinearColor& Disabled)
		{
			FButtonStyle S;
			auto Tinted = [](const FLinearColor& C) { FSlateBrush B = *Fill(); B.TintColor = FSlateColor(C); return B; };
			S.SetNormal(Tinted(Normal));
			S.SetHovered(Tinted(Hover));
			S.SetPressed(Tinted(Pressed));
			S.SetDisabled(Tinted(Disabled));
			S.SetNormalPadding(FMargin(18, 10, 18, 10));
			S.SetPressedPadding(FMargin(18, 11, 18, 9));
			return S;
		}
	}

	FSlateFontInfo Font(int32 Size, bool bBold, bool bCondensed)
	{
		const TCHAR* Face = bCondensed ? TEXT("BoldCondensed") : bBold ? TEXT("Bold") : TEXT("Regular");
		return FCoreStyle::GetDefaultFontStyle(Face, Size);
	}

	const FSlateBrush* Image(const FString& File)
	{
		TMap<FString, FImageEntry>& Map = Images();
		if (const FImageEntry* Found = Map.Find(File)) return Found->Brush.Get();
		FImageEntry Entry;
		Entry.Brush = MakeShared<FSlateBrush>();
		if (UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(FFRConfig::ResourcePath(TEXT("UI/") + File)))
		{
			Texture->SRGB = true;
			Entry.Texture.Reset(Texture);
			Entry.Brush->SetResourceObject(Texture);
			Entry.Brush->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
			Entry.Brush->DrawAs = ESlateBrushDrawType::Image;
		}
		else
		{
			Entry.Brush->DrawAs = ESlateBrushDrawType::NoDrawType;
			UE_LOG(LogTemp, Warning, TEXT("FICSIT Roles: UI image missing: %s"), *File);
		}
		return Map.Add(File, MoveTemp(Entry)).Brush.Get();
	}

	bool HasImage(const FString& File) { return Image(File)->GetResourceObject() != nullptr; }

	const FSlateBrush* Sliced(const FString& File, const FMargin& Px)
	{
		static TMap<FString, TSharedPtr<FSlateBrush>> Cache;
		if (const TSharedPtr<FSlateBrush>* Found = Cache.Find(File)) return Found->Get();
		TSharedPtr<FSlateBrush> B = MakeShared<FSlateBrush>(*Image(File));
		if (B->GetResourceObject() && B->ImageSize.X > 0 && B->ImageSize.Y > 0)
		{
			B->DrawAs = ESlateBrushDrawType::Box;
			B->Margin = FMargin(Px.Left / B->ImageSize.X, Px.Top / B->ImageSize.Y, Px.Right / B->ImageSize.X, Px.Bottom / B->ImageSize.Y);
		}
		Cache.Add(File, B);
		return B.Get();
	}

	const FSlateBrush* Fill() { return NineSlice(TEXT("fill.png"), 0.25f); }
	const FSlateBrush* Frame() { return NineSlice(TEXT("frame.png"), 0.25f); }

	const FSlateBrush* White()
	{
		static FSlateBrush B = MakeBrush(FLinearColor::White);
		return &B;
	}

	static FButtonStyle ImageButton(const TCHAR* Normal, const TCHAR* Hover, const FMargin& Px, const FMargin& Padding)
	{
		FButtonStyle S;
		FSlateBrush N = *Sliced(Normal, Px); FSlateBrush H = *Sliced(Hover, Px);
		FSlateBrush P = H; P.TintColor = FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f));
		FSlateBrush D = N; D.TintColor = FSlateColor(FLinearColor(0.35f, 0.35f, 0.35f, 0.8f));
		S.SetNormal(N); S.SetHovered(H); S.SetPressed(P); S.SetDisabled(D);
		S.SetNormalPadding(Padding);
		S.SetPressedPadding(FMargin(Padding.Left, Padding.Top + 2, Padding.Right, Padding.Bottom - 2));
		return S;
	}

	const FButtonStyle& PrimaryButton()
	{
		static FButtonStyle S = HasImage(TEXT("btn_primary.png")) && HasImage(TEXT("btn_primary_hover.png"))
			? ImageButton(TEXT("btn_primary.png"), TEXT("btn_primary_hover.png"), FMargin(28, 22), FMargin(34, 16))
			: MakeButton(Orange, OrangeHot, Orange * 0.85f, Panel3);
		return S;
	}

	const FButtonStyle& SecondaryButton()
	{
		static FButtonStyle S = HasImage(TEXT("btn_secondary.png")) && HasImage(TEXT("btn_secondary_hover.png"))
			? ImageButton(TEXT("btn_secondary.png"), TEXT("btn_secondary_hover.png"), FMargin(20, 18), FMargin(22, 12))
			: MakeButton(Panel3, FLinearColor(FColor::FromHex(TEXT("383D44FF"))), Panel2, Panel2);
		return S;
	}

	const FButtonStyle& DangerButton()
	{
		static FButtonStyle S = MakeButton(Danger * 0.8f, Danger, Danger * 0.65f, Panel2);
		return S;
	}

	const FButtonStyle& FlatButton()
	{
		static FButtonStyle S = [] {
			FButtonStyle B;
			FSlateBrush None; None.DrawAs = ESlateBrushDrawType::NoDrawType;
			B.SetNormal(None); B.SetHovered(None); B.SetPressed(None); B.SetDisabled(None);
			B.SetNormalPadding(FMargin(0)); B.SetPressedPadding(FMargin(0));
			return B;
		}();
		return S;
	}

	const FEditableTextBoxStyle& TextBox()
	{
		static FEditableTextBoxStyle S = [] {
			FEditableTextBoxStyle T = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
			FSlateBrush Bg = *Fill(); Bg.TintColor = FSlateColor(Panel);
			FSlateBrush Hover = *Fill(); Hover.TintColor = FSlateColor(Panel3 * 0.8f);
			T.SetBackgroundImageNormal(Bg);
			T.SetBackgroundImageHovered(Hover);
			T.SetBackgroundImageFocused(Hover);
			T.SetBackgroundImageReadOnly(Bg);
			T.SetForegroundColor(FSlateColor(Text));
			T.SetFocusedForegroundColor(FSlateColor(Text));
			T.SetFont(Font(13));
			T.SetPadding(FMargin(10, 8));
			return T;
		}();
		return S;
	}

	const FScrollBarStyle& ScrollBar()
	{
		static FScrollBarStyle S = [] {
			FScrollBarStyle B = FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar");
			FSlateBrush Thumb = MakeBrush(Line); FSlateBrush ThumbHot = MakeBrush(Orange);
			FSlateBrush Track; Track.DrawAs = ESlateBrushDrawType::NoDrawType;
			B.SetNormalThumbImage(Thumb).SetHoveredThumbImage(ThumbHot).SetDraggedThumbImage(ThumbHot);
			B.SetVerticalBackgroundImage(Track).SetHorizontalBackgroundImage(Track);
			B.SetThickness(6.f);
			return B;
		}();
		return S;
	}

	const FCheckBoxStyle& CheckBox()
	{
		static FCheckBoxStyle S = FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("Checkbox");
		return S;
	}

	FLinearColor DivisionColor(FName Division)
	{
		const FFRDivisionDef* D = FFRConfig::Get().FindDivision(Division);
		return D ? D->Color : Muted;
	}

	const FSlateBrush* DivisionIcon(FName Division)
	{
		const FFRDivisionDef* D = FFRConfig::Get().FindDivision(Division);
		return Image(D && !D->Icon.IsEmpty() ? D->Icon : FString(TEXT("logo_mark.png")));
	}

	void Shutdown()
	{
		// Brushes keep pointing at textures until the next world; textures are kept alive on purpose
		// for the process lifetime (a few hundred KB) to avoid dangling Slate resources.
	}
}
