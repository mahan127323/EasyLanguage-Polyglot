// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "PolyglotCultureController.h"

#include "Async/Async.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationManager.h"

#include "PolyglotLog.h"

namespace
{
	/** Run a function on the game thread on the next tick (safe from any thread). */
	void RunOnGameThreadLater(TFunction<void()> InFunction)
	{
		if (!InFunction)
		{
			return;
		}
		Async(EAsyncExecution::TaskGraphMainThread,
			[InFunction = MoveTemp(InFunction)]()
			{
				InFunction();
			});
	}
}

void FPolyglotCultureController::Startup()
{
	if (!CultureChangedHandle.IsValid())
	{
		CultureChangedHandle = FInternationalization::Get().OnCultureChanged().AddRaw(
			this, &FPolyglotCultureController::HandleCultureChanged);
	}
}

void FPolyglotCultureController::Shutdown()
{
	if (CultureChangedHandle.IsValid())
	{
		FInternationalization::Get().OnCultureChanged().Remove(CultureChangedHandle);
		CultureChangedHandle.Reset();
	}
}

FString FPolyglotCultureController::GetEngineLanguage()
{
	const FCulturePtr Language = FInternationalization::Get().GetCurrentLanguage();
	return Language.IsValid() ? Language->GetName() : FString();
}

FString FPolyglotCultureController::Canonicalize(const FString& CultureName)
{
	FString Trimmed = CultureName.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return Trimmed;
	}

	const FString Canonical = FCulture::GetCanonicalName(Trimmed);
	return Canonical.IsEmpty() ? Trimmed : Canonical;
}

bool FPolyglotCultureController::IsCultureAvailable(const FString& CultureName)
{
	const FString Canonical = Canonicalize(CultureName);
	if (Canonical.IsEmpty())
	{
		return false;
	}
	return FInternationalization::Get().GetCulture(Canonical).IsValid();
}

void FPolyglotCultureController::SwitchCulture(const FString& CultureName, TFunction<void()>&& OnCompletion)
{
	const FString Target = Canonicalize(CultureName);
	if (Target.IsEmpty())
	{
		UE_LOG(LogPolyglot, Warning, TEXT("Culture switch requested with an empty culture name."));
		return;
	}

	const FString Current = GetEngineLanguage();
	if (Current.Equals(Target))
	{
		// Already on the target culture: nothing to reload, but the caller still
		// expects the completion on the next game tick (uniform sequence).
		RunOnGameThreadLater(MoveTemp(OnCompletion));
		return;
	}

	// Let the caller restore Slate-patched widgets BEFORE the engine reloads the
	// text resources. Once the live table is re-populated those widgets would
	// otherwise show the old translation next to freshly loaded text.
	PreSwitch.Broadcast();

	bInternalSwitch = true;
	const bool bChanged = FInternationalization::Get().SetCurrentLanguageAndLocale(Target);
	bInternalSwitch = false;

	if (!bChanged)
	{
		// The engine rejected the switch (should not happen for a culture that
		// passed IsCultureAvailable, but guard anyway). Report and run completion
		// so callers do not wait forever.
		UE_LOG(LogPolyglot, Warning, TEXT("Culture switch to '%s' was rejected by the engine."), *Target);
		RunOnGameThreadLater(MoveTemp(OnCompletion));
		return;
	}

	if (OnCompletion)
	{
		// SetCurrentLanguageAndLocale already queued (or synchronously executed)
		// the engine's own refresh. Chaining our own refresh guarantees that
		// every potentially async resource is merged before we notify.
		TFunction<void()> CompletionCopy = MoveTemp(OnCompletion);
		FTextLocalizationManager::Get().RefreshResources(
			TUniqueFunction<void()>([CompletionCopy]()
			{
				RunOnGameThreadLater(CompletionCopy);
			}));
	}

	UE_LOG(LogPolyglot, Display, TEXT("Editor language switched '%s' -> '%s'."), *Current, *Target);
}

void FPolyglotCultureController::HandleCultureChanged()
{
	if (bInternalSwitch)
	{
		return;
	}

	UE_LOG(LogPolyglot, Display, TEXT("Editor culture changed externally (now '%s')."),
		*GetEngineLanguage());
	ExternalCultureChanged.Broadcast();
}
