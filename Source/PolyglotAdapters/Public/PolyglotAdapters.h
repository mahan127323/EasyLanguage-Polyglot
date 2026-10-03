// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Public API of the PolyglotAdapters module.
 *
 * The module never depends on the specialised editor modules (Niagara,
 * Material, Blueprint, Content Browser) at compile time: every one of them is
 * probed at runtime. The UI layer (control panel) calls QueryAllAdapterStatuses
 * to tell the user which editor areas can currently be translated and how.
 */
namespace PolyglotAdapters
{
	/**
	 * Runtime status of one specialised editor area.
	 */
	struct POLYGLOTADAPTERS_API FAdapterStatus
	{
		/** Human readable editor area, e.g. "Niagara editor". */
		FString DisplayName;

		/** Modules that provide this editor (probed at runtime). */
		TArray<FString> Modules;

		/** True when every primary module currently exists in this build. */
		bool bModulesExist = false;

		/** True when every primary module is loaded right now. */
		bool bModulesLoaded = false;

		/**
		 * Which Polyglot layers translate this area's on-screen text.
		 * Verified against UE 5.8 sources; see each adapter .cpp for evidence.
		 *   - "culture": keyed FText, engine zh-Hans data drives it automatically.
		 *   - "slate":   raw FString/FName-backed labels rewritten by the scanner.
		 */
		FString LayerPolicy;

		/** One-line user-facing description of what is / is not translated. */
		FString ScopeDescription;
	};

	/** Status of the four built-in specialised areas. */
	POLYGLOTADAPTERS_API TArray<FAdapterStatus> QueryAllAdapterStatuses();

	/** Status of one area. */
	POLYGLOTADAPTERS_API FAdapterStatus QueryNiagaraStatus();
	POLYGLOTADAPTERS_API FAdapterStatus QueryMaterialStatus();
	POLYGLOTADAPTERS_API FAdapterStatus QueryBlueprintStatus();
	POLYGLOTADAPTERS_API FAdapterStatus QueryContentBrowserStatus();
}
