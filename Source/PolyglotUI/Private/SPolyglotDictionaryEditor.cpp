// Copyright 2026 BaiMaoMaLou. All Rights Reserved.
//
// Dictionary editor window.
//
// Shows the merged dictionary (built-in + user). Rows can be selected and:
//   - "Add" opens the bulk-add window (paste English/中文 pairs at once),
//   - "Edit" opens the shared entry dialog in a small window,
//   - "Delete" removes a user entry (built-ins are simply overridden by adding),
//   - JSON / CSV / TXT import merge an external file into the user dictionary,
//   - JSON / CSV export write the currently visible rows to disk.
//
// The window is bilingual (English / Simplified Chinese): all user facing
// strings follow the engine culture and the editor refreshes itself when the
// culture changes (subscribed to UPolyglotSubsystem::OnCultureApplied). Row
// content itself is dictionary data and is never translated.

#include "PolyglotUIInternal.h"

#include "DesktopPlatformModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "Misc/FileHelper.h"
#include "PolyglotBulkParser.h"
#include "PolyglotCompat.h"
#include "PolyglotCsv.h"
#include "PolyglotDictionary.h"
#include "PolyglotFileUtil.h"
#include "PolyglotSettings.h"
#include "PolyglotSubsystem.h"
#include "SPolyglotBatchAddDialog.h"
#include "SPolyglotEntryDialog.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SPolyglotDictionaryEditor"

namespace
{
	constexpr int32 MaxVisibleRows = 800;

	FString Shorten(const FString& InText, int32 MaxChars = 110)
	{
		FString Clean = InText;
		Clean.ReplaceInline(TEXT("\r"), TEXT(" "));
		Clean.ReplaceInline(TEXT("\n"), TEXT(" "));
		if (Clean.Len() > MaxChars)
		{
			Clean = Clean.Left(MaxChars) + TEXT("\x2026"); // horizontal ellipsis
		}
		return Clean;
	}

	FText AsSourceText(const FPolyglotEntry& Entry)
	{
		return FText::FromString(Shorten(Entry.Source));
	}

	FText AsTranslationText(const FPolyglotEntry& Entry)
	{
		return FText::FromString(Shorten(Entry.Translation));
	}

	FText AsMetaText(const FPolyglotEntry& Entry)
	{
		FString Meta;
		if (!Entry.Culture.IsEmpty())
		{
			Meta = Entry.Culture;
		}
		if (!Entry.Key.IsEmpty())
		{
			if (!Meta.IsEmpty())
			{
				Meta += TEXT(" / ");
			}
			Meta += TEXT("key:") + Entry.Key;
		}
		if (!Entry.Plugin.IsEmpty())
		{
			if (!Meta.IsEmpty())
			{
				Meta += TEXT(" / ");
			}
			Meta += Entry.Plugin;
		}
		return FText::FromString(Shorten(Meta, 80));
	}

	/** One toolbar button plus the English/Chinese strings used to re-label it. */
	struct FToolButtonRecord
	{
		FToolButtonRecord(
			const TSharedRef<SButton>& InWidget,
			const TSharedPtr<STextBlock>& InLabelText,
			const TCHAR* InLabelEn,
			const TCHAR* InLabelZh,
			const TCHAR* InTipEn,
			const TCHAR* InTipZh)
			: Widget(InWidget)
			, LabelText(InLabelText)
			, LabelEn(InLabelEn)
			, LabelZh(InLabelZh)
			, TipEn(InTipEn)
			, TipZh(InTipZh)
		{
		}

		TSharedRef<SButton> Widget;
		TSharedPtr<STextBlock> LabelText;
		const TCHAR* LabelEn = nullptr;
		const TCHAR* LabelZh = nullptr;
		const TCHAR* TipEn = nullptr;
		const TCHAR* TipZh = nullptr;
	};

	// ---------------------------------------------------------------------
	// JSON import / export helpers (schema matches PolyglotDictionary).
	// ---------------------------------------------------------------------
	FPolyglotEntry JsonObjectToEntry(const TSharedPtr<FJsonObject>& Obj)
	{
		FPolyglotEntry Entry;
		if (!Obj.IsValid())
		{
			return Entry;
		}
		Obj->TryGetStringField(TEXT("ns"), Entry.Namespace);
		Obj->TryGetStringField(TEXT("key"), Entry.Key);
		Obj->TryGetStringField(TEXT("source"), Entry.Source);
		Obj->TryGetStringField(TEXT("culture"), Entry.Culture);
		Obj->TryGetStringField(TEXT("plugin"), Entry.Plugin);
		Obj->TryGetStringField(TEXT("scope"), Entry.Scope);
		Obj->TryGetStringField(TEXT("comment"), Entry.Comment);
		Obj->TryGetStringField(TEXT("translation"), Entry.Translation);
		return Entry;
	}

	TSharedRef<FJsonObject> EntryToJsonObject(const FPolyglotEntry& Entry)
	{
		const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("ns"), Entry.Namespace);
		Obj->SetStringField(TEXT("key"), Entry.Key);
		Obj->SetStringField(TEXT("source"), Entry.Source);
		Obj->SetStringField(TEXT("culture"), Entry.Culture);
		Obj->SetStringField(TEXT("plugin"), Entry.Plugin);
		Obj->SetStringField(TEXT("scope"), Entry.Scope);
		Obj->SetStringField(TEXT("comment"), Entry.Comment);
		Obj->SetStringField(TEXT("translation"), Entry.Translation);
		return Obj;
	}

	bool WriteEntriesJsonToFile(const FString& FilePath, const TArray<FPolyglotEntry>& Entries)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("culture"), Polyglot::Compat::Culture_ChineseSimplified);
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FPolyglotEntry& Entry : Entries)
		{
			Values.Add(MakeShared<FJsonValueObject>(EntryToJsonObject(Entry)));
		}
		Root->SetArrayField(TEXT("entries"), Values);
		return PolyglotFileUtil::SaveJsonToFile(FilePath, Root);
	}

	int32 ReadEntriesJsonFromFile(const FString& FilePath, TArray<FPolyglotEntry>& OutEntries)
	{
		TSharedPtr<FJsonObject> Root;
		if (!PolyglotFileUtil::LoadJsonFromFile(FilePath, Root))
		{
			return 0;
		}

		FString DefaultCulture;
		if (Root.IsValid())
		{
			Root->TryGetStringField(TEXT("culture"), DefaultCulture);
		}

		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("entries"), Values))
		{
			return 0;
		}

		int32 Loaded = 0;
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(Obj) || !(*Obj).IsValid())
			{
				continue;
			}
			FPolyglotEntry Entry = JsonObjectToEntry(*Obj);
			if (Entry.Culture.IsEmpty())
			{
				Entry.Culture = DefaultCulture;
			}
			if (Entry.Culture.IsEmpty())
			{
				Entry.Culture = Polyglot::Compat::Culture_ChineseSimplified;
			}
			if (Entry.Translation.IsEmpty())
			{
				continue;
			}
			OutEntries.Add(Entry);
			++Loaded;
		}
		return Loaded;
	}

	bool ReadEntriesCsvFromFile(const FString& FilePath, TArray<FPolyglotEntry>& OutEntries)
	{
		FString Content;
		if (!PolyglotFileUtil::ReadUtf8FileToString(FilePath, Content))
		{
			return false;
		}
		return PolyglotCsv::ParseEntries(Content, Polyglot::Compat::Culture_ChineseSimplified, OutEntries);
	}
}

class SPolyglotDictionaryEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPolyglotDictionaryEditor) {}
	SLATE_END_ARGS()

	~SPolyglotDictionaryEditor()
	{
		if (UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem())
		{
			Subsystem->OnCultureApplied().RemoveAll(this);
		}
	}

	void Construct(const FArguments& InArgs)
	{
		const FTextBlockStyle& NormalStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
		const FTextBlockStyle& SmallStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText");
		const FEditableTextBoxStyle& EditBoxStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");

		SearchBox = SNew(SEditableTextBox)
			.Style(&EditBoxStyle)
			.OnTextChanged_Lambda([this](const FText&) { RefreshRows(); });

		UserOnlyLabelText = SNew(STextBlock).TextStyle(&SmallStyle);
		UserOnlyCheck = SNew(SCheckBox)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState) { RefreshRows(); })
			[ UserOnlyLabelText.ToSharedRef() ];

		MessageText = SNew(STextBlock)
			.TextStyle(&SmallStyle)
			.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.25f))
			.AutoWrapText(true);

		SummaryText = SNew(STextBlock)
			.TextStyle(&SmallStyle)
			.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f));

		RowsBox = SNew(SScrollBox);

		TitleText = SNew(STextBlock)
			.TextStyle(&NormalStyle);
		LanguageLabelText = SNew(STextBlock).TextStyle(&SmallStyle);
		CultureRow = SNew(SHorizontalBox);

		ChildSlot
			[
				SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
					.Padding(10.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(FMargin(0.f, 0.f, 0.f, 2.f))
							[ TitleText.ToSharedRef() ]
						+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(FMargin(0.f, 2.f, 0.f, 6.f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
									[ LanguageLabelText.ToSharedRef() ]
								+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[ CultureRow.ToSharedRef() ]
							]
						+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(FMargin(0.f, 0.f, 0.f, 4.f))
							[ MessageText.ToSharedRef() ]
						+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(FMargin(0.f, 2.f, 0.f, 6.f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
									.FillWidth(1.f)
									.VAlign(VAlign_Center)
									.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
									[ SearchBox.ToSharedRef() ]
								+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[ UserOnlyCheck.ToSharedRef() ]
							]
						+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
							[ BuildToolbar() ]
						+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
							[ SummaryText.ToSharedRef() ]
						+ SVerticalBox::Slot()
							.FillHeight(1.f)
							[
								SNew(SBorder)
									.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
									.Padding(4.f)
									[ RowsBox.ToSharedRef() ]
							]
					]
			];

		RefreshLocalizedUi();
		if (UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem())
		{
			Subsystem->OnCultureApplied().AddRaw(this, &SPolyglotDictionaryEditor::HandleCultureApplied);
		}
	}

private:
	// -----------------------------------------------------------------------
	// Toolbar
	// -----------------------------------------------------------------------

	FToolButtonRecord MakeToolButton(
		const TCHAR* LabelEn,
		const TCHAR* LabelZh,
		const TCHAR* TipEn,
		const TCHAR* TipZh,
		FReply(SPolyglotDictionaryEditor::*Handler)())
	{
		const TSharedPtr<STextBlock> LabelText = SNew(STextBlock)
			.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"));
		return FToolButtonRecord(
			SNew(SButton)
				.OnClicked(FOnClicked::CreateRaw(this, Handler))
				[ LabelText.ToSharedRef() ],
			LabelText,
			LabelEn, LabelZh, TipEn, TipZh);
	}

	TSharedRef<SWidget> BuildToolbar()
	{
		auto AddBtn = [this](const TCHAR* LabelEn, const TCHAR* LabelZh, const TCHAR* TipEn, const TCHAR* TipZh, FReply(SPolyglotDictionaryEditor::*Handler)())
		{
			FToolButtonRecord Record = MakeToolButton(LabelEn, LabelZh, TipEn, TipZh, Handler);
			TSharedRef<SButton> Widget = Record.Widget;
			ToolButtons.Add(MoveTemp(Record));
			return Widget;
		};

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))
				[ AddBtn(
					TEXT("Add"), TEXT("添加"),
					TEXT("Add entries by pasting one or more English/中文 pairs (comma or new line separated)."),
					TEXT("批量添加：直接粘贴多条“英文/中文”词条对（逗号或换行分隔）。"),
					&SPolyglotDictionaryEditor::HandleAddClicked) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))
				[ AddBtn(
					TEXT("Edit..."), TEXT("编辑…"),
					TEXT("Edit the selected entry."),
					TEXT("编辑选中的词条。"),
					&SPolyglotDictionaryEditor::HandleEditClicked) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))
				[ AddBtn(
					TEXT("Delete"), TEXT("删除"),
					TEXT("Remove the selected USER entry (built-ins cannot be removed, only overridden)."),
					TEXT("删除选中的用户词条（内置词条不可删除，只能覆盖或忽略）。"),
					&SPolyglotDictionaryEditor::HandleDeleteClicked) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))
				[ AddBtn(
					TEXT("Import JSON"), TEXT("导入 JSON"),
					TEXT("Merge a Polyglot JSON dictionary into the user dictionary."),
					TEXT("把 Easy language JSON 词典合并进用户词典。"),
					&SPolyglotDictionaryEditor::HandleImportJsonClicked) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))
				[ AddBtn(
					TEXT("Import CSV"), TEXT("导入 CSV"),
					TEXT("Merge a CSV dictionary into the user dictionary."),
					TEXT("把 CSV 词典合并进用户词典。"),
					&SPolyglotDictionaryEditor::HandleImportCsvClicked) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))
				[ AddBtn(
					TEXT("Import TXT"), TEXT("导入 TXT"),
					TEXT("Import a plain-text file of English/中文 pairs (one per line or comma separated)."),
					TEXT("从纯文本文件导入“英文/中文”词条对（逐行或逗号分隔）。"),
					&SPolyglotDictionaryEditor::HandleImportTxtClicked) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))
				[ AddBtn(
					TEXT("Export JSON"), TEXT("导出 JSON"),
					TEXT("Export the visible rows as a JSON dictionary."),
					TEXT("把当前可见词条导出为 JSON 词典。"),
					&SPolyglotDictionaryEditor::HandleExportJsonClicked) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))
				[ AddBtn(
					TEXT("Export CSV"), TEXT("导出 CSV"),
					TEXT("Export the visible rows as a CSV dictionary."),
					TEXT("把当前可见词条导出为 CSV 词典。"),
					&SPolyglotDictionaryEditor::HandleExportCsvClicked) ]
			+ SHorizontalBox::Slot().AutoWidth()
				[ AddBtn(
					TEXT("Reload"), TEXT("重新加载"),
					TEXT("Reload every dictionary from disk."),
					TEXT("从磁盘重新加载全部词典。"),
					&SPolyglotDictionaryEditor::HandleReloadClicked) ];
	}

	// -----------------------------------------------------------------------
	// Culture switching
	// -----------------------------------------------------------------------

	void HandleCultureChosen(const FString& CultureCode)
	{
		UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
		if (Subsystem)
		{
			if (!Subsystem->RequestCulture(CultureCode))
			{
				SetMessage(FText::Format(
					PolyglotUI::UiText(TEXT("Culture {0} is not available on this machine."), TEXT("本机不支持语言 {0}。")),
					FText::FromString(CultureCode)));
			}
		}
	}

	void HandleCultureApplied(const FString& /*NewEngineCulture*/)
	{
		RefreshLocalizedUi();
	}

	void RebuildCultureRow()
	{
		CultureRow->ClearChildren();
		UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
		const FString EngineLang = Subsystem ? Subsystem->GetEngineLanguage() : FString();

		struct FLocalCultureButton
		{
			const TCHAR* Code;
			const TCHAR* LocalName;
		};
		static const FLocalCultureButton Buttons[] =
		{
			{ TEXT("en"), TEXT("English") },
			{ TEXT("zh-Hans"), TEXT("简体中文") },
		};

		for (const FLocalCultureButton& Button : Buttons)
		{
			const bool bActive = EngineLang.Equals(Button.Code);
			CultureRow->AddSlot()
				.AutoWidth()
				.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
				[
					SNew(SButton)
						.OnClicked_Lambda([this, Code = FString(Button.Code)]() { HandleCultureChosen(Code); return FReply::Handled(); })
						[
							SNew(STextBlock)
								.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
								.ColorAndOpacity(bActive ? FLinearColor(0.45f, 0.8f, 1.f) : FLinearColor(0.8f, 0.8f, 0.8f))
								.Text(FText::FromString(Button.LocalName))
						]
				];
		}
	}

	// -----------------------------------------------------------------------
	// Localization refresh
	// -----------------------------------------------------------------------

	void RefreshLocalizedUi()
	{
		using PolyglotUI::UiText;

		TitleText->SetText(UiText(TEXT("Easy language - Dictionary Editor"), TEXT("Easy language - 词典编辑器")));
		LanguageLabelText->SetText(UiText(TEXT("Editor language:"), TEXT("界面语言：")));
		SearchBox->SetHintText(UiText(TEXT("Search source / translation..."), TEXT("搜索原文 / 译文…")));
		UserOnlyLabelText->SetText(UiText(TEXT("User entries only"), TEXT("仅显示用户词条")));

		const bool bZh = PolyglotUI::IsChineseUi();
		for (FToolButtonRecord& Button : ToolButtons)
		{
			Button.LabelText->SetText(bZh
				? FText::FromString(Button.LabelZh)
				: FText::FromString(Button.LabelEn));
			Button.Widget->SetToolTipText(bZh
				? FText::FromString(Button.TipZh)
				: FText::FromString(Button.TipEn));
		}

		RebuildCultureRow();
		RefreshRows();

		// The transient message may carry text from the previous language.
		MessageText->SetText(FText::GetEmpty());

		const TSharedPtr<SWindow> OwningWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		if (OwningWindow.IsValid())
		{
			OwningWindow->SetTitle(UiText(TEXT("Easy language - Dictionary Editor"), TEXT("Easy language - 词典编辑器")));
		}
	}

	// -----------------------------------------------------------------------
	// Data refresh
	// -----------------------------------------------------------------------

	void RefreshData()
	{
		AllEntries.Reset();
		UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
		FPolyglotDictionary* Dictionary = Subsystem ? Subsystem->GetDictionary() : nullptr;
		if (Dictionary)
		{
			Dictionary->GetEntries(AllEntries, /*bUserOnly=*/false);
		}

		TArray<FPolyglotEntry> UserEntries;
		if (Dictionary)
		{
			Dictionary->GetEntries(UserEntries, /*bUserOnly=*/true);
		}
		UserIdentity.Reset();
		for (const FPolyglotEntry& Entry : UserEntries)
		{
			UserIdentity.Add(MakeIdentity(Entry));
		}

		RefreshRows();
	}

	static FString MakeIdentity(const FPolyglotEntry& Entry)
	{
		return Entry.Culture + TEXT("\x1F") + Entry.Namespace + TEXT("\x1F") + Entry.Key + TEXT("\x1F") + Entry.Source;
	}

	bool IsUserEntry(const FPolyglotEntry& Entry) const
	{
		return UserIdentity.Contains(MakeIdentity(Entry));
	}

	void SetMessage(const FText& Text)
	{
		if (MessageText.IsValid())
		{
			MessageText->SetText(Text);
		}
	}

	void RefreshRows()
	{
		Filtered.Reset();

		const FString Search = SearchBox->GetText().ToString().TrimStartAndEnd();
		const bool bUserOnly = UserOnlyCheck->IsChecked();

		int32 TotalWithTranslation = 0;
		for (const FPolyglotEntry& Entry : AllEntries)
		{
			if (Entry.Translation.IsEmpty())
			{
				continue;
			}
			++TotalWithTranslation;
			if (bUserOnly && !IsUserEntry(Entry))
			{
				continue;
			}
			if (!Search.IsEmpty())
			{
				if (!Entry.Source.Contains(Search) && !Entry.Translation.Contains(Search))
				{
					continue;
				}
			}
			Filtered.Add(Entry);
		}

		RowsBox->ClearChildren();
		const int32 RowsToShow = FMath::Min(Filtered.Num(), MaxVisibleRows);
		for (int32 i = 0; i < RowsToShow; ++i)
		{
			const FPolyglotEntry Entry = Filtered[i];
			const bool bSelected = i == SelectedIndex && SelectedEntryMatches(Filtered[i]);
			const bool bUser = IsUserEntry(Entry);

			RowsBox->AddSlot()
				.Padding(FMargin(0.f, 1.f))
				[
					SNew(SButton)
						.OnClicked_Lambda([this, i, Entry]()
						{
							SelectedIndex = i;
							SelectedEntry = Entry;
							const bool bSelectedIsUser = IsUserEntry(Entry);
							SetMessage(FText::Format(
								PolyglotUI::UiText(
									TEXT("Selected: {0} -> {1} ({2}{3})"),
									TEXT("已选中：{0} -> {1}（{2}{3}）")),
								AsSourceText(Entry), AsTranslationText(Entry),
								FText::FromString(Entry.Culture),
								bSelectedIsUser
									? (PolyglotUI::IsChineseUi() ? FText::FromString(TEXT("，用户")) : FText::FromString(TEXT(" [user]")))
									: FText::GetEmpty()));
							RefreshRows();
							return FReply::Handled();
						})
						.ToolTipText(PolyglotUI::UiText(
							TEXT("Click to select this entry, then use Edit / Delete."),
							TEXT("点击选中该词条，之后可用“编辑…” / “删除”。")))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
								.FillWidth(0.44f)
								.VAlign(VAlign_Center)
								.Padding(FMargin(2.f, 0.f, 6.f, 0.f))
								[
									SNew(STextBlock)
										.TextStyle(&NormalTextStyle())
										.Text(AsSourceText(Entry))
										.ColorAndOpacity(bSelected ? FLinearColor(0.4f, 0.7f, 1.f) : FLinearColor(0.85f, 0.85f, 0.85f))
								]
							+ SHorizontalBox::Slot()
								.FillWidth(0.40f)
								.VAlign(VAlign_Center)
								.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
								[
									SNew(STextBlock)
										.TextStyle(&NormalTextStyle())
										.Text(AsTranslationText(Entry))
										.ColorAndOpacity(bSelected ? FLinearColor(0.4f, 0.7f, 1.f) : FLinearColor(0.9f, 0.95f, 0.9f))
								]
							+ SHorizontalBox::Slot()
								.FillWidth(0.16f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.TextStyle(&SmallTextStyle())
										.Text(AsMetaText(Entry))
										.ColorAndOpacity(bUser ? FLinearColor(0.7f, 0.85f, 0.7f) : FLinearColor(0.5f, 0.5f, 0.5f))
								]
						]
				];
		}

		const FText Summary = FText::Format(
			PolyglotUI::UiText(
				TEXT("{0} row(s) shown of {1} (all entries with a translation: {2}; visible rows capped at {3}). Built-in rows are grey, user rows green."),
				TEXT("显示 {0} / {1} 行（已有译文的全部词条：{2}；可见行数上限 {3}）。内置词条灰色，用户词条绿色。")),
			FText::AsNumber(RowsToShow), FText::AsNumber(Filtered.Num()),
			FText::AsNumber(TotalWithTranslation), FText::AsNumber(MaxVisibleRows));
		SummaryText->SetText(Summary);
	}

	bool SelectedEntryMatches(const FPolyglotEntry& Candidate) const
	{
		return Candidate.Source == SelectedEntry.Source
			&& Candidate.Translation == SelectedEntry.Translation
			&& Candidate.Culture == SelectedEntry.Culture
			&& Candidate.Key == SelectedEntry.Key
			&& Candidate.Namespace == SelectedEntry.Namespace;
	}

	// -----------------------------------------------------------------------
	// Entry windows (single edit + bulk add)
	// -----------------------------------------------------------------------

	void OpenEntryWindow(const FPolyglotEntry& Initial, const FText& Note)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(PolyglotUI::UiText(TEXT("Easy language - Dictionary Entry"), TEXT("Easy language - 词条")))
			.ClientSize(FVector2D(560.f, 520.f))
			.SizingRule(ESizingRule::UserSized)
			.SupportsMaximize(false)
			.SupportsMinimize(false);

		TWeakPtr<SWindow> WeakWindow = Window;
		// AsShared() only returns SWidget; the concrete object is always an
		// SPolyglotDictionaryEditor (created by OpenDictionaryEditorWindow), so
		// the static down-cast back is safe. This keeps the capture non-dangling.
		TWeakPtr<SWidget> WeakSelf = AsShared();

		Window->SetContent(
			SNew(SPolyglotEntryDialog)
				.InitialEntry(Initial)
				.NoteText(Note)
				.bSourceReadOnly(false)
				.bCultureReadOnly(false)
				.OnSave(PolyglotUI::FPolyglotEntrySaveDelegate::CreateLambda(
					[WeakWindow, WeakSelf](const FPolyglotEntry& Saved)
					{
						UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
						const bool bOk = Subsystem && Subsystem->AddOrUpdateUserEntry(Saved);
						if (bOk)
						{
							PolyglotUI::Notify(PolyglotUI::UiText(TEXT("Entry saved."), TEXT("词条已保存。")));
						}
						else
						{
							PolyglotUI::Notify(PolyglotUI::UiText(
								TEXT("Entry was not saved (empty source/key or empty translation)."),
								TEXT("词条未保存（源文本/Key 为空或译文为空）。")));
						}
						const TSharedPtr<SPolyglotDictionaryEditor> Pinned =
							StaticCastSharedPtr<SPolyglotDictionaryEditor>(WeakSelf.Pin());
						if (Pinned.IsValid())
						{
							Pinned->RefreshData();
						}
						const TSharedPtr<SWindow> PinnedWindow = WeakWindow.Pin();
						if (PinnedWindow.IsValid())
						{
							PinnedWindow->RequestDestroyWindow();
						}
					}))
				.OnCancel(PolyglotUI::FPolyglotEntryCancelDelegate::CreateLambda(
					[WeakWindow]()
					{
						const TSharedPtr<SWindow> PinnedWindow = WeakWindow.Pin();
						if (PinnedWindow.IsValid())
						{
							PinnedWindow->RequestDestroyWindow();
						}
					})));

		FSlateApplication::Get().AddWindow(Window);
	}

	void OpenBatchAddWindow()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(PolyglotUI::UiText(TEXT("Easy language - Add Dictionary Entries"), TEXT("Easy language - 批量添加词条")))
			.ClientSize(FVector2D(740.f, 600.f))
			.SizingRule(ESizingRule::UserSized)
			.SupportsMaximize(false)
			.SupportsMinimize(false);

		TWeakPtr<SWindow> WeakWindow = Window;
		TWeakPtr<SWidget> WeakSelf = AsShared();

		Window->SetContent(
			SNew(SPolyglotBatchAddDialog)
				.DefaultCulture(Polyglot::Compat::Culture_ChineseSimplified)
				.OnImport(PolyglotUI::FPolyglotBatchImportDelegate::CreateLambda(
					[WeakWindow, WeakSelf](const TArray<FPolyglotEntry>& Entries, const PolyglotUI::FBulkParseResult& Stats)
					{
						UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
						FPolyglotDictionary* Dictionary = Subsystem ? Subsystem->GetDictionary() : nullptr;
						int32 Merged = 0;
						if (Dictionary)
						{
							Merged = Dictionary->ImportUserEntries(Entries);
							if (Merged > 0)
							{
								Subsystem->ReloadDictionaries();
							}
						}

						const TSharedPtr<SPolyglotDictionaryEditor> Pinned =
							StaticCastSharedPtr<SPolyglotDictionaryEditor>(WeakSelf.Pin());
						if (Pinned.IsValid())
						{
							Pinned->SelectedIndex = INDEX_NONE;
							Pinned->SelectedEntry = FPolyglotEntry();
							Pinned->RefreshData();
							if (Merged == 0)
							{
								Pinned->SetMessage(PolyglotUI::UiText(
									TEXT("Nothing was added (the parsed pairs may already exist)."),
									TEXT("没有新增（解析出的词条可能已存在）。")));
							}
							else
							{
								Pinned->SetMessage(FText::Format(
									PolyglotUI::UiText(
										TEXT("Bulk add finished: parsed {0} item(s), merged {1} into the user dictionary, {2} skipped."),
										TEXT("批量添加完成：解析 {0} 项，合并 {1} 项到用户词典，跳过 {2} 项。")),
									FText::AsNumber(Stats.ItemCount),
									FText::AsNumber(Merged),
									FText::AsNumber(Stats.SkippedCount)));
							}
						}

						const TSharedPtr<SWindow> PinnedWindow = WeakWindow.Pin();
						if (PinnedWindow.IsValid())
						{
							PinnedWindow->RequestDestroyWindow();
						}
					}))
				.OnCancel(PolyglotUI::FPolyglotBatchCancelDelegate::CreateLambda(
					[WeakWindow]()
					{
						const TSharedPtr<SWindow> PinnedWindow = WeakWindow.Pin();
						if (PinnedWindow.IsValid())
						{
							PinnedWindow->RequestDestroyWindow();
						}
					})));

		FSlateApplication::Get().AddWindow(Window);
	}

	// -----------------------------------------------------------------------
	// Toolbar handlers
	// -----------------------------------------------------------------------

	FReply HandleAddClicked()
	{
		OpenBatchAddWindow();
		return FReply::Handled();
	}

	FReply HandleEditClicked()
	{
		if (Filtered.IsValidIndex(SelectedIndex))
		{
			OpenEntryWindow(Filtered[SelectedIndex], PolyglotUI::UiText(
				TEXT("Editing this entry. Save writes it back to the user dictionary."),
				TEXT("正在编辑该词条。保存后将写回用户词典。")));
		}
		else
		{
			SetMessage(PolyglotUI::UiText(
				TEXT("Select a row first (click it), then press Edit."),
				TEXT("请先点击选中一行，再点“编辑…”。")));
		}
		return FReply::Handled();
	}

	FReply HandleDeleteClicked()
	{
		if (!Filtered.IsValidIndex(SelectedIndex))
		{
			SetMessage(PolyglotUI::UiText(
				TEXT("Select a row first, then press Delete."),
				TEXT("请先选中一行，再点“删除”。")));
			return FReply::Handled();
		}

		const FPolyglotEntry Entry = Filtered[SelectedIndex];
		if (!IsUserEntry(Entry))
		{
			SetMessage(PolyglotUI::UiText(
				TEXT("This is a built-in entry. Built-ins cannot be removed; add the same source with an empty translation instead to ignore it, or override it with your own translation."),
				TEXT("这是内置词条，无法删除。可用空译文添加同一条源文本来忽略它，或用你自己的译文覆盖它。")));
			return FReply::Handled();
		}

		UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
		if (Subsystem)
		{
			FPolyglotEntry Removed = Entry;
			Removed.Translation.Empty();
			if (Subsystem->RemoveUserEntry(Removed))
			{
				SetMessage(PolyglotUI::UiText(TEXT("User entry removed."), TEXT("用户词条已删除。")));
			}
			else
			{
				SetMessage(PolyglotUI::UiText(
					TEXT("Could not locate that exact user entry (it may have been edited since)."),
					TEXT("找不到完全匹配的用户词条（可能已被修改）。")));
			}
			SelectedIndex = INDEX_NONE;
			SelectedEntry = FPolyglotEntry();
			RefreshData();
		}
		return FReply::Handled();
	}

	FReply HandleReloadClicked()
	{
		UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
		if (Subsystem)
		{
			Subsystem->ReloadDictionaries();
		}
		SelectedIndex = INDEX_NONE;
		SelectedEntry = FPolyglotEntry();
		RefreshData();
		SetMessage(PolyglotUI::UiText(TEXT("Dictionaries reloaded."), TEXT("词典已重新加载。")));
		return FReply::Handled();
	}

	// -----------------------------------------------------------------------
	// File pickers + imports
	// -----------------------------------------------------------------------

	bool PickImportFile(FString& OutPath, const FText& Title, const FString& Filter)
	{
		IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
		if (!Desktop)
		{
			SetMessage(PolyglotUI::UiText(
				TEXT("File dialogs are not available in this context."),
				TEXT("当前环境不可用文件对话框。")));
			return false;
		}
		TArray<FString> Files;
		const bool bPicked = Desktop->OpenFileDialog(
			nullptr,
			Title.ToString(),
			PolyglotSettings::GetSavedPolyglotDir(),
			TEXT(""),
			Filter,
			EFileDialogFlags::None,
			Files);
		if (!bPicked || Files.Num() == 0)
		{
			return false;
		}
		OutPath = Files[0];
		return true;
	}

	bool PickExportFile(FString& OutPath, const FText& Title, const FString& DefaultName, const FString& Filter)
	{
		IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
		if (!Desktop)
		{
			SetMessage(PolyglotUI::UiText(
				TEXT("File dialogs are not available in this context."),
				TEXT("当前环境不可用文件对话框。")));
			return false;
		}
		TArray<FString> Files;
		const bool bPicked = Desktop->SaveFileDialog(
			nullptr,
			Title.ToString(),
			PolyglotSettings::GetSavedPolyglotDir(),
			DefaultName,
			Filter,
			EFileDialogFlags::None,
			Files);
		if (!bPicked || Files.Num() == 0)
		{
			return false;
		}
		OutPath = Files[0];
		return true;
	}

	FReply HandleImportJsonClicked()
	{
		FString Path;
		if (PickImportFile(Path,
			PolyglotUI::UiText(TEXT("Import JSON dictionary"), TEXT("导入 JSON 词典")),
			TEXT("Easy language JSON (*.json)|*.json")))
		{
			ImportEntriesFromFile(Path, /*bJson=*/true);
		}
		return FReply::Handled();
	}

	FReply HandleImportCsvClicked()
	{
		FString Path;
		if (PickImportFile(Path,
			PolyglotUI::UiText(TEXT("Import CSV dictionary"), TEXT("导入 CSV 词典")),
			TEXT("Easy language CSV (*.csv)|*.csv")))
		{
			ImportEntriesFromFile(Path, /*bJson=*/false);
		}
		return FReply::Handled();
	}

	FReply HandleImportTxtClicked()
	{
		FString Path;
		if (!PickImportFile(Path,
			PolyglotUI::UiText(TEXT("Import TXT pairs"), TEXT("导入 TXT 词条对")),
			TEXT("Text list (*.txt)|*.txt")))
		{
			return FReply::Handled();
		}

		FString Content;
		if (!PolyglotFileUtil::ReadUtf8FileToString(Path, Content))
		{
			SetMessage(PolyglotUI::UiText(
				TEXT("Could not read the text file (UTF-8 expected)."),
				TEXT("无法读取该文本文件（应为 UTF-8）。")));
			return FReply::Handled();
		}

		TArray<FPolyglotEntry> Parsed;
		const PolyglotUI::FBulkParseResult Stats = PolyglotUI::ParseBulkEntryText(
			Content, Polyglot::Compat::Culture_ChineseSimplified, Parsed);
		MergeParsedBulk(Parsed, Stats);
		return FReply::Handled();
	}

	void ImportEntriesFromFile(const FString& Path, bool bJson)
	{
		TArray<FPolyglotEntry> Parsed;
		const int32 ParsedCount = bJson
			? ReadEntriesJsonFromFile(Path, Parsed)
			: (ReadEntriesCsvFromFile(Path, Parsed) ? Parsed.Num() : 0);

		if (Parsed.Num() == 0)
		{
			SetMessage(PolyglotUI::UiText(
				TEXT("The file contained no readable entries."),
				TEXT("文件里没有可读取的词条。")));
			return;
		}

		UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
		FPolyglotDictionary* Dictionary = Subsystem ? Subsystem->GetDictionary() : nullptr;
		if (!Dictionary)
		{
			SetMessage(PolyglotUI::UiText(
				TEXT("Easy language subsystem unavailable."),
				TEXT("Easy language 子系统不可用。")));
			return;
		}

		const int32 Merged = Dictionary->ImportUserEntries(Parsed);
		if (Merged > 0)
		{
			Subsystem->ReloadDictionaries();
		}
		SelectedIndex = INDEX_NONE;
		SelectedEntry = FPolyglotEntry();
		RefreshData();
		SetMessage(FText::Format(
			PolyglotUI::UiText(
				TEXT("Parsed {0} entry(ies); merged {1} into the user dictionary."),
				TEXT("解析到 {0} 条词条；已合并 {1} 条到用户词典。")),
			FText::AsNumber(ParsedCount), FText::AsNumber(Merged)));
	}

	/** Shared tail used by Import TXT: merge parsed plain-text pairs. */
	void MergeParsedBulk(const TArray<FPolyglotEntry>& Parsed, const PolyglotUI::FBulkParseResult& Stats)
	{
		if (Parsed.Num() == 0)
		{
			SetMessage(PolyglotUI::UiText(
				TEXT("The file contained no readable \"English/中文\" pairs."),
				TEXT("文件里没有可读取的“英文/中文”词条对。")));
			return;
		}

		UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
		FPolyglotDictionary* Dictionary = Subsystem ? Subsystem->GetDictionary() : nullptr;
		if (!Dictionary)
		{
			SetMessage(PolyglotUI::UiText(
				TEXT("Easy language subsystem unavailable."),
				TEXT("Easy language 子系统不可用。")));
			return;
		}

		const int32 Merged = Dictionary->ImportUserEntries(Parsed);
		if (Merged > 0)
		{
			Subsystem->ReloadDictionaries();
		}
		SelectedIndex = INDEX_NONE;
		SelectedEntry = FPolyglotEntry();
		RefreshData();
		SetMessage(FText::Format(
			PolyglotUI::UiText(
				TEXT("TXT import: parsed {0} item(s), merged {1} into the user dictionary, {2} skipped."),
				TEXT("TXT 导入：解析 {0} 项，合并 {1} 项到用户词典，跳过 {2} 项。")),
			FText::AsNumber(Stats.ItemCount),
			FText::AsNumber(Merged),
			FText::AsNumber(Stats.SkippedCount)));
	}

	FReply HandleExportJsonClicked()
	{
		ExportVisibleEntries(/*bJson=*/true);
		return FReply::Handled();
	}

	FReply HandleExportCsvClicked()
	{
		ExportVisibleEntries(/*bJson=*/false);
		return FReply::Handled();
	}

	void ExportVisibleEntries(bool bJson)
	{
		if (Filtered.Num() == 0)
		{
			SetMessage(PolyglotUI::UiText(
				TEXT("Nothing to export (the filter is empty). Clear the search or check your entries."),
				TEXT("没有可导出的内容（当前过滤结果为空）。请清空搜索或检查词条。")));
			return;
		}

		FString Path;
		const FString Description = bJson ? TEXT("JSON") : TEXT("CSV");
		const bool bPicked = PickExportFile(Path,
			bJson
				? PolyglotUI::UiText(TEXT("Export JSON dictionary"), TEXT("导出 JSON 词典"))
				: PolyglotUI::UiText(TEXT("Export CSV dictionary"), TEXT("导出 CSV 词典")),
			bJson ? TEXT("export.json") : TEXT("export.csv"),
			bJson ? TEXT("Easy language JSON (*.json)|*.json") : TEXT("Easy language CSV (*.csv)|*.csv"));
		if (!bPicked)
		{
			return;
		}

		bool bOk = false;
		if (bJson)
		{
			bOk = WriteEntriesJsonToFile(Path, Filtered);
		}
		else
		{
			const FString Csv = PolyglotCsv::WriteEntries(Filtered);
			bOk = PolyglotFileUtil::WriteUtf8StringToFile(Path, Csv);
		}

		SetMessage(bOk
			? FText::Format(
				PolyglotUI::UiText(TEXT("Exported {0} row(s) to {1}."), TEXT("已导出 {0} 行到 {1}。")),
				FText::AsNumber(Filtered.Num()), FText::FromString(Path))
			: PolyglotUI::UiText(TEXT("Export failed."), TEXT("导出失败。")));
	}

	const FTextBlockStyle& NormalTextStyle() const
	{
		return FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
	}

	const FTextBlockStyle& SmallTextStyle() const
	{
		return FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText");
	}

	TSharedPtr<STextBlock> TitleText;
	TSharedPtr<STextBlock> LanguageLabelText;
	TSharedPtr<SHorizontalBox> CultureRow;
	TArray<FToolButtonRecord> ToolButtons;

	TSharedPtr<SEditableTextBox> SearchBox;
	TSharedPtr<SCheckBox> UserOnlyCheck;
	TSharedPtr<STextBlock> UserOnlyLabelText;
	TSharedPtr<STextBlock> MessageText;
	TSharedPtr<STextBlock> SummaryText;
	TSharedPtr<SScrollBox> RowsBox;

	TArray<FPolyglotEntry> AllEntries;
	TArray<FPolyglotEntry> Filtered;
	TSet<FString> UserIdentity;

	int32 SelectedIndex = INDEX_NONE;
	FPolyglotEntry SelectedEntry;
};

namespace PolyglotUI
{
	static TWeakPtr<SWindow> DictionaryEditorWindowWeak;

	void OpenDictionaryEditorWindow()
	{
		if (FSlateApplication::IsInitialized() == false)
		{
			return;
		}

		const TSharedPtr<SWindow> Existing = DictionaryEditorWindowWeak.Pin();
		if (Existing.IsValid())
		{
			Existing->BringToFront();
			return;
		}

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(UiText(TEXT("Easy language - Dictionary Editor"), TEXT("Easy language - 词典编辑器")))
			.ClientSize(FVector2D(960.f, 600.f))
			.SizingRule(ESizingRule::UserSized)
			.SupportsMaximize(true)
			.SupportsMinimize(true)
			.AutoCenter(EAutoCenter::PreferredWorkArea);

		Window->SetContent(SNew(SPolyglotDictionaryEditor));
		FSlateApplication::Get().AddWindow(Window);
		DictionaryEditorWindowWeak = Window;
	}
}

#undef LOCTEXT_NAMESPACE
