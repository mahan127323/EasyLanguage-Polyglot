// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

/**
 * Small JSON helpers used across Polyglot files.
 *
 * Polyglot deliberately keeps every JSON payload on disk as UTF-8 without BOM
 * (engine / editor friendly) and decodes it explicitly as UTF-8. Reading with
 * the naive FFileHelper::LoadFileToString on a Chinese-locale Windows system
 * would misinterpret the bytes, so the helpers below are the only place that
 * touches dictionaries / settings.
 */
namespace PolyglotFileUtil
{
	/** Read a UTF-8 file into a JSON object. Returns false on failure. */
	POLYGLOTCORE_API bool LoadJsonFromFile(const FString& FilePath, TSharedPtr<FJsonObject>& OutRoot);

	/** Serialize a JSON object to a UTF-8 file (no BOM). Returns false on failure. */
	POLYGLOTCORE_API bool SaveJsonToFile(const FString& FilePath, const TSharedRef<FJsonObject>& Root);

	/** Read raw UTF-8 file bytes and decode to FString. Returns false on failure. */
	POLYGLOTCORE_API bool ReadUtf8FileToString(const FString& FilePath, FString& OutString);

	/** Encode FString to UTF-8 and write to file (no BOM). Returns false on failure. */
	POLYGLOTCORE_API bool WriteUtf8StringToFile(const FString& FilePath, const FString& Content);
}
