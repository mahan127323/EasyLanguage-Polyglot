// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

// Blueprint editor adapter.
//
// Verified against UE 5.8 sources (research agent report):
//   - Engine/BlueprintGraph nodes ("Add", "Branch", ...): UEdGraphNode::GetNodeTitle()
//     returns keyed FText for engine nodes, so the culture layer plus the official
//     zh-Hans translation (or our override patch) handles them.
//   - Function call nodes to user assets: title built from the function FName, raw.
//   - My Blueprint entries, variable / function / macro / event-dispatcher names:
//     FName-backed, edited through SInlineEditableTextBlock.
//   - Graph names (Event Graph / Anim Graph): plain FString in SGraphEditor titles.
//   - Comment bubbles: raw user text.
//   Engine nodes are handled by culture + override injection; raw labels are handled
//   by the Slate scanner using exact dictionary matches, which naturally leaves every
//   user-defined name alone (it never matches a dictionary key).
//
// Runtime probe modules: BlueprintGraph + Kismet (both are editor-only modules).

#include "PolyglotAdapters.h"

namespace PolyglotAdapters
{
	FAdapterStatus QueryBlueprintStatus()
	{
		FAdapterStatus Status;
		Status.DisplayName = TEXT("Blueprint editor");
		Status.Modules.Add(TEXT("BlueprintGraph"));
		Status.Modules.Add(TEXT("Kismet"));
		Status.LayerPolicy = TEXT("culture(engine nodes) + slate(raw labels)");
		Status.ScopeDescription =
			TEXT("Engine blueprint nodes follow the culture layer. Function names, ")
			TEXT("graph names and other raw labels are translated display-only when they ")
			TEXT("exactly match a dictionary entry; user-created names are never rewritten.");
		return Status;
	}
}
