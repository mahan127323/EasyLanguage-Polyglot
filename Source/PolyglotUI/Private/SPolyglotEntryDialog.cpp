// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "SPolyglotEntryDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "PolyglotCompat.h"
#include "PolyglotUIInternal.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SPolyglotEntryDialog"

namespace
{
	/** A consistent label style used by every form row. */
	const FTextBlockStyle& LabelStyle()
	{
		return FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText");
	}

	/** A text box whose text can never be empty for the field labels we render. */
	TSharedRef<SWidget> MakeFieldLabel(const FText& InLabel)
	{
		return SNew(STextBlock)
			.TextStyle(&LabelStyle())
			.Text(InLabel);
	}

	/** Single line box with a fixed height. */
	TSharedRef<SEditableTextBox> MakeTextBox()
	{
		return SNew(SEditableTextBox)
			.Style(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
			.Font(FCoreStyle::Get().GetFontStyle("NormalFont"));
	}
}

void SPolyglotEntryDialog::Construct(const FArguments& InArgs)
{
	DefaultCulture = InArgs._InitialEntry.Culture.IsEmpty()
		? Polyglot::Compat::Culture_ChineseSimplified
		: InArgs._InitialEntry.Culture;

	OnSaveDelegate = InArgs._OnSave;
	OnCancelDelegate = InArgs._OnCancel;

	const FTextBlockStyle& NormalTextStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
	const FEditableTextBoxStyle& EditBoxStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	const FSlateFontInfo& NormalFont = FCoreStyle::Get().GetFontStyle("NormalFont");

	const FText InitialSource = FText::FromString(InArgs._InitialEntry.Source);
	const FText InitialTranslation = FText::FromString(InArgs._InitialEntry.Translation);

	SourceBox = SNew(SMultiLineEditableTextBox)
		.Style(&EditBoxStyle)
		.Font(NormalFont)
		.Text(InitialSource)
		.IsReadOnly(InArgs._bSourceReadOnly)
		.AutoWrapText(true);

	TranslationBox = SNew(SMultiLineEditableTextBox)
		.Style(&EditBoxStyle)
		.Font(NormalFont)
		.Text(InitialTranslation)
		.AutoWrapText(true);

	CultureBox = MakeTextBox();
	CultureBox->SetText(FText::FromString(DefaultCulture));
	if (InArgs._bCultureReadOnly)
	{
		CultureBox->SetIsReadOnly(true);
	}

	NamespaceBox = MakeTextBox();
	NamespaceBox->SetText(FText::FromString(InArgs._InitialEntry.Namespace));
	KeyBox = MakeTextBox();
	KeyBox->SetText(FText::FromString(InArgs._InitialEntry.Key));
	ScopeBox = MakeTextBox();
	ScopeBox->SetText(FText::FromString(InArgs._InitialEntry.Scope));
	PluginBox = MakeTextBox();
	PluginBox->SetText(FText::FromString(InArgs._InitialEntry.Plugin));
	CommentBox = MakeTextBox();
	CommentBox->SetText(FText::FromString(InArgs._InitialEntry.Comment));

	ValidationText = SNew(STextBlock)
		.TextStyle(&NormalTextStyle)
		.ColorAndOpacity(FLinearColor(0.85f, 0.35f, 0.35f))
		.AutoWrapText(true);

	TSharedRef<SVerticalBox> Form = SNew(SVerticalBox);

	if (!InArgs._NoteText.IsEmpty())
	{
		Form->AddSlot()
			.Padding(FMargin(0.f, 0.f, 0.f, 8.f))
			.AutoHeight()
			[
				SNew(STextBlock)
					.TextStyle(&NormalTextStyle)
					.Text(InArgs._NoteText)
					.AutoWrapText(true)
			];
	}

	Form->AddSlot()
		.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.FillWidth(0.22f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
				[ MakeFieldLabel(PolyglotUI::UiText(TEXT("Culture"), TEXT("语言"))) ]
			+ SHorizontalBox::Slot()
				.FillWidth(0.78f)
				[ CultureBox.ToSharedRef() ]
		];

	Form->AddSlot()
		.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.FillWidth(0.22f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Top)
				.Padding(FMargin(0.f, 4.f, 8.f, 0.f))
				[ MakeFieldLabel(PolyglotUI::UiText(TEXT("Source (English)"), TEXT("源文本（英文）"))) ]
			+ SHorizontalBox::Slot()
				.FillWidth(0.78f)
				[
					SNew(SBox)
						.MinDesiredHeight(44.f)
						[ SourceBox.ToSharedRef() ]
				]
		];

	Form->AddSlot()
		.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.FillWidth(0.22f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Top)
				.Padding(FMargin(0.f, 4.f, 8.f, 0.f))
				[ MakeFieldLabel(PolyglotUI::UiText(TEXT("Translation"), TEXT("译文"))) ]
			+ SHorizontalBox::Slot()
				.FillWidth(0.78f)
				[
					SNew(SBox)
						.MinDesiredHeight(44.f)
						[ TranslationBox.ToSharedRef() ]
				]
		];

	// ---------------------------------------------------------------------
	// Advanced identity & bookkeeping fields (collapsed by default).
	// ---------------------------------------------------------------------
	TSharedRef<SVerticalBox> AdvancedBox = SNew(SVerticalBox);

	auto AddAdvancedRow = [&AdvancedBox](const FText& Label, const TSharedRef<SEditableTextBox>& Box)
	{
		AdvancedBox->AddSlot()
			.Padding(FMargin(0.f, 2.f, 0.f, 2.f))
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
					.FillWidth(0.22f)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
					[ MakeFieldLabel(Label) ]
				+ SHorizontalBox::Slot()
					.FillWidth(0.78f)
					[ Box ]
			];
	};

	AddAdvancedRow(PolyglotUI::UiText(TEXT("Namespace"), TEXT("命名空间")), NamespaceBox.ToSharedRef());
	AddAdvancedRow(PolyglotUI::UiText(TEXT("Key"), TEXT("键 (Key)")), KeyBox.ToSharedRef());
	AddAdvancedRow(PolyglotUI::UiText(TEXT("Scope"), TEXT("作用域")), ScopeBox.ToSharedRef());
	AddAdvancedRow(PolyglotUI::UiText(TEXT("Plugin"), TEXT("插件")), PluginBox.ToSharedRef());
	AddAdvancedRow(PolyglotUI::UiText(TEXT("Comment"), TEXT("备注")), CommentBox.ToSharedRef());

	TSharedRef<SButton> SaveButton = SNew(SButton)
		.OnClicked(FOnClicked::CreateRaw(this, &SPolyglotEntryDialog::HandleSaveClicked))
		.Text(PolyglotUI::UiText(TEXT("Save"), TEXT("保存")));
	TSharedRef<SButton> CancelButton = SNew(SButton)
		.OnClicked(FOnClicked::CreateRaw(this, &SPolyglotEntryDialog::HandleCancelClicked))
		.Text(PolyglotUI::UiText(TEXT("Cancel"), TEXT("取消")));

	Form->AddSlot()
		.Padding(FMargin(0.f, 4.f, 0.f, 0.f))
		.AutoHeight()
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
				[
					SNew(SBorder)
						.Padding(FMargin(10.f, 6.f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
										.TextStyle(&NormalTextStyle)
										.Text(PolyglotUI::UiText(TEXT("Advanced (localization identity & notes)"), TEXT("高级（本地化标识与备注）")))
										.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
								]
							+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(FMargin(0.f, 2.f, 0.f, 0.f))
								[ AdvancedBox ]
						]
				]
		];

	Form->AddSlot()
		.Padding(FMargin(0.f, 6.f, 0.f, 0.f))
		.AutoHeight()
		[ ValidationText.ToSharedRef() ];

	Form->AddSlot()
		.Padding(FMargin(0.f, 10.f, 0.f, 0.f))
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
				[ SaveButton ]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[ CancelButton ]
			+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[ SNew(SSpacer) ]
		];

	ChildSlot
		[
			SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.Padding(12.f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
						[ Form ]
				]
		];
}

FPolyglotEntry SPolyglotEntryDialog::BuildEntryFromFields() const
{
	FPolyglotEntry Entry;
	Entry.Culture = CultureBox->GetText().ToString().TrimStartAndEnd();
	Entry.Source = SourceBox->GetText().ToString();
	Entry.Translation = TranslationBox->GetText().ToString();
	Entry.Namespace = NamespaceBox->GetText().ToString().TrimStartAndEnd();
	Entry.Key = KeyBox->GetText().ToString().TrimStartAndEnd();
	Entry.Scope = ScopeBox->GetText().ToString().TrimStartAndEnd();
	Entry.Plugin = PluginBox->GetText().ToString().TrimStartAndEnd();
	Entry.Comment = CommentBox->GetText().ToString();

	// The trimmed Source is used for matching, but we preserve the raw box text
	// for exact matches with the editor display string.
	if (Entry.Culture.IsEmpty())
	{
		Entry.Culture = DefaultCulture.IsEmpty() ? Polyglot::Compat::Culture_ChineseSimplified : DefaultCulture;
	}
	return Entry;
}

FText SPolyglotEntryDialog::GetValidationMessage() const
{
	if (SourceBox->GetText().IsEmpty() && KeyBox->GetText().IsEmpty())
	{
		return PolyglotUI::UiText(
			TEXT("A source string (or at least a localization key) is required."),
			TEXT("需要填写源文本（或至少一个本地化键 Key）。"));
	}
	if (TranslationBox->GetText().IsEmpty())
	{
		return PolyglotUI::UiText(
			TEXT("The translation is empty. Leave it empty only when you want to REMOVE an existing translation."),
			TEXT("译文为空。只有在想“删除某条已有翻译”时才应留空。"));
	}
	return FText::GetEmpty();
}

FReply SPolyglotEntryDialog::HandleSaveClicked()
{
	const FText Error = GetValidationMessage();
	ValidationText->SetText(Error);
	if (!Error.IsEmpty())
	{
		return FReply::Handled();
	}
	OnSaveDelegate.ExecuteIfBound(BuildEntryFromFields());
	return FReply::Handled();
}

FReply SPolyglotEntryDialog::HandleCancelClicked()
{
	OnCancelDelegate.ExecuteIfBound();
	return FReply::Handled();
}

void SPolyglotEntryDialog::FocusTranslationBox()
{
	if (TranslationBox.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(TranslationBox);
	}
}

#undef LOCTEXT_NAMESPACE
