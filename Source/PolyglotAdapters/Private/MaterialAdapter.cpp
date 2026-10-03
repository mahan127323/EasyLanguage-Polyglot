// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

// Material editor adapter.
//
// Verified against UE 5.8 sources (research agent report):
//   - Canvas node titles come from UMaterialGraphNode::GetNodeTitle() which builds
//     FText from the raw FString captions of UMaterialExpression::GetCaption()
//     (e.g. "Add", "Texture Sample") - no localisation key. The title is drawn by
//     the generic SGraphNode via an STextBlock, so the Polyglot scanner can rewrite
//     it from the Material dictionary.
//   - The right-click / searchable node palette rows come from
//     FMaterialEditorUtilities::GetMaterialExpressionActions() -> FText::FromString
//     of the class name, again raw and rendered as STextBlock rows.
//   - A few fixed strings (Comment menu, tooltips) are LOCTEXT and follow the
//     culture layer.
// No engine API hook is used; parameter expressions keep their live values because
// the scanner only matches exact dictionary keys.
//
// Runtime probe module: MaterialEditor.

#include "PolyglotAdapters.h"

namespace PolyglotAdapters
{
	FAdapterStatus QueryMaterialStatus()
	{
		FAdapterStatus Status;
		Status.DisplayName = TEXT("Material editor");
		Status.Modules.Add(TEXT("MaterialEditor"));
		Status.LayerPolicy = TEXT("slate + culture(fixed strings)");
		Status.ScopeDescription =
			TEXT("Node titles and palette rows are raw expression captions, translated ")
			TEXT("display-only by the Slate scanner from the Material dictionary. ")
			TEXT("LOCTEXT fixed strings follow the engine culture layer automatically.");
		return Status;
	}
}
