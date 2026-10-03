// Copyright 2026 BaiMaoMaLou. All Rights Reserved.
//
// Text parser shared by the "bulk add" window and the "Import TXT" button.
//
// Supported format (one pair per entry):
//     English/中文
// Entries are separated by either a new line or an English comma, both work:
//     Blueprint/蓝图,simulation/模拟
//     Camera/摄像机
// The pair separator is the LAST '/' of the item, so sources that themselves
// contain a slash (e.g. "A/B/C" is read as source "A/B", translation "C") are
// tolerated. If the translation itself contains a '/' the item cannot be parsed
// safely - the UI documents that limitation and suggests CSV / single entry.

#pragma once

#include "CoreMinimal.h"
#include "PolyglotTypes.h"

namespace PolyglotUI
{
	/** Result of parsing a bulk text. */
	struct FBulkParseResult
	{
		/** Number of comma/newline separated items found (non empty after trim). */
		int32 ItemCount = 0;

		/** Items successfully turned into entries (source + translation). */
		int32 ValidCount = 0;

		/** Items skipped: empty or missing a usable translation after the '/'. */
		int32 SkippedCount = 0;
	};

	/**
	 * Parse bulk text into dictionary entries. Culture is copied onto every
	 * entry untouched when non-empty; empty input entries get no culture field
	 * (callers usually pre-fill a default such as "zh-Hans").
	 */
	FBulkParseResult ParseBulkEntryText(const FString& Text, const FString& DefaultCulture, TArray<FPolyglotEntry>& OutEntries);
}
