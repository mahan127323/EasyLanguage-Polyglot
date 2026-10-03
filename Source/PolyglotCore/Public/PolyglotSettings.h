// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Where Polyglot reads and writes its own data. The plugin never touches the
 * engine directory or the project Content folder; everything lives under:
 *
 *   {Project}/Config/Polyglot/   settings + user dictionary (travels with the
 *                                project and can be version controlled)
 *   {Project}/Saved/Polyglot/    cache / exports (ignored by VCS)
 *   {Plugin}/Content/Dictionaries/  read-only built-in dictionaries
 */
namespace PolyglotSettings
{
	/** Persistent plugin state (very small, JSON file). */
	struct FPolyglotState
	{
		bool bEnabled = false;
		FString Culture;
		float ScanIntervalSeconds = 0.5f;
		float ScanTimeBudgetMs = 2.0f;
		bool bTranslateStaticTextBlocks = true;
		bool bTranslateEditableText = false;

		void Reset()
		{
			bEnabled = false;
			Culture.Empty();
			ScanIntervalSeconds = 0.5f;
			ScanTimeBudgetMs = 2.0f;
			bTranslateStaticTextBlocks = true;
			bTranslateEditableText = false;
		}
	};

	/** Directory holding all Polyglot files for this project: {Project}/Config/Polyglot */
	POLYGLOTCORE_API FString GetProjectPolyglotDir();

	/** Settings file path. */
	POLYGLOTCORE_API FString GetSettingsPath();

	/** User dictionary file path (managed by the picker and the dictionary editor). */
	POLYGLOTCORE_API FString GetUserDictionaryPath();

	/** Directory that may hold extra user dictionary files (e.g. plugin-specific packs). */
	POLYGLOTCORE_API FString GetUserDictionariesDir();

	/** Built-in dictionary directory (read only, ships with the plugin). */
	POLYGLOTCORE_API FString GetBuiltinDictionaryDir();

	/** Cache / export directory inside Saved. */
	POLYGLOTCORE_API FString GetSavedPolyglotDir();

	/** Load persisted state (returns defaults when the file is missing/corrupt). */
	POLYGLOTCORE_API FPolyglotState LoadState();

	/** Persist the state. */
	POLYGLOTCORE_API void SaveState(const FPolyglotState& InState);
}
