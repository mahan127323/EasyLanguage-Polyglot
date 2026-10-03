// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "SPolyglotBatchAddDialog.h"

#include "PolyglotBulkParser.h"
#include "PolyglotUIInternal.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SPolyglotBatchAddDialog"

void SPolyglotBatchAddDialog::Construct(const FArguments& InArgs)
{
	OnImportDelegate = InArgs._OnImport;
	OnCancelDelegate = InArgs._OnCancel;

	const FTextBlockStyle& NormalStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
	const FTextBlockStyle& SmallStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText");

	// Short bilingual helper used below to keep every widget one-liner readable.
	const auto T = [](const TCHAR* En, const TCHAR* Zh) { return PolyglotUI::UiText(En, Zh); };

	CultureBox = SNew(SEditableTextBox)
		.Text(FText::FromString(InArgs._DefaultCulture))
		.OnTextChanged_Lambda([this](const FText&) { Reparse(); });

	InputBox = SNew(SMultiLineEditableTextBox)
		.HintText(T(
			TEXT("Blueprint/蓝图,simulation/模拟\nAsset/资产\n\n(comma or new line separated pairs, format: English/中文)"),
			TEXT("Blueprint/蓝图,simulation/模拟\nAsset/资产\n\n（条目用逗号或换行分隔，格式：英文/中文）")))
		.OnTextChanged_Lambda([this](const FText&) { Reparse(); });

	StatsText = SNew(STextBlock)
		.TextStyle(&SmallStyle)
		.AutoWrapText(true);

	ImportButtonText = SNew(STextBlock)
		.TextStyle(&SmallStyle);
	ImportButton = SNew(SButton)
		.OnClicked(FOnClicked::CreateRaw(this, &SPolyglotBatchAddDialog::HandleImportClicked))
		[
			ImportButtonText.ToSharedRef()
		];

	ChildSlot
		[
			SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.Padding(12.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.f, 0.f, 0.f, 10.f))
						[
							SNew(STextBlock)
								.TextStyle(&NormalStyle)
								.AutoWrapText(true)
								.Text(T(
									TEXT("Paste one or more English/中文 pairs below. Entries are added to the USER dictionary in one write."),
									TEXT("在下方粘贴一个或多个“英文/中文”词条对，将一次性写入用户词典。")))
						]
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
								[
									SNew(STextBlock)
										.TextStyle(&SmallStyle)
										.Text(T(TEXT("Target culture"), TEXT("目标语言")))
								]
							+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
								[
									SNew(SBox)
										.WidthOverride(120.f)
										[ CultureBox.ToSharedRef() ]
								]
							+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.TextStyle(&SmallStyle)
										.Text(T(TEXT("(default zh-Hans)"), TEXT("（默认为 zh-Hans）")))
								]
						]
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
						[
							SNew(STextBlock)
								.TextStyle(&SmallStyle)
								.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
								.AutoWrapText(true)
								.Text(T(
									TEXT("Format limit: items are separated by commas / new lines and each is split at its LAST '/'. Text containing ',' or a '/' inside the translation cannot be represented - use a single entry or JSON/CSV import for those."),
									TEXT("格式限制：条目以逗号 / 换行分隔，并在最后一个“/”处拆分“英文/中文”。译文本身含“,”或“/”的内容无法表示，请改用单条添加或 JSON/CSV 导入。")))
						]
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
						[
							SNew(SBox)
								.MinDesiredHeight(220.f)
								[ InputBox.ToSharedRef() ]
						]
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.f, 4.f, 0.f, 0.f))
						[ StatsText.ToSharedRef() ]
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
								[ ImportButton.ToSharedRef() ]
							+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(SButton)
										.OnClicked(FOnClicked::CreateRaw(this, &SPolyglotBatchAddDialog::HandleCancelClicked))
										[
											SNew(STextBlock)
												.TextStyle(&SmallStyle)
												.Text(T(TEXT("Cancel"), TEXT("取消")))
										]
								]
						]
				]
		];

	Reparse();
}

void SPolyglotBatchAddDialog::Reparse()
{
	ParsedEntries.Reset();
	const FString Culture = CultureBox->GetText().ToString().TrimStartAndEnd();
	const PolyglotUI::FBulkParseResult Result = PolyglotUI::ParseBulkEntryText(InputBox->GetText().ToString(), Culture, ParsedEntries);
	LastParseResult = Result;
	ValidItemCount = Result.ValidCount;
	SkippedItemCount = Result.SkippedCount;

	StatsText->SetText(FText::Format(
		PolyglotUI::UiText(
			TEXT("{0} pair(s) ready to add to \"{1}\"; {2} item(s) skipped (blank or missing a translation)."),
			TEXT("可添加 {0} 组到“{1}”；{2} 项被跳过（空项或缺少译文）。")),
		FText::AsNumber(ValidItemCount),
		FText::FromString(Culture.IsEmpty() ? TEXT("-") : Culture),
		FText::AsNumber(SkippedItemCount)));

	ImportButtonText->SetText(ValidItemCount > 0
		? FText::Format(
			PolyglotUI::UiText(TEXT("Add {0} pairs"), TEXT("添加 {0} 组")),
			FText::AsNumber(ValidItemCount))
		: PolyglotUI::UiText(TEXT("Add"), TEXT("添加")));
	ImportButton->SetEnabled(ValidItemCount > 0);
}

FReply SPolyglotBatchAddDialog::HandleImportClicked()
{
	if (ValidItemCount > 0)
	{
		OnImportDelegate.ExecuteIfBound(ParsedEntries, LastParseResult);
	}
	return FReply::Handled();
}

FReply SPolyglotBatchAddDialog::HandleCancelClicked()
{
	OnCancelDelegate.ExecuteIfBound();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
