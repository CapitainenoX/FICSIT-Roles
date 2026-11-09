// Runtime localization from Resources/Localization/<lang>.json (flat key -> text).
// Language follows the game setting (two-letter ISO code), falls back to English, then to the key.
// Placeholders use {0}, {1}... and are filled with FText::Format.
#pragma once

#include "CoreMinimal.h"

class FICSITROLES_API FFRLoc
{
public:
	static FText T(const FString& Key);
	static FText F(const FString& Key, const FText& A0);
	static FText F(const FString& Key, const FText& A0, const FText& A1);
	static FText F(const FString& Key, const FText& A0, const FText& A1, const FText& A2);
	static FString Str(const FString& Key) { return T(Key).ToString(); }

	/** Display name of a division (div.<id>.name), or the id itself for unknown divisions. */
	static FText Division(FName Id);
	/** Two-letter language currently loaded. */
	static FString Language();
	/** Called on startup and when the game culture changes. */
	static void Reload();

private:
	static const FString* Find(const FString& Key);
};
