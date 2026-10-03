// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

// Niagara editor adapter.
//
// Verified against UE 5.8 sources (research agent report):
//   - Stack module rows ("Spawn Rate", ...): UNiagaraStackModuleItem::GetDisplayName()
//     -> UNiagaraNodeFunctionCall::GetNodeTitle() -> FText::FromString(FunctionDisplayName).
//   - Particle input rows: UNiagaraStackFunctionInput::GetDisplayName() is backed by
//     FText::FromName(InputParameterHandle.GetName()).
//   - "Add module" palette rows: FNiagaraEditorUtilities::FormatScriptName(AssetName).
//   All of these are raw FName/FString backed labels rendered through plain Slate
//   STextBlock widgets - culture switching does NOT change them, and the engine
//   ships no NiagaraEditor zh-Hans locres. The Polyglot Slate scanner translates
//   them from the dictionary; no engine API hook is required (and none is used).
//
// Runtime probe module: NiagaraEditor (editor code) + Niagara (runtime data).

#include "PolyglotAdapters.h"

namespace PolyglotAdapters
{
	FAdapterStatus QueryNiagaraStatus()
	{
		FAdapterStatus Status;
		Status.DisplayName = TEXT("Niagara editor");
		Status.Modules.Add(TEXT("NiagaraEditor"));
		Status.LayerPolicy = TEXT("slate");
		Status.ScopeDescription =
			TEXT("Particle module names, input rows and the add-module palette are raw names. ")
			TEXT("They are translated display-only by the Slate scanner from the Niagara dictionary. ")
			TEXT("Custom user names are left untouched unless they match a dictionary entry exactly.");
		return Status;
	}
}
