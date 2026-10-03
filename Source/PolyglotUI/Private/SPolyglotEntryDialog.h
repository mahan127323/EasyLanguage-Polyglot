// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "PolyglotTypes.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SMultiLineEditableTextBox;
class STextBlock;

namespace PolyglotUI
{
	/** Broadcast with the finished entry when the user confirms a dialog. */
	DECLARE_DELEGATE_OneParam(FPolyglotEntrySaveDelegate, const FPolyglotEntry&);
	/** Broadcast when the user cancels a dialog. */
	DECLARE_DELEGATE(FPolyglotEntryCancelDelegate);
}

/**
 * A small form that edits one dictionary entry (FPolyglotEntry).
 *
 * Used by the dictionary editor (standalone window) and by the visual picker
 * (embedded in the picker HUD). All fields are plain text boxes; the caller
 * receives the assembled entry through OnSave. Cancelling fires OnCancel.
 */
class SPolyglotEntryDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPolyglotEntryDialog) {}
		/** The entry to start from (empty for "create new"). */
		SLATE_ARGUMENT(FPolyglotEntry, InitialEntry)
		/** Optional explanatory line shown at the top of the form. */
		SLATE_ARGUMENT(FText, NoteText)
		/** When true the Source box cannot be edited (picker results). */
		SLATE_ARGUMENT(bool, bSourceReadOnly)
		/** Culture field disabled (keep the active culture). */
		SLATE_ARGUMENT(bool, bCultureReadOnly)
		/** Called with the assembled entry when the user presses Save. */
		SLATE_EVENT(PolyglotUI::FPolyglotEntrySaveDelegate, OnSave)
		/** Called when the user presses Cancel. */
		SLATE_EVENT(PolyglotUI::FPolyglotEntryCancelDelegate, OnCancel)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Convenience: assemble an entry from the current box content. */
	FPolyglotEntry BuildEntryFromFields() const;

	/** Put keyboard focus into the translation box (safe to call after the widget is in a window). */
	void FocusTranslationBox();

private:
	FText GetValidationMessage() const;
	FReply HandleSaveClicked();
	FReply HandleCancelClicked();

	TSharedPtr<SEditableTextBox> CultureBox;
	TSharedPtr<SMultiLineEditableTextBox> SourceBox;
	TSharedPtr<SMultiLineEditableTextBox> TranslationBox;
	TSharedPtr<SEditableTextBox> NamespaceBox;
	TSharedPtr<SEditableTextBox> KeyBox;
	TSharedPtr<SEditableTextBox> ScopeBox;
	TSharedPtr<SEditableTextBox> PluginBox;
	TSharedPtr<SEditableTextBox> CommentBox;
	TSharedPtr<STextBlock> ValidationText;

	PolyglotUI::FPolyglotEntrySaveDelegate OnSaveDelegate;
	PolyglotUI::FPolyglotEntryCancelDelegate OnCancelDelegate;
	FString DefaultCulture;
};
