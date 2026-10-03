// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "PolyglotSubsystem.h"

#include "Editor.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationManager.h"

#include "PolyglotCompat.h"
#include "PolyglotCultureController.h"
#include "PolyglotDictionary.h"
#include "PolyglotLog.h"
#include "PolyglotSettings.h"
#include "PolyglotSlateScanner.h"
#include "PolyglotTextInjector.h"

#if WITH_EDITOR
#include "Subsystems/SubsystemCollection.h"
#endif

// ---------------------------------------------------------------------------
// Pipeline container.
//
// All pipeline objects live behind this opaque struct so that the header of the
// subsystem never needs the full definition of every pipeline class. The struct
// is defined here where all those types are complete.
// ---------------------------------------------------------------------------
struct FPolyglotPipeline
{
	TUniquePtr<FPolyglotDictionary> Dictionary;
	TUniquePtr<FPolyglotTextInjector> Injector;
	TUniquePtr<FPolyglotCultureController> Culture;
	TUniquePtr<FPolyglotSlateScanner> Scanner;
};

// Out-of-line destructor: compiled here, where FPolyglotPipeline is complete.
UPolyglotSubsystem::~UPolyglotSubsystem()
{
	ShutdownPipeline();
}

UPolyglotSubsystem::UPolyglotSubsystem()
{
}

void UPolyglotSubsystem::InitPipeline()
{
	if (Pipeline)
	{
		return;
	}

	Pipeline = new FPolyglotPipeline();
	Pipeline->Dictionary = MakeUnique<FPolyglotDictionary>();
	Pipeline->Dictionary->ReloadAll();

	Pipeline->Injector = MakeUnique<FPolyglotTextInjector>();
	Pipeline->Injector->SetDictionary(Pipeline->Dictionary.Get());

	Pipeline->Culture = MakeUnique<FPolyglotCultureController>();
	Pipeline->Culture->Startup();

	Pipeline->Scanner = MakeUnique<FPolyglotSlateScanner>();
	Pipeline->Scanner->Startup();
	UpdateScannerContext();
	Pipeline->Scanner->SetEnabled(bEnabled);

	// Capture the language the editor booted with. Turning the plugin off
	// restores exactly this (and not blindly "en", which would stomp over a
	// project that legitimately boots in Chinese or Japanese).
	DefaultEditorLanguage = FPolyglotCultureController::GetEngineLanguage();

	// Subscribe to text revision bumps. This is what makes the override patch
	// self-healing: whenever something (culture refresh, additional LocRes
	// mount, localization dashboard...) reloads engine text data and wipes our
	// one-shot injection, the next bump re-applies it.
	TextRevisionChangedHandle = FTextLocalizationManager::Get().OnTextRevisionChangedEvent.AddUObject(
		this, &UPolyglotSubsystem::HandleTextRevisionChanged);
}

void UPolyglotSubsystem::ShutdownPipeline()
{
	if (Pipeline)
	{
		if (TextRevisionChangedHandle.IsValid())
		{
			FTextLocalizationManager::Get().OnTextRevisionChangedEvent.Remove(TextRevisionChangedHandle);
			TextRevisionChangedHandle.Reset();
		}

		Pipeline->Scanner->Shutdown();
		Pipeline->Scanner.Reset();

		Pipeline->Culture->Shutdown();
		Pipeline->Culture.Reset();
		Pipeline->Injector.Reset();
		Pipeline->Dictionary.Reset();

		delete Pipeline;
		Pipeline = nullptr;
	}
}

UPolyglotSubsystem* UPolyglotSubsystem::Get()
{
#if WITH_EDITOR
	if (GEditor)
	{
		return GEditor->GetEditorSubsystem<UPolyglotSubsystem>();
	}
#endif
	return nullptr;
}

void UPolyglotSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!FModuleManager::Get().IsModuleLoaded(TEXT("PolyglotCore")))
	{
		UE_LOG(LogPolyglot, Error, TEXT("PolyglotCore is not loaded while PolyglotSubsystem initializes."));
		return;
	}

	LoadState();
	InitPipeline();

	// Restore the last session when translations were left enabled: apply the
	// saved culture right away so the whole editor UI boots in that language.
	if (bEnabled && !ActiveCulture.IsEmpty() && FPolyglotCultureController::IsCultureAvailable(ActiveCulture))
	{
		UE_LOG(LogPolyglot, Display, TEXT("Polyglot re-applying culture '%s' at editor startup."), *ActiveCulture);
		ApplyCultureFor(ActiveCulture);
	}

	UE_LOG(LogPolyglot, Display, TEXT("Polyglot subsystem initialized (enabled=%d, culture=%s, engine-default=%s)."),
		bEnabled ? 1 : 0, *ActiveCulture, *DefaultEditorLanguage);
}

void UPolyglotSubsystem::Deinitialize()
{
	if (Pipeline)
	{
		// Translation state never survives an editor restart by itself (the
		// culture change is runtime-only), so nothing to restore here.
	}

	ShutdownPipeline();

	Super::Deinitialize();

	UE_LOG(LogPolyglot, Display, TEXT("Polyglot subsystem deinitialized."));
}

bool UPolyglotSubsystem::RequestCulture(const FString& CultureName)
{
	if (!Pipeline || !Pipeline->Culture)
	{
		return false;
	}

	const FString Canonical = FPolyglotCultureController::Canonicalize(CultureName);
	if (Canonical.IsEmpty())
	{
		return false;
	}

	if (!FPolyglotCultureController::IsCultureAvailable(Canonical))
	{
		UE_LOG(LogPolyglot, Warning, TEXT("Requested culture '%s' is not available in this engine."), *CultureName);
		return false;
	}

	ActiveCulture = Canonical;
	SaveState();
	UpdateScannerContext();

	if (bEnabled)
	{
		ApplyCultureFor(ActiveCulture);
	}

	return true;
}

void UPolyglotSubsystem::ToggleChineseEnglish()
{
	RequestCulture(ActiveCulture == Polyglot::Compat::Culture_ChineseSimplified
		? Polyglot::Compat::Culture_English
		: Polyglot::Compat::Culture_ChineseSimplified);
}

FString UPolyglotSubsystem::GetActiveCulture() const
{
	return ActiveCulture;
}

FString UPolyglotSubsystem::GetEngineLanguage() const
{
	return FPolyglotCultureController::GetEngineLanguage();
}

bool UPolyglotSubsystem::IsEnabled() const
{
	return bEnabled;
}

void UPolyglotSubsystem::SetEnabled(bool bInEnabled)
{
	if (bEnabled == bInEnabled)
	{
		return;
	}

	bEnabled = bInEnabled;
	SaveState();

	if (Pipeline && Pipeline->Scanner)
	{
		Pipeline->Scanner->SetEnabled(bEnabled);
	}

	if (bEnabled)
	{
		// Put back whatever language the user last chose.
		FString Target = !ActiveCulture.IsEmpty() ? ActiveCulture : Polyglot::Compat::Culture_ChineseSimplified;
		if (ActiveCulture.IsEmpty())
		{
			ActiveCulture = Target;
			SaveState();
		}
		UpdateScannerContext();
		ApplyCultureFor(Target);
	}
	else
	{
		RevertToDefaultLanguage();
	}
}

void UPolyglotSubsystem::ReloadDictionaries()
{
	if (!Pipeline || !Pipeline->Dictionary)
	{
		return;
	}

	Pipeline->Dictionary->ReloadAll();
	ReapplyAfterDictionaryChange();
}

void UPolyglotSubsystem::SetScanParameters(float IntervalSeconds, float TimeBudgetMs, bool bStatic, bool bEditable)
{
	ScanIntervalSeconds = FMath::Clamp(IntervalSeconds, 0.1f, 60.0f);
	ScanTimeBudgetMs = FMath::Clamp(TimeBudgetMs, 0.1f, 50.0f);
	bTranslateStaticTextBlocks = bStatic;
	bTranslateEditableText = bEditable;

	SaveState();
	UpdateScannerContext();
}

void UPolyglotSubsystem::ForceRescanAllWindows()
{
	if (Pipeline && Pipeline->Scanner)
	{
		Pipeline->Scanner->ForceRescanAllWindows();
	}
}

void UPolyglotSubsystem::RestoreAllWidgets()
{
	if (Pipeline && Pipeline->Scanner)
	{
		Pipeline->Scanner->RestoreAllPatchedWidgets();
	}
}

FPolyglotDictionary* UPolyglotSubsystem::GetDictionary() const
{
	return Pipeline ? Pipeline->Dictionary.Get() : nullptr;
}

bool UPolyglotSubsystem::TranslateSource(const FString& SourceText, FString& OutTranslated) const
{
	if (!Pipeline || !Pipeline->Dictionary || ActiveCulture.IsEmpty())
	{
		return false;
	}
	return Pipeline->Dictionary->TranslateSource(ActiveCulture, SourceText, OutTranslated);
}

bool UPolyglotSubsystem::AddOrUpdateUserEntry(const FPolyglotEntry& Entry)
{
	if (!Pipeline || !Pipeline->Dictionary)
	{
		return false;
	}
	const bool bChanged = Pipeline->Dictionary->AddOrUpdateUserEntry(Entry);
	if (bChanged)
	{
		ReapplyAfterDictionaryChange();
	}
	return bChanged;
}

bool UPolyglotSubsystem::RemoveUserEntry(const FPolyglotEntry& Entry)
{
	if (!Pipeline || !Pipeline->Dictionary)
	{
		return false;
	}
	const bool bChanged = Pipeline->Dictionary->RemoveUserEntry(Entry);
	if (bChanged)
	{
		ReapplyAfterDictionaryChange();
	}
	return bChanged;
}

// ---------------------------------------------------------------------------
// Culture application flow.
// ---------------------------------------------------------------------------

void UPolyglotSubsystem::ApplyCultureFor(const FString& CultureName)
{
	if (!Pipeline || !Pipeline->Culture || !Pipeline->Injector)
	{
		return;
	}
	if (CultureName.IsEmpty())
	{
		return;
	}

	// Restore previously patched Slate widgets so no stale widget text survives
	// the language change; the scanner re-patches after the switch.
	RestoreAllWidgets();

	Pipeline->Culture->SwitchCulture(CultureName,
		[this]()
		{
			OnCultureSwitchApplied_GameThread();
		});
}

void UPolyglotSubsystem::OnCultureSwitchApplied_GameThread()
{
	if (!Pipeline || !Pipeline->Injector || !bEnabled)
	{
		return;
	}
	if (ActiveCulture.IsEmpty())
	{
		return;
	}

	// Engine resources for the active culture are merged at this point. Inject
	// (or re-inject) the dictionary overrides. The revision bump produced by the
	// injection is swallowed by the guard in HandleTextRevisionChanged.
	bInternalTextUpdate = true;
	Pipeline->Injector->ApplyPatch(ActiveCulture);
	bInternalTextUpdate = false;

	// Widget-level text that does not flow through the localization system is
	// (re)patched by the scanning layer. Keep the tail identical to the proven
	// dictionary-edit path (ReapplyAfterDictionaryChange): drop the lookup cache
	// (a stale entry from the previous culture must not survive) and rescan
	// every window. Third-party panels that rebuilt themselves during the engine
	// refresh can take a moment to reappear, so one deferred rescan catches them.
	if (Pipeline->Scanner)
	{
		Pipeline->Scanner->OnDictionariesReloaded();
		Pipeline->Scanner->RequestRescanIn(1.0f);
	}

	UE_LOG(LogPolyglot, Display,
		TEXT("Culture switch applied: engine='%s', dictionary culture='%s', widget scanner restarted (full fast rescan scheduled)."),
		*GetEngineLanguage(), *ActiveCulture);

	// Notify bilingual editor windows that the culture finished applying so they
	// can refresh their own UI strings.
	CultureApplied.Broadcast(GetEngineLanguage());
}

void UPolyglotSubsystem::RevertToDefaultLanguage()
{
	if (!Pipeline || !Pipeline->Culture || !Pipeline->Injector)
	{
		return;
	}

	RestoreAllWidgets();

	// Switch back to the boot language and neutralise every keyed override so
	// stale rows can not survive the engine refresh.
	Pipeline->Culture->SwitchCulture(DefaultEditorLanguage,
		[this]()
		{
			if (!Pipeline || !Pipeline->Injector)
			{
				return;
			}
			bInternalTextUpdate = true;
			Pipeline->Injector->ApplyPatch(DefaultEditorLanguage);
			bInternalTextUpdate = false;
			if (Pipeline->Scanner)
			{
				Pipeline->Scanner->OnDictionariesReloaded();
			}

			CultureApplied.Broadcast(GetEngineLanguage());
		});
}

void UPolyglotSubsystem::HandleTextRevisionChanged()
{
	if (!Pipeline || !Pipeline->Culture || !Pipeline->Injector)
	{
		return;
	}
	if (bInternalTextUpdate)
	{
		// Revision bump caused by our own injection - nothing to do.
		return;
	}
	if (!bEnabled)
	{
		return;
	}

	// Any bump that we did not cause means the engine reloaded / replaced text
	// data and our one-shot injection may have been wiped. Re-apply it. Also
	// follow an externally chosen culture so the plugin never fights the user:
	// ActiveCulture tracks whatever the editor is actually displaying.
	const FString EngineLanguage = FPolyglotCultureController::GetEngineLanguage();
	if (!EngineLanguage.IsEmpty() && !ActiveCulture.Equals(EngineLanguage))
	{
		ActiveCulture = EngineLanguage;
		SaveState();
		UpdateScannerContext();
		UE_LOG(LogPolyglot, Display, TEXT("Editor culture changed externally, Polyglot now follows '%s'."),
			*EngineLanguage);
	}

	bInternalTextUpdate = true;
	Pipeline->Injector->ApplyPatch(ActiveCulture);
	bInternalTextUpdate = false;

	ForceRescanAllWindows();
}

void UPolyglotSubsystem::ReapplyAfterDictionaryChange()
{
	if (!bEnabled || !Pipeline || ActiveCulture.IsEmpty())
	{
		return;
	}

	// 1. Restore every scanned widget to its source first, otherwise widgets
	//    patched with the *old* translation would keep it forever (the dual-state
	//    check sees no difference and skips them).
	RestoreAllWidgets();

	// 2. Re-inject the keyed overrides into the live table.
	if (Pipeline->Injector)
	{
		bInternalTextUpdate = true;
		Pipeline->Injector->ApplyPatch(ActiveCulture);
		bInternalTextUpdate = false;
	}

	// 3. Drop the scanner's lookup cache and re-scan, which applies the new
	//    source->translation rows to every already-open window.
	if (Pipeline->Scanner)
	{
		Pipeline->Scanner->OnDictionariesReloaded();
	}
}

void UPolyglotSubsystem::UpdateScannerContext()
{
	if (Pipeline && Pipeline->Scanner)
	{
		Pipeline->Scanner->Configure(
			Pipeline->Dictionary.Get(),
			ActiveCulture,
			ScanIntervalSeconds,
			ScanTimeBudgetMs,
			bTranslateStaticTextBlocks,
			bTranslateEditableText);
	}
}

void UPolyglotSubsystem::SaveState() const
{
	PolyglotSettings::FPolyglotState State;
	State.bEnabled = bEnabled;
	State.Culture = ActiveCulture;
	State.ScanIntervalSeconds = ScanIntervalSeconds;
	State.ScanTimeBudgetMs = ScanTimeBudgetMs;
	State.bTranslateStaticTextBlocks = bTranslateStaticTextBlocks;
	State.bTranslateEditableText = bTranslateEditableText;
	PolyglotSettings::SaveState(State);
}

void UPolyglotSubsystem::LoadState()
{
	const PolyglotSettings::FPolyglotState State = PolyglotSettings::LoadState();
	bEnabled = State.bEnabled;
	ActiveCulture = !State.Culture.IsEmpty() ? State.Culture : Polyglot::Compat::Culture_ChineseSimplified;
	ScanIntervalSeconds = State.ScanIntervalSeconds;
	ScanTimeBudgetMs = State.ScanTimeBudgetMs;
	bTranslateStaticTextBlocks = State.bTranslateStaticTextBlocks;
	bTranslateEditableText = State.bTranslateEditableText;
}
