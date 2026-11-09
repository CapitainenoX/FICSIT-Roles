#include "FRLoc.h"
#include "FRConfig.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogFRLoc, Log, All);

namespace
{
	TMap<FString, FString> GActive;
	TMap<FString, FString> GEnglish;
	FString GLanguage;
	bool GLoaded = false;

	bool LoadFile(const FString& Lang, TMap<FString, FString>& Out)
	{
		FString Text;
		const FString Path = FFRConfig::ResourcePath(FString::Printf(TEXT("Localization/%s.json"), *Lang));
		if (!FFileHelper::LoadFileToString(Text, *Path)) return false;
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			UE_LOG(LogFRLoc, Error, TEXT("Invalid localization file %s"), *Path);
			return false;
		}
		Out.Reset();
		for (const auto& Pair : Root->Values)
		{
			FString Value;
			if (Pair.Value.IsValid() && Pair.Value->TryGetString(Value)) Out.Add(Pair.Key, Value);
		}
		return true;
	}

	void EnsureLoaded()
	{
		if (!GLoaded) FFRLoc::Reload();
	}
}

void FFRLoc::Reload()
{
	GLoaded = true;
	LoadFile(TEXT("en"), GEnglish);
	const FCultureRef Culture = FInternationalization::Get().GetCurrentLanguage();
	FString Lang = Culture->GetTwoLetterISOLanguageName().ToLower();
	if (Lang.IsEmpty()) Lang = TEXT("en");
	if (Lang == TEXT("en") || !LoadFile(Lang, GActive))
	{
		if (Lang != TEXT("en")) UE_LOG(LogFRLoc, Display, TEXT("No FICSIT Roles translation for '%s', English used"), *Lang);
		GActive = GEnglish;
		Lang = TEXT("en");
	}
	GLanguage = Lang;
	UE_LOG(LogFRLoc, Display, TEXT("Localization '%s' loaded (%d keys)"), *GLanguage, GActive.Num());
}

const FString* FFRLoc::Find(const FString& Key)
{
	EnsureLoaded();
	if (const FString* V = GActive.Find(Key)) return V;
	return GEnglish.Find(Key);
}

FText FFRLoc::T(const FString& Key)
{
	const FString* V = Find(Key);
	return FText::FromString(V ? *V : Key);
}

FText FFRLoc::F(const FString& Key, const FText& A0)
{
	return FText::Format(FTextFormat(T(Key)), A0);
}

FText FFRLoc::F(const FString& Key, const FText& A0, const FText& A1)
{
	return FText::Format(FTextFormat(T(Key)), A0, A1);
}

FText FFRLoc::F(const FString& Key, const FText& A0, const FText& A1, const FText& A2)
{
	return FText::Format(FTextFormat(T(Key)), A0, A1, A2);
}

FText FFRLoc::Division(FName Id)
{
	if (Id.IsNone()) return T(TEXT("div.none"));
	const FString Key = FString::Printf(TEXT("div.%s.name"), *Id.ToString());
	return Find(Key) ? T(Key) : FText::FromName(Id);
}

FString FFRLoc::Language()
{
	EnsureLoaded();
	return GLanguage;
}
