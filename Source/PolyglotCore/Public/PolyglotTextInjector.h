// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FPolyglotEntry;

class FPolyglotDictionary;

/**
 * FPolyglotTextInjector - the FText "localization override" layer.
 *
 * The engine already ships full Simplified-Chinese translations for itself
 * (Engine/Content/Localization/Editor/zh-Hans/...). What is missing there is:
 *   - our dictionary overrides for engine text the official locres translated
 *     differently (or not at all),
 *   - translations for third-party plugins that have no zh-Hans locres yet.
 *
 * This class turns dictionary entries that carry a localization identity
 * (namespace + key) into an FTextLocalizationResource and pushes it into the
 * engine's live display-string table through
 * FTextLocalizationManager::UpdateFromLocalizationResource.
 *
 * Engine semantics verified against UE 5.8 sources:
 *
 *  1. FTextLocalizationResource::AddEntry stores an int32 priority where
 *     "smaller numbers are higher priority"; conflicts are resolved by
 *     ShouldReplaceEntry. We use ELocalizedTextSourcePriority::Lowest (-1000).
 *
 *  2. FTextLocalizationManager::UpdateLiveTable ignores that priority and simply
 *     overwrites the current live entry (bReplaceExisting=true) whenever the
 *     injected resource contains the same text id. Practically this means the
 *     LAST write wins, so the full patch has to be re-injected AFTER the engine
 *     finished refreshing its own resources for the new culture.
 *
 *  3. UpdateLiveTable never REMOVES live entries that the incoming resource
 *     does not contain. Switching zh-Hans -> en therefore leaves stale Chinese
 *     overrides alive for keys that exist in no English locres. The patch this
 *     class builds is culture-agnostic on purpose: for EVERY keyed dictionary
 *     entry it writes either the active culture's translation or the original
 *     English source (a "neutralising" row). Whatever culture is active, every
 *     key we ever overrode ends up pointing at the correct text.
 *
 * 4. Each injection dirties the text revision, which makes every STextBlock /
 *    Slate control that displays a localizable FText repaint with the new
 *    display string automatically.
 *
 * This class deliberately avoids RegisterPolyglotTextData: FPolyglotTextData
 * lives in an engine-owned text source that has NO public removal API and is
 * re-applied on every refresh at the highest priority. One-shot resources like
 * the ones built here can be rebuilt/reversed on every refresh, which is
 * exactly what a live language toggle needs. Single user entries are applied
 * by re-running the full patch (rebuilding a few thousand rows is negligible)
 * instead of leaving permanent polyglot data behind.
 */
class POLYGLOTCORE_API FPolyglotTextInjector
{
public:
	FPolyglotTextInjector() = default;

	/** The dictionary that owns every entry (owned by the subsystem). */
	void SetDictionary(FPolyglotDictionary* InDictionary) { Dictionary = InDictionary; }

	/**
	 * Priority of every injected row. Lowest numeric value wins over engine
	 * entries (which use priority 0 / culture order).
	 */
	static constexpr int32 PatchPriority = -1000; // ELocalizedTextSourcePriority::Lowest

	/**
	 * Rebuild and inject the full override patch for the given active culture.
	 *
	 * Must run AFTER the engine finished refreshing its own localization
	 * resources (i.e. from the culture controller's completion callback).
	 *
	 * @return true when at least one row was injected.
	 */
	bool ApplyPatch(const FString& ActiveCulture);

	/**
	 * Inject a single dictionary entry immediately. Used by the text picker so
	 * the user sees the translation right after pressing "add". The next full
	 * patch (culture switch / reload / dictionary change) takes over from there.
	 */
	bool ApplySingleEntryPatch(const FPolyglotEntry& Entry);

	/** Number of keyed entries currently known by the dictionary (diagnostics). */
	int32 GetKeyedEntryCount() const;

	/** Culture of the last successful ApplyPatch() ("" if never applied). */
	const FString& GetLastPatchedCulture() const { return LastPatchedCulture; }

	/** Number of rows written by the last successful ApplyPatch(). */
	int32 GetLastPatchRowCount() const { return LastPatchRowCount; }

private:
	FPolyglotDictionary* Dictionary = nullptr;
	FString LastPatchedCulture;
	int32 LastPatchRowCount = 0;
};
