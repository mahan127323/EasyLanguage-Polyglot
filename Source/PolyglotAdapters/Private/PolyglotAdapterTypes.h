// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace PolyglotAdapters
{
	/**
	 * Runtime status of one specialised editor area.
	 *
	 * Adapters never depend on the specialised editor modules at compile time:
	 * the UI layer uses this status to tell the user which areas can currently
	 * be translated and how (culture layer vs Slate scanner layer).
	 */
	struct FAdapterStatus
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
	TArray<FAdapterStatus> QueryAllAdapterStatuses();

	/** Status of one area. */
	FAdapterStatus QueryNiagaraStatus();
	FAdapterStatus QueryMaterialStatus();
	FAdapterStatus QueryBlueprintStatus();
	FAdapterStatus QueryContentBrowserStatus();
}
