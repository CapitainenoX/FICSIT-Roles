#include "UI/FRNames.h"
#include "FRLoc.h"
#include "FGRecipe.h"
#include "Resources/FGItemDescriptor.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "UObject/UObjectIterator.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	UClass* FindClass(const FString& Short, UClass* Base)
	{
		static TMap<FString, TWeakObjectPtr<UClass>> Cache;
		const FString Key = Base->GetName() + TEXT(":") + Short;
		if (const TWeakObjectPtr<UClass>* Found = Cache.Find(Key)) if (Found->IsValid()) return Found->Get();
		const FString Wanted = Short + TEXT("_C");
		for (TObjectIterator<UClass> It; It; ++It)
			if (It->GetName() == Wanted && It->IsChildOf(Base)) { Cache.Add(Key, *It); return *It; }
		return nullptr;
	}

	FString Pretty(FString S)
	{
		for (const TCHAR* Prefix : { TEXT("Build_"), TEXT("Recipe_"), TEXT("Desc_"), TEXT("Equip_"), TEXT("BP_") }) S.RemoveFromStart(Prefix);
		S.RemoveFromEnd(TEXT("*"));
		return S.Replace(TEXT("_"), TEXT(" "));
	}

	const FSlateBrush* BrushFor(UClass* Item)
	{
		static TMap<TWeakObjectPtr<UClass>, TSharedPtr<FSlateBrush>> Cache;
		if (!Item) return nullptr;
		if (const TSharedPtr<FSlateBrush>* Found = Cache.Find(Item)) return Found->Get();
		UTexture2D* Texture = UFGItemDescriptor::GetSmallIcon(Item);
		if (!Texture) return nullptr;
		// Brushes hold raw pointers the GC does not see: keep the game texture alive for the session.
		static TArray<TStrongObjectPtr<UTexture2D>> Keep;
		Keep.Emplace(Texture);
		TSharedPtr<FSlateBrush> B = MakeShared<FSlateBrush>();
		B->SetResourceObject(Texture);
		B->ImageSize = FVector2D(64, 64);
		Cache.Add(Item, B);
		return B.Get();
	}

	UClass* DescriptorOfBuilding(const FString& Short)
	{
		FString Name = Short;
		Name.RemoveFromEnd(TEXT("*"));
		if (Name.RemoveFromStart(TEXT("Build_"))) return FindClass(TEXT("Desc_") + Name, UFGItemDescriptor::StaticClass());
		return nullptr;
	}
}

FText FRNames::Item(const FString& ShortName)
{
	UClass* C = FRFindItem(ShortName);
	return C ? UFGItemDescriptor::GetItemName(C) : FText::FromString(Pretty(ShortName));
}

const FSlateBrush* FRNames::ItemIcon(const FString& ShortName)
{
	return BrushFor(FRFindItem(ShortName));
}

FText FRNames::Target(EFRPerm Perm, const FString& ShortName)
{
	if (ShortName == TEXT("DropPod")) return FFRLoc::T(TEXT("tech.droppod"));
	switch (Perm)
	{
	case EFRPerm::Build:
		if (UClass* Desc = DescriptorOfBuilding(ShortName)) return UFGItemDescriptor::GetItemName(Desc);
		break;
	case EFRPerm::Craft:
		if (UClass* Recipe = FindClass(ShortName, UFGRecipe::StaticClass())) return UFGRecipe::GetRecipeName(Recipe);
		break;
	case EFRPerm::Collect:
		if (UClass* C = FRFindItem(ShortName)) return UFGItemDescriptor::GetItemName(C);
		break;
	default: break;
	}
	return FText::FromString(Pretty(ShortName));
}

FText FRNames::Node(FName NodeId)
{
	const FString Key = FString::Printf(TEXT("tech.%s.name"), *NodeId.ToString());
	const FText Localized = FFRLoc::T(Key);
	if (!Localized.ToString().Equals(Key)) return Localized;
	const FFRTechNode* N = FFRConfig::Get().FindNode(NodeId);
	if (!N) return FText::FromName(NodeId);
	int32 Count = 0;
	FText First;
	for (int32 P = 0; P < (int32)EFRPerm::Num; ++P)
		for (const FString& T : N->Grants.Of((EFRPerm)P))
		{
			if (Count++ == 0) First = Target((EFRPerm)P, T);
		}
	if (Count == 0) return FText::FromName(NodeId);
	return Count == 1 ? First : FText::Format(FText::FromString(TEXT("{0} +{1}")), First, FText::AsNumber(Count - 1));
}

const FSlateBrush* FRNames::NodeIcon(FName NodeId)
{
	const FFRTechNode* N = FFRConfig::Get().FindNode(NodeId);
	if (!N) return nullptr;
	for (const FString& T : N->Grants.Of(EFRPerm::Build)) if (const FSlateBrush* B = BrushFor(DescriptorOfBuilding(T))) return B;
	for (const FString& T : N->Grants.Of(EFRPerm::Collect)) if (const FSlateBrush* B = BrushFor(FRFindItem(T))) return B;
	return nullptr;
}
