// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "PolyglotTypes.h"
#include "PolyglotSubsystem.generated.h"

class FPolyglotCultureController;
class FPolyglotDictionary;
class FPolyglotTextInjector;
class FPolyglotSlateScanner;
struct FPolyglotPipeline;

/** Broadcast when the editor finished switching (or reverting) its culture and
 *  the dictionaries/scanner are already re-applied. Parameter = the engine
 *  language that is now active (e.g. "en", "zh-Hans"). UI panels subscribe to
 *  refresh their own bilingual strings. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPolyglotCultureApplied, const FString& /*NewEngineCulture*/);

/**
 * UPolyglotSubsystem - central coordinator of the whole plugin.
 *
 * Lifecycle: created automatically by the Editor subsystem manager when the
 * PolyglotCore module is loaded (the plugin ships only for the Editor, so this
 * never runs in a packaged game).
 *
 * Responsibility: own the dictionary / culture / slate-scan pipeline objects,
 * expose the high level operations used by the UI (switch language, pick text,
 * reload dictionaries, enable/disable).
 *
 * The pipeline object instances are stored behind an opaque FPolyglotPipeline
 * unique pointer. Its destructor is defined in the .cpp where every pipeline
 * type is complete, which keeps the generated reflection code (which only sees
 * this header) free of incomplete-type deletes.
 */
UCLASS()
class POLYGLOTCORE_API UPolyglotSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UPolyglotSubsystem();

	/** Convenience accessor; valid only after the editor subsystem collection is up. */
	static UPolyglotSubsystem* Get();

	virtual ~UPolyglotSubsystem() override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Switch the whole editor UI to the given culture (eg "zh-Hans", "en", "ja"). Returns false if unsupported. */
	bool RequestCulture(const FString& CultureName);

	/** Toggle between simplified Chinese and English (keeps English when culture is currently Chinese). */
	void ToggleChineseEnglish();

	/** Current active culture name ("" while unset). */
	FString GetActiveCulture() const;

	/** Culture the engine is actually displaying right now ("" if unavailable). */
	FString GetEngineLanguage() const;

	/** Whether translations are currently being applied. */
	bool IsEnabled() const;

	/** Enable / disable the whole pipeline (disable restores every touched widget). */
	void SetEnabled(bool bEnabled);

	/** Reload dictionaries from disk and re-apply them. */
	void ReloadDictionaries();

	/** Force every open window to be rescanned on the next tick. */
	void ForceRescanAllWindows();

	/** Restore every widget that the slate scanner patched back to its original text. */
	void RestoreAllWidgets();

	// ------------------------------------------------------------------------
	// Scan layer options (read/write from the settings page).
	// ------------------------------------------------------------------------

	float GetScanIntervalSeconds() const { return ScanIntervalSeconds; }
	float GetScanTimeBudgetMs() const { return ScanTimeBudgetMs; }
	bool GetTranslateStaticTextBlocks() const { return bTranslateStaticTextBlocks; }
	bool GetTranslateEditableText() const { return bTranslateEditableText; }

	/** Update scan options, persist them and push them into the scanner. */
	void SetScanParameters(float IntervalSeconds, float TimeBudgetMs, bool bStatic, bool bEditable);

	// ------------------------------------------------------------------------
	// Dictionary access used by the UI, picker and adapters.
	// ------------------------------------------------------------------------

	/** The active dictionary (owned by the subsystem, never null after init). */
	FPolyglotDictionary* GetDictionary() const;

	/** Translate a plain visible string using the active culture. */
	bool TranslateSource(const FString& SourceText, FString& OutTranslated) const;

	/** Add / update / remove an entry in the main user dictionary. */
	bool AddOrUpdateUserEntry(const FPolyglotEntry& Entry);
	bool RemoveUserEntry(const FPolyglotEntry& Entry);

	// ------------------------------------------------------------------------
	// Culture-change notification (used by bilingual editor windows).
	// ------------------------------------------------------------------------

	/** Broadcast after the engine finished a culture switch/revert (widgets already rescanned). */
	FOnPolyglotCultureApplied& OnCultureApplied() { return CultureApplied; }

private:
	/** Build / rebuild internal pipeline objects. */
	void InitPipeline();
	void ShutdownPipeline();

	/**
	 * Run the full "make the editor speak CultureName" sequence:
	 * restore widget patches -> engine culture switch -> refresh -> inject the
	 * dictionary overrides -> ask the scanner layer to rescan every window.
	 * Only meaningful while translations are enabled.
	 */
	void ApplyCultureFor(const FString& CultureName);

	/** Game-thread continuation of ApplyCultureFor: run right after the engine finished refreshing. */
	void OnCultureSwitchApplied_GameThread();

	/** Engine bumped the text revision (culture refresh, locres mount, our own injection ...). */
	void HandleTextRevisionChanged();

	/** Switch back to the editor's original language when translations are turned off. */
	void RevertToDefaultLanguage();

	/** Re-apply dictionary overrides + rescan after dictionary content changed. */
	void ReapplyAfterDictionaryChange();

	/** Push culture + scan options into the scanner layer (called on every change). */
	void UpdateScannerContext();

	/** Persist the active culture to the per-project settings file. */
	void SaveState() const;
	void LoadState();

	/** True while the subsystem itself writes into the localization live table. */
	bool bInternalTextUpdate = false;

	/** Language the editor booted with (what disabling translations restores). */
	FString DefaultEditorLanguage;

	// Scan layer configuration (persisted; editable from the settings page).
	float ScanIntervalSeconds = 0.5f;
	float ScanTimeBudgetMs = 2.0f;
	bool bTranslateStaticTextBlocks = true;
	bool bTranslateEditableText = false;

	FDelegateHandle TextRevisionChangedHandle;

	bool bEnabled = false;
	FString ActiveCulture;

	/**
	 * Heap-allocated pipeline (opaque), created and destroyed only from the .cpp
	 * where FPolyglotPipeline is complete. A raw pointer is used on purpose:
	 * UHT-generated code sees this header only and must not instantiate a delete
	 * of an incomplete type.
	 */
	FPolyglotPipeline* Pipeline = nullptr;

	FOnPolyglotCultureApplied CultureApplied;
};
