// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

// Content browser adapter.
//
// Verified against UE 5.8 sources (research agent report):
//   - Asset names are FAssetData.AssetName / display names rendered by SAssetView
//     rows (STextBlock). Folder names are FContentBrowserItemData display strings
//     from UContentBrowserDataSubsystem. Both are FName/FString backed.
//   - Rows and tiles are rebuilt when the browser navigates / refreshes, so any
//     display rewrite is re-applied by the periodic scanner - no rename API is ever
//     used; the underlying FName and on-disk path stay untouched (zero-write rule).
//   - Asset-type labels ("Static Mesh", "Data Table", ...) are keyed FText that the
//     culture layer translates; the engine zh-Hans Editor locres already covers the
//     common types.
//
// The important safety note is enforced here: this adapter performs NO engine call.
// Translating item names is a pure display-layer rewrite and is bounded by exact
// dictionary matches (built-in common-folder list or user-picked entries), so
// arbitrary user asset names are never touched.
//
// Runtime probe module: ContentBrowser (+ ContentBrowserData present in all 5.x).

#include "PolyglotAdapters.h"

namespace PolyglotAdapters
{
	FAdapterStatus QueryContentBrowserStatus()
	{
		FAdapterStatus Status;
		Status.DisplayName = TEXT("Content browser");
		Status.Modules.Add(TEXT("ContentBrowser"));
		Status.LayerPolicy = TEXT("culture(type labels) + slate(item names)");
		Status.ScopeDescription =
			TEXT("Asset type labels follow the culture layer. Asset and folder names are ")
			TEXT("translated display-only when they exactly match a dictionary entry; the ")
			TEXT("underlying FName and path are never modified.");
		return Status;
	}
}
