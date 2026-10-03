// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PolyglotTypes.h"

/**
 * FPolyglotDictionary
 *
 * Loads and owns every dictionary entry:
 *   1. built-in dictionaries shipped with the plugin (read only),
 *   2. extra user dictionary packs in the project polyglot directory,
 *   3. the main user dictionary written by the text picker / dictionary editor.
 *
 * Later sources override earlier ones for the same (culture, namespace, key,
 * source) identity, which gives the natural priority: built-in < user.
 *
 * The class builds two in-memory indexes per culture:
 *   - source  -> translation   (used by the Slate scanning layer)
 *   - ns/key  -> translation   (used by the FText localization injector)
 *
 * Files are re-read with ReloadAll(); timestamps are polled by the subsystem so
 * users can simply edit dictionaries in any editor and see them hot-reload.
 */
class POLYGLOTCORE_API FPolyglotDictionary
{
public:
	FPolyglotDictionary() = default;
	~FPolyglotDictionary() = default;

	// ------------------------------------------------------------------------
	// Loading
	// ------------------------------------------------------------------------

	/** Load / reload everything (built-ins, extra packs, user file). */
	void ReloadAll();

	/** Load just the user main dictionary file again. */
	void ReloadUserFile();

	// ------------------------------------------------------------------------
	// Lookups (all queries are culture-scoped)
	// ------------------------------------------------------------------------

	/** Exact source-string lookup for the given culture. */
	bool TranslateSource(const FString& Culture, const FString& SourceText, FString& OutTranslated) const;

	/** Exact namespace+key lookup for the given culture. */
	bool TranslateLocalized(const FString& Culture, const FString& Namespace, const FString& Key, FString& OutTranslated) const;

	/** Returns true when a source entry exists for the culture. */
	bool HasSourceEntry(const FString& Culture, const FString& SourceText) const;

	// ------------------------------------------------------------------------
	// Entry management (persisted into the main user dictionary)
	// ------------------------------------------------------------------------

	/** Add or update an entry. Empty translation removes it. Returns true on success. */
	bool AddOrUpdateUserEntry(const FPolyglotEntry& InEntry);

	/** Remove a user entry that matches culture + ns + key + source. */
	bool RemoveUserEntry(const FPolyglotEntry& InEntry);

	/**
	 * Merge a batch of entries into the user dictionary with a single save.
	 * Entries with the same identity overwrite existing user entries; empty
	 * translations delete. @return the number of non-empty entries merged.
	 */
	int32 ImportUserEntries(const TArray<FPolyglotEntry>& InEntries);

	/** All entries (for editors / picker). bUserOnly filters to user-provided. */
	void GetEntries(TArray<FPolyglotEntry>& OutEntries, bool bUserOnly) const;

	/**
	 * Collect every entry that carries a localization identity (non-empty Key)
	 * together with a non-empty Source, Translation and Culture. This is the
	 * exact subset consumed by FPolyglotTextInjector when it rebuilds the FText
	 * override patch (a row needs ns/key/source to write a reliable entry).
	 */
	void GetLocalizationEntries(TArray<FPolyglotEntry>& OutEntries) const;

	/** How many raw entries are currently loaded. */
	int32 GetEntryCount() const;

	// ------------------------------------------------------------------------
	// Utilities
	// ------------------------------------------------------------------------

	/** Namespace + Key folded into one lookup token (separator collision-safe enough for dictionaries). */
	static FString MakeLocalizedToken(const FString& Namespace, const FString& Key);

	const FString& GetUserDictionaryPath() const { return UserDictionaryPath; }

	/** Last modification timestamps used for cheap polling. */
	FDateTime GetUserFileTimestamp() const { return UserFileTimestamp; }
	void TouchUserFileTimestamp();

private:
	struct FRawEntry
	{
		FPolyglotEntry Data;
		bool bUser = false;
	};

	void LoadBuiltins();
	void LoadUserDirectory();
	void LoadUserMainFile();
	void LoadJsonDictionaryFile(const FString& Path, bool bUserFile);
	void RebuildIndexes();
	void AddRawEntry(const FRawEntry& InEntry);
	void SaveUserFile();

	// Raw storage (kept to power editors / export tools).
	TArray<FRawEntry> RawEntries;

	// culture -> (source -> translation)
	TMap<FString, TMap<FString, FString>> CultureSourceIndex;
	// culture -> (token -> translation)
	TMap<FString, TMap<FString, FString>> CultureLocalizedIndex;

	FString UserDictionaryPath;
	FDateTime UserFileTimestamp = FDateTime::MinValue();
	bool bDirty = false;
};
