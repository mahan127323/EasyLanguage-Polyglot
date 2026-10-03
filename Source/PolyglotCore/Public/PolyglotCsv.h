// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PolyglotTypes.h"

/**
 * CSV helpers used by the import/export UI and by the export commandlet.
 *
 * Header:
 *   Namespace,Key,Source,Translation,Culture,Plugin
 *
 * Namespace / Key are optional (leave empty for source-only entries), Culture
 * defaults to "zh-Hans" and Plugin is informational.
 */
namespace PolyglotCsv
{
	/** Parse CSV text into dictionary entries. Returns false on structural failure. */
	POLYGLOTCORE_API bool ParseEntries(const FString& CsvText, const FString& DefaultCulture, TArray<FPolyglotEntry>& OutEntries);

	/** Serialize entries to CSV text (with header). */
	POLYGLOTCORE_API FString WriteEntries(const TArray<FPolyglotEntry>& InEntries);

	/** Simple CSV cell escaping. */
	POLYGLOTCORE_API FString EscapeCell(const FString& Cell);
	POLYGLOTCORE_API FString UnescapeCell(const FString& Cell);
}
