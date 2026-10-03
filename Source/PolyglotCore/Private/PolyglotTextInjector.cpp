// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "PolyglotTextInjector.h"

#include "Internationalization/Culture.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/TextLocalizationResource.h"

#include "PolyglotDictionary.h"
#include "PolyglotLog.h"
#include "PolyglotTypes.h"

namespace
{
	/** Canonical culture spelling used for every culture comparison. */
	FString CanonicalCultureName(const FString& InName)
	{
		if (InName.IsEmpty())
		{
			return InName;
		}
		const FString Canonical = FCulture::GetCanonicalName(InName);
		return Canonical.IsEmpty() ? InName : Canonical;
	}
}

int32 FPolyglotTextInjector::GetKeyedEntryCount() const
{
	if (!Dictionary)
	{
		return 0;
	}
	TArray<FPolyglotEntry> Keyed;
	Dictionary->GetLocalizationEntries(Keyed);
	return Keyed.Num();
}

bool FPolyglotTextInjector::ApplySingleEntryPatch(const FPolyglotEntry& Entry)
{
	if (Entry.Key.IsEmpty() || Entry.Source.IsEmpty() || Entry.Translation.IsEmpty())
	{
		UE_LOG(LogPolyglot, Verbose, TEXT("Single entry patch skipped: key/source/translation missing."));
		return false;
	}

	FTextLocalizationResource Resource;
	Resource.AddEntry(Entry.Namespace, Entry.Key, Entry.Source, Entry.Translation, PatchPriority);

	if (Resource.IsEmpty())
	{
		return false;
	}

	FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);
	UE_LOG(LogPolyglot, Display, TEXT("Applied single FText override '%s' -> '%s'."),
		*Entry.Source, *Entry.Translation);
	return true;
}

bool FPolyglotTextInjector::ApplyPatch(const FString& InActiveCulture)
{
	if (!Dictionary)
	{
		return false;
	}

	const FString ActiveCulture = CanonicalCultureName(InActiveCulture);
	if (ActiveCulture.IsEmpty())
	{
		return false;
	}

	TArray<FPolyglotEntry> KeyedEntries;
	Dictionary->GetLocalizationEntries(KeyedEntries);

	// -----------------------------------------------------------------------
	// Build the override table keyed by the ns/key token.
	//
	// Two passes give the final text for every token:
	//   1. any entry seeds the row with its English source ("neutralise"),
	//   2. an entry whose culture matches the active culture overwrites the row
	//      with its translation.
	//
	// If the dictionary ever contains two translations for the same ns/key and
	// culture, the last raw entry wins (user files load after built-ins, and
	// the dictionary already resolves duplicate identities that way).
	// -----------------------------------------------------------------------
	struct FOverrideRow
	{
		FString Source;
		FString Text;
	};

	TMap<FString, FOverrideRow> Rows;
	Rows.Reserve(KeyedEntries.Num());

	for (const FPolyglotEntry& Entry : KeyedEntries)
	{
		if (Entry.Source.IsEmpty())
		{
			continue;
		}

		const FString Token = FPolyglotDictionary::MakeLocalizedToken(Entry.Namespace, Entry.Key);
		if (!Rows.Contains(Token))
		{
			// First sight of this ns/key: neutralise it back to English so a
			// stale translation from a previously active culture can never
			// survive a language toggle.
			Rows.Emplace(Token, FOverrideRow{ Entry.Source, Entry.Source });
		}

		if (CanonicalCultureName(Entry.Culture).Equals(ActiveCulture))
		{
			FOverrideRow& Row = Rows.FindChecked(Token);
			Row.Source = Entry.Source;
			Row.Text = Entry.Translation;
		}
	}

	// -----------------------------------------------------------------------
	// Serialise the rows into a localization resource and hand it to the live
	// table. Splitting the token back into namespace/key is done instead of
	// storing two strings per map row to keep the row type trivially copyable.
	// -----------------------------------------------------------------------
	FTextLocalizationResource Resource;
	int32 Written = 0;

	for (const TPair<FString, FOverrideRow>& Pair : Rows)
	{
		const FOverrideRow& Row = Pair.Value;
		if (Row.Source.IsEmpty() || Row.Text.IsEmpty())
		{
			continue;
		}

		int32 Separator = INDEX_NONE;
		if (!Pair.Key.FindChar(TCHAR(0x1F), Separator))
		{
			continue;
		}

		const FString Namespace = Pair.Key.Left(Separator);
		const FString Key = Pair.Key.Mid(Separator + 1);
		Resource.AddEntry(Namespace, Key, Row.Source, Row.Text, PatchPriority);
		++Written;
	}

	if (Written == 0)
	{
		UE_LOG(LogPolyglot, Verbose, TEXT("Override patch empty for culture '%s'."), *ActiveCulture);
		return false;
	}

	FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);

	LastPatchedCulture = ActiveCulture;
	LastPatchRowCount = Written;

	UE_LOG(LogPolyglot, Display, TEXT("Injected %d FText override(s) for culture '%s' (%d keyed entries in dictionary)."),
		Written, *ActiveCulture, KeyedEntries.Num());
	return true;
}
