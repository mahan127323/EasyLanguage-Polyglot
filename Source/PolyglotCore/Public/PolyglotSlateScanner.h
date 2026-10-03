// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"

class FPolyglotDictionary;
class SWidget;
class SWindow;

/**
 * FPolyglotSlateScanner - the Slate scanning layer of the plugin.
 *
 * The localization injector only drives text that flows through FText + culture
 * (engine locres + dictionary ns/key rows). Everything else - text derived from
 * FName / FString / plain STextBlock content - is patched here, on the widget
 * itself, without touching any engine file or asset.
 *
 * Scanning model:
 *   - Windows are collected from FSlateApplication and their widget trees are
 *     visited in fragments so every tick stays inside the configured time
 *     budget (default 2 ms). A full pass is cheap enough to re-run every
 *     ScanIntervalSeconds (default 0.5 s), which picks up newly opened
 *     tabs / dialogs and repopulated list rows.
 *   - Text widgets are recognised by GetType() (see FPolyglotWidgetPatcher) and
 *     only patched when the displayed text is NOT already written in the target
 *     culture's script (English leftovers only) and a dictionary translation
 *     exists.
 *   - A per-widget record table keeps the original source text for every patch.
 *     Comparing the *applied* translation against the current text gives the
 *     dual-state check the whole design relies on: unchanged -> skip, changed ->
 *     the widget was re-used / refreshed with new data, re-derive and re-patch.
 *   - RestoreAllPatchedWidgets() writes every recorded source back and clears
 *     the table; it runs before a language switch and when the plugin is turned
 *     off, so no widget is left touched.
 *
 * Safety rules:
 *   - Widgets under the focused control are never touched (typing is not
 *     interrupted).
 *   - Hidden / collapsed subtrees are skipped.
 *   - Editable text boxes are only patched when explicitly enabled (default
 *     off) because rewriting them can freeze data-bound input.
 *   - Everything runs on the game thread; Slate widgets are never touched from
 *     a worker.
 */
class POLYGLOTCORE_API FPolyglotSlateScanner
{
public:
	FPolyglotSlateScanner() = default;
	~FPolyglotSlateScanner();

	FPolyglotSlateScanner(const FPolyglotSlateScanner&) = delete;
	FPolyglotSlateScanner& operator=(const FPolyglotSlateScanner&) = delete;

	/** Register the ticker + window events. Safe to call once. */
	void Startup();

	/** Unregister everything and restore every patched widget. */
	void Shutdown();

	/**
	 * Feed the active pipeline context. Anything that invalidates previously
	 * cached translations (new dictionary, new culture, new options) triggers a
	 * rescan of every window on the next tick.
	 */
	void Configure(FPolyglotDictionary* InDictionary, const FString& InCulture,
		float InIntervalSeconds, float InTimeBudgetMs,
		bool bInTranslateStaticText, bool bInTranslateEditableText);

	/** Whether scanning is allowed to run (mirrors the subsystem switch). */
	void SetEnabled(bool bInEnabled);
	bool IsEnabled() const { return bEnabled; }

	/** Write every recorded source back to its widget and forget all records. */
	void RestoreAllPatchedWidgets();

	/** Rescan every window on the next tick (culture switch / dictionary reload). */
	void ForceRescanAllWindows();

	/** Dictionary content changed: drop the translation lookup cache and rescan. */
	void OnDictionariesReloaded();

	/**
	 * Schedule one extra full rescan roughly InDelaySeconds from now. Used after
	 * a culture switch finishes: panels that rebuilt / recreated themselves
	 * during the engine refresh may not be in the window queue yet, and this
	 * catches them shortly afterwards. Single-shot; ignored while disabled.
	 */
	void RequestRescanIn(float InDelaySeconds);

private:
	struct FWidgetPatchRecord
	{
		FString Source;      // original displayed text
		FString AppliedText; // what we wrote (for the dual-state check)
	};

	bool HandleTick(float DeltaSeconds);
	void HandleWindowBeingDestroyed(const SWindow& Window);

	bool RunScanRound(double RoundStartSeconds, double& RemainingBudgetSeconds);
	void BuildWindowQueue();
	static void CollectWindows(const TSharedRef<SWindow>& RootWindow, TArray<TSharedRef<SWindow>>& OutWindows);

	void ScanWidget(const TSharedRef<SWidget>& Widget, int32 Depth,
		const TSharedPtr<SWidget>& FocusedWidget,
		double RoundStartSeconds, double& RemainingBudgetSeconds, int32& ProcessedWidgets);
	void MaybePatchTextWidget(const TSharedRef<SWidget>& Widget, const TSharedPtr<SWidget>& FocusedWidget);
	void CleanupExpiredRecords();

	FString LookupTranslation(const FString& SourceText);

	// Pipeline context (never owns anything).
	FPolyglotDictionary* Dictionary = nullptr;
	FString Culture;

	bool bEnabled = false;
	bool bTranslateStaticText = true;
	bool bTranslateEditableText = false;
	float IntervalSeconds = 0.5f;
	float TimeBudgetMs = 2.0f;

	bool bForceNextRound = false;
	double NextScanTime = 0.0;

	TArray<TWeakPtr<SWindow>> WindowQueue;
	int32 NextWindowIndex = 0;

	// Widget -> (source, what we wrote). Weak keys die with their widgets.
	TMap<TWeakPtr<SWidget>, FWidgetPatchRecord> PatchRecords;

	// Source -> translation cache (empty value = known miss). Cleared when the
	// dictionary or culture changes and mass-cleared when it grows too large.
	TMap<FString, FString> TranslationCache;

	int32 ProcessedWidgetsSinceCleanup = 0;

	/** Text widgets patched since the start of the current scan pass. */
	int32 TotalPatchedThisPass = 0;

	/** Widgets visited since the start of the current scan pass (diagnostics). */
	int32 ProcessedWidgetsThisPass = 0;

	/** One extra full rescan scheduled after a culture switch (see RequestRescanIn). */
	bool bDeferredRescanPending = false;
	double DeferredRescanTime = 0.0;

	/** Last time the culture-mismatch pause was reported (log throttling). */
	double LastCultureMismatchLogTime = 0.0;

	FTSTicker::FDelegateHandle TickerHandle;
	FDelegateHandle WindowBeingDestroyedHandle;
};
