// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * FPolyglotCultureController - the culture layer of the plugin.
 *
 * Owns every engine interaction around culture:
 *   - validating that a requested culture exists in the engine (ICU),
 *   - switching the whole editor UI language without restarting,
 *   - ordering the switch so that the caller can restore Slate-patched widgets
 *     BEFORE the engine reloads its text resources and re-apply dictionary
 *     overrides AFTER that reload has completed.
 *
 * Engine behaviour this class relies on (verified against UE 5.8):
 *
 *  - FInternationalization::SetCurrentLanguageAndLocale() broadcasts
 *    OnCultureChanged(); the text localization manager reacts by queuing an
 *    (async) RefreshResources() that reloads the .locres data of the new
 *    culture into its live display-string table.
 *
 *  - FTextLocalizationManager::RefreshResources(Notification) chains a
 *    refresh behind any in-flight refresh and invokes Notification when done.
 *    Notification may run on a non-game thread, so every completion is
 *    marshalled back to the game thread by this class.
 *
 *  - The manager never removes live entries that the new culture's data does
 *    not contain; stale entries from the previously loaded culture survive a
 *    simple culture switch. The FPolyglotTextInjector solves that by writing
 *    neutralising rows (the original English source) for every keyed entry it
 *    knows, which is why the full patch has to be re-injected after every
 *    engine-side refresh (see PolyglotTextInjector.h for details).
 */
class POLYGLOTCORE_API FPolyglotCultureController
{
public:
	FPolyglotCultureController() = default;
	~FPolyglotCultureController() = default;

	// ------------------------------------------------------------------------
	// Lifecycle
	// ------------------------------------------------------------------------

	void Startup();
	void Shutdown();

	// ------------------------------------------------------------------------
	// Culture queries (all static, culture information is process global)
	// ------------------------------------------------------------------------

	/** Canonical name of the language the editor currently displays ("en", "zh-Hans", ...). */
	static FString GetEngineLanguage();

	/** Canonical form of a culture name, eg "zh-hans" -> "zh-Hans". Returns the trimmed input when the engine cannot canonicalize it. */
	static FString Canonicalize(const FString& CultureName);

	/** True when the engine (ICU) knows this culture and it may be activated. */
	static bool IsCultureAvailable(const FString& CultureName);

	// ------------------------------------------------------------------------
	// Switching
	// ------------------------------------------------------------------------

	/**
	 * Switch the editor UI language without restarting.
	 *
	 * When CultureName differs from the current engine language, the engine
	 * reloads its localization resources for the new culture. OnCompletion is
	 * guaranteed to run on the game thread AFTER that reload finished (which is
	 * the safe moment to inject dictionary overrides). When the culture is
	 * already current, no reload happens and OnCompletion is still invoked on
	 * the next game-thread tick so callers always observe a uniform sequence.
	 */
	void SwitchCulture(const FString& CultureName, TFunction<void()>&& OnCompletion);

	// ------------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------------

	/** Fires immediately before SwitchCulture() changes the engine culture. */
	FSimpleMulticastDelegate& OnPreSwitch() { return PreSwitch; }

	/**
	 * Fires whenever the editor culture changed WITHOUT this controller asking
	 * for it (user changed the language in editor preferences, another tool
	 * switched culture, ...). The engine already reloaded its resources by then.
	 */
	FSimpleMulticastDelegate& OnExternalCultureChanged() { return ExternalCultureChanged; }

private:
	void HandleCultureChanged();

	bool bInternalSwitch = false;
	FDelegateHandle CultureChangedHandle;
	FSimpleMulticastDelegate PreSwitch;
	FSimpleMulticastDelegate ExternalCultureChanged;
};
