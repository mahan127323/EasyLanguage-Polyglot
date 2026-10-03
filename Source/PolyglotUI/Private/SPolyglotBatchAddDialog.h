// Copyright 2026 BaiMaoMaLou. All Rights Reserved.
//
// "Bulk add" dialog opened by the dictionary editor's Add button.
// Paste one or more "English/中文" pairs (separated by commas or new lines)
// and add them all to the user dictionary in a single write.

#pragma once

#include "CoreMinimal.h"
#include "PolyglotBulkParser.h"
#include "PolyglotTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

// Forward declarations used by the member widgets (TSharedPtr tolerates
// incomplete types; the .cpp includes the full Slate headers).
class SButton;
class SEditableTextBox;
class SMultiLineEditableTextBox;
class STextBlock;

namespace PolyglotUI
{
	/** Called with the fully parsed entries when the user confirms the batch. */
	DECLARE_DELEGATE_TwoParams(FPolyglotBatchImportDelegate, const TArray<FPolyglotEntry>& /*Entries*/, const FBulkParseResult& /*Stats*/);

	/** Called when the user cancels the dialog. */
	DECLARE_DELEGATE(FPolyglotBatchCancelDelegate);
}

class SPolyglotBatchAddDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPolyglotBatchAddDialog)
		: _DefaultCulture(TEXT("zh-Hans"))
		{}
		/** Culture stamped onto every parsed entry. */
		SLATE_ARGUMENT(FString, DefaultCulture)
		SLATE_EVENT(PolyglotUI::FPolyglotBatchImportDelegate, OnImport)
		SLATE_EVENT(PolyglotUI::FPolyglotBatchCancelDelegate, OnCancel)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** Re-run the parser over the current input and refresh the stats row. */
	void Reparse();

	FReply HandleImportClicked();
	FReply HandleCancelClicked();

	TSharedPtr<SEditableTextBox> CultureBox;
	TSharedPtr<SMultiLineEditableTextBox> InputBox;
	TSharedPtr<STextBlock> StatsText;
	TSharedPtr<STextBlock> ImportButtonText;
	TSharedPtr<SButton> ImportButton;

	TArray<FPolyglotEntry> ParsedEntries;
	PolyglotUI::FBulkParseResult LastParseResult;
	int32 ValidItemCount = 0;
	int32 SkippedItemCount = 0;

	PolyglotUI::FPolyglotBatchImportDelegate OnImportDelegate;
	PolyglotUI::FPolyglotBatchCancelDelegate OnCancelDelegate;
};
