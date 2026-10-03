// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "PolyglotSlateScanner.h"

#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"

#include "PolyglotCompat.h"
#include "PolyglotCultureController.h"
#include "PolyglotDictionary.h"
#include "PolyglotLog.h"
#include "PolyglotWidgetPatcher.h"

namespace
{
	constexpr int32 MaxWidgetDepth = 200;
	constexpr int32 TranslationCacheLimit = 8192;
	constexpr int32 CleanupIntervalWidgets = 512;

	/**
	 * Time budget applied per tick while a forced rescan is running. A forced
	 * rescan is a low-frequency user action (language switch, dictionary edit,
	 * reload), so a one-off generous budget lets the whole editor window tree
	 * finish within a handful of frames instead of trickling behind the regular
	 * poll cadence. The configured budget resumes right after the pass ends.
	 */
	constexpr float ForcedRescanBudgetMs = 50.0f;

	/** True for the CJK cultures we ship dictionaries for. */
	bool IsCjkCulture(const FString& CultureName)
	{
		return CultureName.StartsWith(TEXT("zh"))
			|| CultureName.StartsWith(TEXT("ja"))
			|| CultureName.StartsWith(TEXT("ko"));
	}

	/**
	 * Does the text contain characters from East-Asian scripts? When the active
	 * culture is CJK this is our "already localised / is our own translation"
	 * signal and the widget is left alone.
	 */
	bool ContainsEastAsianText(const FString& InText)
	{
		for (const TCHAR Char : InText)
		{
			const uint32 CodePoint = static_cast<uint32>(Char);
			if ((CodePoint >= 0x2E80 && CodePoint <= 0x9FFF)   // CJK radicals + unified ideographs
				|| (CodePoint >= 0x3040 && CodePoint <= 0x30FF) // Hiragana + Katakana
				|| (CodePoint >= 0xAC00 && CodePoint <= 0xD7AF) // Hangul
				|| (CodePoint >= 0xF900 && CodePoint <= 0xFAFF) // compatibility ideographs
				|| (CodePoint >= 0x3400 && CodePoint <= 0x4DBF)) // extension A
			{
				return true;
			}
		}
		return false;
	}
}

FPolyglotSlateScanner::~FPolyglotSlateScanner()
{
	Shutdown();
}

void FPolyglotSlateScanner::Startup()
{
	if (TickerHandle.IsValid() || !FSlateApplication::IsInitialized())
	{
		return;
	}

	WindowBeingDestroyedHandle = FSlateApplication::Get().OnWindowBeingDestroyed().AddRaw(
		this, &FPolyglotSlateScanner::HandleWindowBeingDestroyed);

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FPolyglotSlateScanner::HandleTick));
}

void FPolyglotSlateScanner::Shutdown()
{
	RestoreAllPatchedWidgets();

	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	if (WindowBeingDestroyedHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnWindowBeingDestroyed().Remove(WindowBeingDestroyedHandle);
		WindowBeingDestroyedHandle.Reset();
	}
}

void FPolyglotSlateScanner::Configure(FPolyglotDictionary* InDictionary, const FString& InCulture,
	float InIntervalSeconds, float InTimeBudgetMs,
	bool bInTranslateStaticText, bool bInTranslateEditableText)
{
	const bool bContextChanged = (Dictionary != InDictionary) || !Culture.Equals(InCulture);
	const bool bOptionsChanged = (IntervalSeconds != InIntervalSeconds)
		|| (TimeBudgetMs != InTimeBudgetMs)
		|| (bTranslateStaticText != bInTranslateStaticText)
		|| (bTranslateEditableText != bInTranslateEditableText);

	Dictionary = InDictionary;
	Culture = InCulture;
	IntervalSeconds = FMath::Max(0.05f, InIntervalSeconds);
	TimeBudgetMs = FMath::Clamp(InTimeBudgetMs, 0.1f, 100.0f);
	bTranslateStaticText = bInTranslateStaticText;
	bTranslateEditableText = bInTranslateEditableText;

	if (bContextChanged)
	{
		TranslationCache.Reset();
	}
	if (bContextChanged || bOptionsChanged)
	{
		ForceRescanAllWindows();
	}

	UE_LOG(LogPolyglot, Display,
		TEXT("Slate scanner configured: culture='%s', static=%d, editable=%d, interval=%.2fs, budget=%.1fms."),
		*Culture, bTranslateStaticText ? 1 : 0, bTranslateEditableText ? 1 : 0, IntervalSeconds, TimeBudgetMs);
}

void FPolyglotSlateScanner::SetEnabled(bool bInEnabled)
{
	if (bEnabled == bInEnabled)
	{
		return;
	}
	bEnabled = bInEnabled;
	if (bEnabled)
	{
		bForceNextRound = true;
		NextScanTime = 0.0;
	}
	else
	{
		// Do not restore here: the disable flow restores at its own cadence.
		WindowQueue.Reset();
		NextWindowIndex = 0;
		bDeferredRescanPending = false;
	}
}

void FPolyglotSlateScanner::RestoreAllPatchedWidgets()
{
	if (PatchRecords.IsEmpty())
	{
		return;
	}

	// Copy the keys first: SetWidgetText may destroy the very widgets we patch
	// (closing windows etc.) which would invalidate the map we are iterating.
	TArray<TWeakPtr<SWidget>> Records;
	Records.Reserve(PatchRecords.Num());
	for (const TPair<TWeakPtr<SWidget>, FWidgetPatchRecord>& Pair : PatchRecords)
	{
		Records.Add(Pair.Key);
	}

	int32 Restored = 0;
	for (const TWeakPtr<SWidget>& WeakWidget : Records)
	{
		const TSharedPtr<SWidget> Widget = WeakWidget.Pin();
		if (!Widget.IsValid())
		{
			continue;
		}
		const FWidgetPatchRecord* Record = PatchRecords.Find(WeakWidget);
		if (!Record)
		{
			continue;
		}
		if (FPolyglotWidgetPatcher::SetWidgetText(Widget.ToSharedRef(), Record->Source))
		{
			++Restored;
		}
	}

	PatchRecords.Reset();
	TranslationCache.Reset();
	WindowQueue.Reset();
	NextWindowIndex = 0;

	UE_LOG(LogPolyglot, Verbose, TEXT("Slate scanner restored %d widget(s)."), Restored);
}

void FPolyglotSlateScanner::ForceRescanAllWindows()
{
	bForceNextRound = true;
	NextScanTime = 0.0;
	WindowQueue.Reset();
	NextWindowIndex = 0;
}

void FPolyglotSlateScanner::OnDictionariesReloaded()
{
	TranslationCache.Reset();
	ForceRescanAllWindows();
}

void FPolyglotSlateScanner::RequestRescanIn(float InDelaySeconds)
{
	if (!bEnabled)
	{
		return;
	}
	bDeferredRescanPending = true;
	DeferredRescanTime = FPlatformTime::Seconds() + FMath::Max(0.05f, InDelaySeconds);
}

// ---------------------------------------------------------------------------
// Ticker / scheduling
// ---------------------------------------------------------------------------

bool FPolyglotSlateScanner::HandleTick(float DeltaSeconds)
{
	if (!bEnabled || !Dictionary || Culture.IsEmpty())
	{
		return true;
	}

	// English needs no scanning (the neutralising patch + engine data already
	// show English), and while the engine is still switching we must not race it.
	if (Culture.StartsWith(TEXT("en")))
	{
		return true;
	}

	// While the engine language differs from the dictionary culture (e.g. the
	// editor is still on English) the scan pass must wait for the switch.
	const FString EngineLanguage = FPolyglotCultureController::GetEngineLanguage();
	if (!Culture.Equals(EngineLanguage, ESearchCase::IgnoreCase))
	{
		const double NowLog = FPlatformTime::Seconds();
		if (NowLog - LastCultureMismatchLogTime > 5.0)
		{
			LastCultureMismatchLogTime = NowLog;
			UE_LOG(LogPolyglot, Display,
				TEXT("Slate scanner paused: dictionary culture '%s' differs from the engine language '%s'. Switch the editor to the target language first."),
				*Culture, *EngineLanguage);
		}
		return true;
	}
	if (!FSlateApplication::IsInitialized())
	{
		return true;
	}

	const double Now = FPlatformTime::Seconds();

	// A rescan requested "shortly after the switch" (RequestRescanIn) fires once.
	// Placed after the pause checks so a switch that is still in flight simply
	// lets the pending flag survive until the engine actually matches.
	if (bDeferredRescanPending && Now >= DeferredRescanTime)
	{
		bDeferredRescanPending = false;
		ForceRescanAllWindows();
	}

	if (!bForceNextRound && Now < NextScanTime)
	{
		return true;
	}

	// Forced rescans (language switch / dictionary change / reload) run with a
	// generous one-off budget so the whole window tree is visited within a few
	// frames; regular polls keep the user-configured budget.
	const bool bWasForced = bForceNextRound;
	double BudgetSeconds =
		(bWasForced ? static_cast<double>(ForcedRescanBudgetMs) : static_cast<double>(TimeBudgetMs)) / 1000.0;

	const bool bFinished = RunScanRound(Now, BudgetSeconds);

	if (bFinished)
	{
		const int32 PatchedThisPass = TotalPatchedThisPass;
		const int32 ProcessedThisPass = ProcessedWidgetsThisPass;
		if (bWasForced)
		{
			UE_LOG(LogPolyglot, Display,
				TEXT("Slate scanner force pass complete: processed %d widget(s), patched %d text widget(s)."),
				ProcessedThisPass, PatchedThisPass);
			if (IsCjkCulture(Culture) && ProcessedThisPass >= 1000 && PatchedThisPass == 0)
			{
				UE_LOG(LogPolyglot, Display,
					TEXT("No patchable text found for culture '%s' after a full forced pass - check that the user dictionary contains the visible source strings under this culture, and that picked text matches exactly (run with -LogCmds=\"LogPolyglot Verbose\" to see per-string misses)."),
					*Culture);
			}
		}
		else if (PatchedThisPass > 0)
		{
			UE_LOG(LogPolyglot, Display, TEXT("Slate scanner pass complete: patched %d text widget(s)."), PatchedThisPass);
		}
		TotalPatchedThisPass = 0;
		ProcessedWidgetsThisPass = 0;
		bForceNextRound = false;
		NextScanTime = Now + static_cast<double>(IntervalSeconds);
	}
	else
	{
		// More work queued: continue on the next tick.
		NextScanTime = Now;
	}
	return true;
}

bool FPolyglotSlateScanner::RunScanRound(double RoundStartSeconds, double& RemainingBudgetSeconds)
{
	if (NextWindowIndex >= WindowQueue.Num())
	{
		// A fresh queue starts a new scan pass.
		TotalPatchedThisPass = 0;
		ProcessedWidgetsThisPass = 0;
		BuildWindowQueue();
		NextWindowIndex = 0;
	}

	const TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetUserFocusedWidget(0);
	int32 ProcessedWidgets = 0;

	while (NextWindowIndex < WindowQueue.Num() && RemainingBudgetSeconds > 0.0)
	{
		const TSharedPtr<SWindow> Window = WindowQueue[NextWindowIndex].Pin();
		++NextWindowIndex;
		if (!Window.IsValid())
		{
			continue;
		}
		if (!Window->IsVisible())
		{
			continue;
		}

		const TSharedRef<SWidget> Content = Window->GetContent();
		ScanWidget(Content, 0, FocusedWidget, RoundStartSeconds, RemainingBudgetSeconds, ProcessedWidgets);
	}

	ProcessedWidgetsThisPass += ProcessedWidgets;
	return NextWindowIndex >= WindowQueue.Num();
}

void FPolyglotSlateScanner::BuildWindowQueue()
{
	WindowQueue.Reset();
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	TArray<TSharedRef<SWindow>> Windows;
	for (const TSharedRef<SWindow>& RootWindow : FSlateApplication::Get().GetInteractiveTopLevelWindows())
	{
		CollectWindows(RootWindow, Windows);
	}

	WindowQueue.Reserve(Windows.Num());
	for (const TSharedRef<SWindow>& Window : Windows)
	{
		WindowQueue.Add(TWeakPtr<SWindow>(Window));
	}
}

void FPolyglotSlateScanner::CollectWindows(const TSharedRef<SWindow>& RootWindow, TArray<TSharedRef<SWindow>>& OutWindows)
{
	OutWindows.Add(RootWindow);
	for (const TSharedRef<SWindow>& ChildWindow : RootWindow->GetChildWindows())
	{
		CollectWindows(ChildWindow, OutWindows);
	}
}

// ---------------------------------------------------------------------------
// Widget tree walk
// ---------------------------------------------------------------------------

void FPolyglotSlateScanner::ScanWidget(const TSharedRef<SWidget>& Widget, int32 Depth,
	const TSharedPtr<SWidget>& FocusedWidget,
	double RoundStartSeconds, double& RemainingBudgetSeconds, int32& ProcessedWidgets)
{
	if (RemainingBudgetSeconds <= 0.0 || Depth > MaxWidgetDepth)
	{
		return;
	}

	// Time check: sample the wall clock every 64 widgets.
	++ProcessedWidgets;
	++ProcessedWidgetsSinceCleanup;
	if (ProcessedWidgetsSinceCleanup >= CleanupIntervalWidgets)
	{
		CleanupExpiredRecords();
		ProcessedWidgetsSinceCleanup = 0;
	}
	if ((ProcessedWidgets & 0x3F) == 0)
	{
		RemainingBudgetSeconds -= (FPlatformTime::Seconds() - RoundStartSeconds);
	}

	// Do not descend into hidden / collapsed subtrees.
	const EVisibility Visibility = Widget->GetVisibility();
	if (Visibility == EVisibility::Hidden || Visibility == EVisibility::Collapsed)
	{
		return;
	}

	// Never touch a control that owns the keyboard focus (typing / editing).
	if (FocusedWidget.IsValid() && FocusedWidget == Widget)
	{
		return;
	}

	// Patch this widget when its type is a text widget we translate.
	const FName WidgetTypeName = Widget->GetType();
	if ((bTranslateStaticText && FPolyglotWidgetPatcher::IsStaticTextType(WidgetTypeName))
		|| (bTranslateEditableText && FPolyglotWidgetPatcher::IsEditableTextType(WidgetTypeName)))
	{
		MaybePatchTextWidget(Widget, FocusedWidget);
	}

	// Recurse into children.
	const FChildren* Children = Widget->GetChildren();
	if (!Children)
	{
		return;
	}
	const int32 ChildCount = Children->Num();
	for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
	{
		// GetChildAt hands out a const child reference in UE 5.8; the widgets
		// belong to their parent and are non-const in reality, so cast back.
		const TSharedRef<SWidget> Child = ConstCastSharedRef<SWidget>(Children->GetChildAt(ChildIndex));
		if (RemainingBudgetSeconds <= 0.0)
		{
			return;
		}
		ScanWidget(Child, Depth + 1, FocusedWidget, RoundStartSeconds, RemainingBudgetSeconds, ProcessedWidgets);
	}
}

void FPolyglotSlateScanner::MaybePatchTextWidget(const TSharedRef<SWidget>& Widget, const TSharedPtr<SWidget>& FocusedWidget)
{
	if (FocusedWidget.IsValid() && FocusedWidget == Widget)
	{
		return;
	}

	FText DisplayedText;
	if (!FPolyglotWidgetPatcher::GetWidgetText(Widget, DisplayedText))
	{
		return;
	}

	const FString CurrentText = DisplayedText.ToString();
	if (CurrentText.IsEmpty())
	{
		return;
	}

	const TWeakPtr<SWidget> WeakWidget(Widget);
	const FWidgetPatchRecord* Existing = PatchRecords.Find(WeakWidget);

	// Dual-state check: still showing exactly what we wrote -> nothing to do.
	if (Existing && Existing->AppliedText == CurrentText)
	{
		return;
	}

	// Text written in the target culture's script is localised already (either
	// by the engine, the override patch, or a previous pass of ours). Drop any
	// stale record - the source we knew is gone - and move on.
	if (IsCjkCulture(Culture) && ContainsEastAsianText(CurrentText))
	{
		if (Existing)
		{
			PatchRecords.Remove(WeakWidget);
		}
		return;
	}

	// Treat the current text as a fresh source candidate (row reuse / refresh).
	const FString SourceText = CurrentText;
	const FString Translation = LookupTranslation(SourceText);
	if (Translation.IsEmpty() || Translation.Equals(SourceText))
	{
		if (Existing && Existing->AppliedText != SourceText)
		{
			PatchRecords.Remove(WeakWidget);
		}
		return;
	}

	if (!FPolyglotWidgetPatcher::SetWidgetText(Widget, Translation))
	{
		UE_LOG(LogPolyglot, Verbose, TEXT("Slate scanner: translation exists but the widget rejected it (%s, source '%s')."),
			*Widget->GetType().ToString(), *SourceText);
		return;
	}

	FWidgetPatchRecord Record;
	Record.Source = SourceText;
	Record.AppliedText = Translation;
	PatchRecords.Emplace(WeakWidget, MoveTemp(Record));
	++TotalPatchedThisPass;
	UE_LOG(LogPolyglot, Verbose, TEXT("Slate scanner patched %s: '%s' -> '%s'."),
		*Widget->GetType().ToString(), *SourceText, *Translation);
}

FString FPolyglotSlateScanner::LookupTranslation(const FString& SourceText)
{
	if (SourceText.IsEmpty())
	{
		return FString();
	}

	const FString* Cached = TranslationCache.Find(SourceText);
	if (Cached)
	{
		return *Cached;
	}

	FString Translation;
	if (!Dictionary || !Dictionary->TranslateSource(Culture, SourceText, Translation))
	{
		Translation.Reset();
		// One line per unseen source string - the cache records the miss, so a
		// repeated scan does not spam the log. This is the primary clue when a
		// third-party label is not translated: either it is missing from the
		// dictionary for this culture, or the visible text differs from the
		// entry source (whitespace, separators, markup, live refresh ...).
		UE_LOG(LogPolyglot, Verbose,
			TEXT("Slate scanner: no translation for '%s' under culture '%s'. Check the user dictionary, or that the picked source matches the visible text exactly."),
			*SourceText, *Culture);
	}

	if (TranslationCache.Num() >= TranslationCacheLimit)
	{
		TranslationCache.Reset();
	}
	TranslationCache.Emplace(SourceText, Translation);
	return Translation;
}

void FPolyglotSlateScanner::HandleWindowBeingDestroyed(const SWindow& Window)
{
	// Nothing specific to do: widget records use weak pointers and die with
	// their widgets (expired entries are cleaned on the next cleanup pass).
	// Rescanning is scheduled so remaining windows are visited promptly.
	if (bEnabled)
	{
		bForceNextRound = true;
		NextScanTime = 0.0;
	}
}

void FPolyglotSlateScanner::CleanupExpiredRecords()
{
	if (PatchRecords.IsEmpty())
	{
		return;
	}
	for (auto It = PatchRecords.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
