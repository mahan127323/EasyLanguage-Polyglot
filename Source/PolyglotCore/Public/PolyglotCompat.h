// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * PolyglotCompat.h
 *
 * The single place that concentrates every engine-version difference so the rest
 * of the codebase compiles unchanged across UE 5.7 / 5.8 (and newer 5.x releases).
 * When upgrading to a new engine only this header (and the dictionaries) need love.
 */

// ---------------------------------------------------------------------------
// Version helpers
// ---------------------------------------------------------------------------

namespace Polyglot::Compat
{
	/** True when built against UE 5.8 or newer. */
	constexpr bool IsEngine_5_8_Plus()
	{
		return ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8;
	}

	/** True when built against UE 5.7 or newer. */
	constexpr bool IsEngine_5_7_Plus()
	{
		return ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7;
	}

	/** The canonical language culture codes used by the plugin. */
	inline constexpr const TCHAR* Culture_ChineseSimplified = TEXT("zh-Hans");
	inline constexpr const TCHAR* Culture_English = TEXT("en");

	// -------------------------------------------------------------------------
	// Runtime module probing
	//
	// Editor features (Niagara, Material editor, Blueprint graph, Content browser,
	// ...) must NEVER become compile-time dependencies. Instead the plugin asks
	// at runtime whether the module is available and only then hooks the extra
	// behaviour. This keeps the plugin safe on any 5.x combination.
	// -------------------------------------------------------------------------

	/** Returns true if the given module name is currently loaded. */
	POLYGLOTCORE_API bool IsModuleLoaded(const FString& ModuleName);

	/** Returns true if the module can be loaded right now (attempts to load it). */
	POLYGLOTCORE_API bool TryLoadModule(const FString& ModuleName);

	/** The editor module names probed by the adapters layer. */
	namespace Modules
	{
		inline constexpr const TCHAR* NiagaraEditor = TEXT("NiagaraEditor");
		inline constexpr const TCHAR* MaterialEditor = TEXT("MaterialEditor");
		inline constexpr const TCHAR* BlueprintGraph = TEXT("BlueprintGraph");
		inline constexpr const TCHAR* Kismet = TEXT("Kismet");
		inline constexpr const TCHAR* ContentBrowser = TEXT("ContentBrowser");
		inline constexpr const TCHAR* ContentBrowserData = TEXT("ContentBrowserData");
		inline constexpr const TCHAR* ContentBrowserAssetDataSource = TEXT("ContentBrowserAssetDataSource");
		inline constexpr const TCHAR* LocalizationDashboard = TEXT("LocalizationDashboard");
	}

	// -------------------------------------------------------------------------
	// Slate widget type names
	//
	// Widgets are recognised by their SWidget::GetType() FName (a stable string
	// carrying the C++ class name) and never by compile-time type. The cast is
	// performed in FPolyglotWidgetPatcher against exactly these names. Keeping
	// them here means the patcher, the scanner and the picker all agree.
	// -------------------------------------------------------------------------
	namespace WidgetType
	{
		inline const FName& STextBlock() { static const FName Name(TEXT("STextBlock")); return Name; }
		inline const FName& SRichTextBlock() { static const FName Name(TEXT("SRichTextBlock")); return Name; }
		inline const FName& SEditableText() { static const FName Name(TEXT("SEditableText")); return Name; }
		inline const FName& SEditableTextBox() { static const FName Name(TEXT("SEditableTextBox")); return Name; }
		inline const FName& SMultiLineEditableText() { static const FName Name(TEXT("SMultiLineEditableText")); return Name; }
		inline const FName& SMultiLineEditableTextBox() { static const FName Name(TEXT("SMultiLineEditableTextBox")); return Name; }
		inline const FName& SInlineEditableTextBlock() { static const FName Name(TEXT("SInlineEditableTextBlock")); return Name; }
		inline const FName& SSearchBox() { static const FName Name(TEXT("SSearchBox")); return Name; }
	}
}
