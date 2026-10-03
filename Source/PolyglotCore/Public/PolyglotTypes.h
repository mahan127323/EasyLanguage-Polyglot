// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * A single dictionary entry.
 *
 * An entry has two possible natures:
 *  - Localization identity (Namespace + Key + Source): used to override the
 *    FText localization table for text that already flows through the UE text
 *    system (engine, plugins using LOCTEXT/NSLOCTEXT ...). Key is mandatory in
 *    that case.
 *  - Source string only (Source, no key): used by the Slate scanning layer for
 *    text that never touches the localization system (FText::FromString /
 *    FName-derived labels like Niagara module names, material node captions,
 *    blueprint variable names, content browser folder/file names, ...).
 *
 * Entries carrying both work in both worlds.
 */
struct FPolyglotEntry
{
	/** Localization namespace of the source text ("" when unknown). */
	FString Namespace;

	/** Localization key of the source text ("" when the entry is source-only). */
	FString Key;

	/** The exact English / original text as displayed by the editor. */
	FString Source;

	/** Culture this translation belongs to, eg "zh-Hans", "ja", "en". */
	FString Culture;

	/** The translated text (empty = remove / do not translate). */
	FString Translation;

	/** Optional plugin or tool the source belongs to (informational). */
	FString Plugin;

	/** Optional free-form note (informational). */
	FString Comment;

	/** Optional UI scope hint, eg "Niagara", "Material", "Blueprint", "ContentBrowser". */
	FString Scope;

	bool IsLocalizationEntry() const { return !Namespace.IsEmpty() || !Key.IsEmpty(); }
	bool IsSourceOnly() const { return Key.IsEmpty(); }

	/** Compact identity used for deduplication within one culture. */
	FString BuildLookupKey() const
	{
		return Culture + TEXT("\x1F") + Namespace + TEXT("\x1F") + Key + TEXT("\x1F") + Source;
	}
};
