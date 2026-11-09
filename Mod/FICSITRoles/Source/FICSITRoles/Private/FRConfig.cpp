#include "FRConfig.h"
#include "Resources/FGItemDescriptor.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogFRConfig, Log, All);

FString FRShortClassName(const UClass* Class)
{
	if (!Class) return FString();
	FString Name = Class->GetName();
	Name.RemoveFromEnd(TEXT("_C"));
	return Name;
}

UClass* FRFindItem(const FString& NameOrPath)
{
	static TMap<FString, TWeakObjectPtr<UClass>> Cache;
	if (NameOrPath.IsEmpty()) return nullptr;
	if (const TWeakObjectPtr<UClass>* Found = Cache.Find(NameOrPath))
		if (Found->IsValid()) return Found->Get();
	UClass* Result = nullptr;
	if (NameOrPath.Contains(TEXT("/")))
	{
		Result = LoadClass<UFGItemDescriptor>(nullptr, *NameOrPath);
	}
	else
	{
		// Item descriptors are loaded with the recipes once a world exists: look them up by name.
		const FString Wanted = NameOrPath.EndsWith(TEXT("_C")) ? NameOrPath : NameOrPath + TEXT("_C");
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->GetName() == Wanted && It->IsChildOf(UFGItemDescriptor::StaticClass())) { Result = *It; break; }
		}
	}
	if (Result) Cache.Add(NameOrPath, Result);
	else UE_LOG(LogFRConfig, Warning, TEXT("Item not found: %s"), *NameOrPath);
	return Result;
}

UClass* FRFindClass(const FString& ShortName, UClass* Base)
{
	static TMap<FString, TWeakObjectPtr<UClass>> Cache;
	if (ShortName.IsEmpty() || !Base) return nullptr;
	const FString Key = Base->GetName() + TEXT(":") + ShortName;
	if (const TWeakObjectPtr<UClass>* Found = Cache.Find(Key)) if (Found->IsValid()) return Found->Get();
	const FString Wanted = ShortName + TEXT("_C");
	for (TObjectIterator<UClass> It; It; ++It)
		if (It->GetName() == Wanted && It->IsChildOf(Base)) { Cache.Add(Key, *It); return *It; }
	return nullptr;
}

const FFRConfig& FFRConfig::Get()
{
	static FFRConfig* Instance = [] { FFRConfig* C = new FFRConfig(); C->Load(); return C; }();
	return *Instance;
}

FString FFRConfig::ResourcePath(const FString& Relative)
{
	static const FString Base = [] {
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("FICSITRoles"));
		return Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources")) : FString(TEXT("Resources"));
	}();
	return FPaths::Combine(Base, Relative);
}

static FLinearColor ParseHex(const FString& Hex, const FLinearColor& Fallback)
{
	if (Hex.Len() != 7 || Hex[0] != TEXT('#')) return Fallback;
	return FLinearColor(FColor::FromHex(Hex));
}

static TArray<FString> ReadStrings(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
{
	TArray<FString> Out;
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Obj.IsValid() && Obj->TryGetArrayField(Field, Arr))
		for (const TSharedPtr<FJsonValue>& V : *Arr) { FString S; if (V.IsValid() && V->TryGetString(S) && !S.IsEmpty()) Out.Add(S); }
	return Out;
}

static TArray<TPair<FString, int32>> ReadAmounts(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
{
	TArray<TPair<FString, int32>> Out;
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Obj.IsValid() || !Obj->TryGetArrayField(Field, Arr)) return Out;
	for (const TSharedPtr<FJsonValue>& Item : *Arr)
	{
		const TArray<TSharedPtr<FJsonValue>>* Entry = nullptr;
		if (Item.IsValid() && Item->TryGetArray(Entry) && Entry->Num() == 2)
			Out.Emplace((*Entry)[0]->AsString(), FMath::Clamp((int32)(*Entry)[1]->AsNumber(), 1, 5000));
	}
	return Out;
}

static FFRPermSet ReadPerms(const TSharedPtr<FJsonObject>& Obj)
{
	FFRPermSet Set;
	if (!Obj.IsValid()) return Set;
	Set.Of(EFRPerm::Build) = ReadStrings(Obj, TEXT("build"));
	Set.Of(EFRPerm::Craft) = ReadStrings(Obj, TEXT("craft"));
	Set.Of(EFRPerm::Collect) = ReadStrings(Obj, TEXT("collect"));
	Set.Of(EFRPerm::Equip) = ReadStrings(Obj, TEXT("equip"));
	return Set;
}

void FFRConfig::LoadDefaults()
{
	Divisions.Reset();
	Tech.Reset();
	auto Add = [this](const TCHAR* Id, const TCHAR* Color, TArray<FString> Build) {
		FFRDivisionDef D; D.Id = Id; D.Color = ParseHex(Color, FLinearColor::White);
		D.Icon = FString::Printf(TEXT("div_%s.png"), Id); D.Base.Of(EFRPerm::Build) = MoveTemp(Build); Divisions.Add(D);
	};
	Add(TEXT("exploration"), TEXT("#4FB477"), { TEXT("Build_RadarTower"), TEXT("Build_Mam") });
	Add(TEXT("energy"), TEXT("#F2B630"), { TEXT("Build_GeneratorBiomass*"), TEXT("Build_GeneratorCoal"), TEXT("Build_GeneratorFuel"), TEXT("Build_GeneratorNuclear") });
	Add(TEXT("industry"), TEXT("#5AA0D8"), { TEXT("Build_SmelterMk1"), TEXT("Build_ConstructorMk1"), TEXT("Build_AssemblerMk1"), TEXT("Build_ManufacturerMk1") });
	Add(TEXT("logistics"), TEXT("#B48CE0"), { TEXT("Build_TrainStation"), TEXT("Build_RailroadTrack*"), TEXT("Build_DroneStation") });
}

void FFRConfig::Load()
{
	// The editable copy lives in Saved/FICSITRoles (survives mod updates). It is created from the shipped
	// default on first launch, and replaced (with a .old backup) when the mod ships a newer revision.
	const FString Default = ResourcePath(TEXT("Config/FICSITRoles.json"));
	const FString User = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FICSITRoles"), TEXT("FICSITRoles.json"));
	auto RevisionOf = [](const FString& File) -> int32 {
		FString Raw; TSharedPtr<FJsonObject> Obj; double R = -1;
		if (FFileHelper::LoadFileToString(Raw, *File) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Obj) && Obj.IsValid())
			Obj->TryGetNumberField(TEXT("configRevision"), R);
		return (int32)R;
	};
	IFileManager& Files = IFileManager::Get();
	const int32 DefaultRev = RevisionOf(Default);
	if (!Files.FileExists(*User))
	{
		Files.Copy(*User, *Default);
	}
	else if (RevisionOf(User) < DefaultRev)
	{
		Files.Copy(*(User + TEXT(".old")), *User);
		Files.Copy(*User, *Default);
		Diagnostics.Add(TEXT("User config replaced by a newer mod revision (previous kept as .old)"));
	}
	const FString Path = Files.FileExists(*User) ? User : Default;
	LoadedPath = Path;
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		Diagnostics.Add(FString::Printf(TEXT("Config unreadable (%s): built-in defaults used"), *Path));
		UE_LOG(LogFRConfig, Error, TEXT("%s"), *Diagnostics.Last());
		LoadDefaults();
		return;
	}

	Root->TryGetStringField(TEXT("terminalKey"), TerminalKey);
	Root->TryGetBoolField(TEXT("mamTrees"), bMamTrees);
	{ double Rev = 0; if (Root->TryGetNumberField(TEXT("configRevision"), Rev)) Revision = (int32)Rev; }
	double Number = 0;
	if (Root->TryGetNumberField(TEXT("maxExpectedPlayers"), Number)) MaxExpectedPlayers = FMath::Clamp((int32)Number, 1, 16);
	if (Root->TryGetNumberField(TEXT("denyNotifyCooldownSeconds"), Number)) DenyCooldown = FMath::Clamp((float)Number, 0.5f, 30.f);
	FString Coverage;
	if (Root->TryGetStringField(TEXT("defaultCoverage"), Coverage))
		DefaultCoverage = Coverage == TEXT("Vacant") ? 1 : Coverage == TEXT("Absent") ? 2 : 0;

	const TArray<TSharedPtr<FJsonValue>>* Divs = nullptr;
	if (Root->TryGetArrayField(TEXT("divisions"), Divs))
	{
		for (const TSharedPtr<FJsonValue>& V : *Divs)
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!V.IsValid() || !V->TryGetObject(Obj)) continue;
			FString Id; (*Obj)->TryGetStringField(TEXT("id"), Id);
			if (Id.IsEmpty() || FindDivision(*Id)) { Diagnostics.Add(TEXT("Division without id or duplicated id ignored")); continue; }
			FFRDivisionDef D;
			D.Id = *Id;
			FString Color; (*Obj)->TryGetStringField(TEXT("color"), Color);
			D.Color = ParseHex(Color, FLinearColor(0.8f, 0.8f, 0.8f));
			(*Obj)->TryGetStringField(TEXT("icon"), D.Icon);
			const TSharedPtr<FJsonObject>* Base = nullptr;
			if ((*Obj)->TryGetObjectField(TEXT("base"), Base)) D.Base = ReadPerms(*Base);
			else D.Base.Of(EFRPerm::Build) = ReadStrings(*Obj, TEXT("build")); // schema 1
			D.StartRecipes = ReadStrings(*Obj, TEXT("startRecipes"));
			D.StartKit = ReadAmounts(*Obj, TEXT("startKit"));
			Divisions.Add(D);
		}
	}
	if (Divisions.Num() == 0) { Diagnostics.Add(TEXT("No division in config: defaults used")); LoadDefaults(); return; }

	Renamed.Reset();
	const TSharedPtr<FJsonObject>* Ren = nullptr;
	if (Root->TryGetObjectField(TEXT("renamed"), Ren))
		for (const TPair<FString, TSharedPtr<FJsonValue>>& R : (*Ren)->Values)
			if (!R.Key.StartsWith(TEXT("_")) && R.Value.IsValid()) Renamed.Add(FName(*R.Key), FName(*R.Value->AsString()));

	const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
	if (Root->TryGetArrayField(TEXT("techTree"), Nodes))
	{
		for (const TSharedPtr<FJsonValue>& V : *Nodes)
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!V.IsValid() || !V->TryGetObject(Obj)) continue;
			FString Id, Div;
			(*Obj)->TryGetStringField(TEXT("id"), Id);
			(*Obj)->TryGetStringField(TEXT("division"), Div);
			if (Id.IsEmpty() || !FindDivision(*Div) || FindNode(*Id)) { Diagnostics.Add(FString::Printf(TEXT("Invalid tech node '%s' ignored"), *Id)); continue; }
			FFRTechNode N;
			N.Id = *Id; N.Division = *Div;
			double Tier = 1; (*Obj)->TryGetNumberField(TEXT("tier"), Tier);
			N.Tier = FMath::Clamp((int32)Tier, 1, 9);
			double Col = -1; (*Obj)->TryGetNumberField(TEXT("col"), Col);
			N.Col = FMath::Clamp((int32)Col, -1, 6);
			for (const FString& R : ReadStrings(*Obj, TEXT("requires"))) N.Requires.Add(*R);
			N.Cost = ReadAmounts(*Obj, TEXT("cost"));
			const TSharedPtr<FJsonObject>* Grants = nullptr;
			if ((*Obj)->TryGetObjectField(TEXT("grants"), Grants)) N.Grants = ReadPerms(*Grants);
			Tech.Add(N);
		}
		// Drop requirements that point nowhere (never block a node forever).
		for (FFRTechNode& N : Tech)
			N.Requires.RemoveAll([this, &N](FName R) {
				const FFRTechNode* P = FindNode(R);
				const bool bBad = !P || P->Division != N.Division || R == N.Id;
				if (bBad) Diagnostics.Add(FString::Printf(TEXT("Node %s: invalid requirement %s removed"), *N.Id.ToString(), *R.ToString()));
				return bBad;
			});
	}
	for (const FString& D : Diagnostics) UE_LOG(LogFRConfig, Warning, TEXT("%s"), *D);
	UE_LOG(LogFRConfig, Display, TEXT("Config loaded from %s: %d divisions, %d tech nodes, MAM=%d"), *Path, Divisions.Num(), Tech.Num(), bMamTrees);
}

const FFRDivisionDef* FFRConfig::FindDivision(FName Id) const
{
	return Divisions.FindByPredicate([Id](const FFRDivisionDef& D) { return D.Id == Id; });
}

int32 FFRConfig::DivisionIndex(FName Id) const
{
	return Divisions.IndexOfByPredicate([Id](const FFRDivisionDef& D) { return D.Id == Id; });
}

const FFRTechNode* FFRConfig::FindNode(FName Id) const
{
	return Tech.FindByPredicate([Id](const FFRTechNode& N) { return N.Id == Id; });
}

bool FFRConfig::Matches(const FString& Pattern, const FString& Name)
{
	if (Pattern.EndsWith(TEXT("*"))) return Name.StartsWith(Pattern.LeftChop(1), ESearchCase::IgnoreCase);
	return Name.Equals(Pattern, ESearchCase::IgnoreCase);
}

const FFRPermRule* FFRConfig::RuleFor(EFRPerm Perm, const FString& Name) const
{
	TMap<FString, int32>& Cache = RuleCache[(int32)Perm];
	if (const int32* Cached = Cache.Find(Name))
		return *Cached == INDEX_NONE ? nullptr : &Rules[*Cached];

	FFRPermRule Rule;
	auto Any = [&Name](const TArray<FString>& Patterns) { return Patterns.ContainsByPredicate([&Name](const FString& P) { return Matches(P, Name); }); };
	for (const FFRDivisionDef& D : Divisions)
		if (Any(D.Base.Of(Perm))) Rule.BaseOwners.AddUnique(D.Id);
	for (const FFRTechNode& N : Tech)
		if (Any(N.Grants.Of(Perm))) Rule.NodeGrants.Emplace(N.Division, N.Id);

	const bool bRestricted = Rule.BaseOwners.Num() > 0 || Rule.NodeGrants.Num() > 0;
	const int32 Index = bRestricted ? Rules.Add(new FFRPermRule(Rule)) : INDEX_NONE;
	Cache.Add(Name, Index);
	return Index == INDEX_NONE ? nullptr : &Rules[Index];
}

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
