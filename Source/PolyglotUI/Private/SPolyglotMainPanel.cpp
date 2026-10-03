// Copyright 2026 BaiMaoMaLou. All Rights Reserved.
//
// Main control panel.
//
// A single window that controls the whole plugin:
//   - enable / disable,
//   - switch editor language (shortcuts for zh-Hans / en plus more cultures),
//   - scan layer options,
//   - status of the specialised editor areas (Niagara / Material / Blueprint /
//     Content Browser) reported by the PolyglotAdapters module,
//   - links to the dictionary editor and the visual picker.
//
// The whole window is bilingual (English / Simplified Chinese): every user
// facing string is resolved through PolyglotUI::UiText against the current
// engine culture, and the panel subscribes to UPolyglotSubsystem::OnCultureApplied
// so all strings (including the window title) follow live language switches.

#include "PolyglotUIInternal.h"

#include "Framework/Application/SlateApplication.h"
#include "PolyglotAdapters.h"
#include "PolyglotDictionary.h"
#include "PolyglotSubsystem.h"
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

#define LOCTEXT_NAMESPACE "SPolyglotMainPanel"

namespace
{
	struct FCultureChoice
	{
		FString Code;
		FText Label;
	};

	/** Language buttons always show each language under its own local name. */
	const TArray<FCultureChoice>& CultureChoices()
	{
		static const TArray<FCultureChoice> Choices =
		{
			{ TEXT("en"),      FText::FromString(TEXT("English")) },
			{ TEXT("zh-Hans"), FText::FromString(TEXT("简体中文")) },
			{ TEXT("ja"),      FText::FromString(TEXT("日本語")) },
			{ TEXT("ko"),      FText::FromString(TEXT("한국어")) },
			{ TEXT("de"),      FText::FromString(TEXT("Deutsch")) },
			{ TEXT("fr"),      FText::FromString(TEXT("Français")) },
		};
		return Choices;
	}
}

class SPolyglotMainPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPolyglotMainPanel) {}
	SLATE_END_ARGS()

	~SPolyglotMainPanel()
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
		const FTextBlockStyle& HeaderStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("DetailsView.CategoryTextStyle");
		const FEditableTextBoxStyle& EditBoxStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");

		EnabledCheck = SNew(SCheckBox)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
			{
				UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
				if (Subsystem)
				{
					Subsystem->SetEnabled(State == ECheckBoxState::Checked);
				}
				Refresh();
			});

		StatusText = SNew(STextBlock)
			.TextStyle(&NormalStyle)
			.AutoWrapText(true);

		IntervalBox = SNew(SEditableTextBox)
			.Style(&EditBoxStyle);
		BudgetBox = SNew(SEditableTextBox)
			.Style(&EditBoxStyle);

		StaticLabelText = SNew(STextBlock).TextStyle(&SmallStyle);
		StaticCheck = SNew(SCheckBox)[ StaticLabelText.ToSharedRef() ];
		EditableLabelText = SNew(STextBlock).TextStyle(&SmallStyle);
		EditableCheck = SNew(SCheckBox)[ EditableLabelText.ToSharedRef() ];

		CountsText = SNew(STextBlock)
			.TextStyle(&SmallStyle)
			.AutoWrapText(true);

		AdaptersText = SNew(STextBlock)
			.TextStyle(&SmallStyle)
			.AutoWrapText(true);

		MessageText = SNew(STextBlock)
			.TextStyle(&SmallStyle)
			.ColorAndOpacity(FLinearColor(0.8f, 0.7f, 0.2f))
			.AutoWrapText(true);

		TSharedRef<SVerticalBox> Panel = SNew(SVerticalBox);

		// ------------------------------------------------------------------
		// Header
		// ------------------------------------------------------------------
		TitleText = SNew(STextBlock)
			.TextStyle(&HeaderStyle);
		Panel->AddSlot()
			.AutoHeight()
			[ TitleText.ToSharedRef() ];

		// ------------------------------------------------------------------
		// Enable + status
		// ------------------------------------------------------------------
		EnabledLabelText = SNew(STextBlock).TextStyle(&NormalStyle);
		Panel->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 6.f, 0.f, 2.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[ EnabledLabelText.ToSharedRef() ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6.f, 0.f, 0.f, 0.f))
					[ EnabledCheck.ToSharedRef() ]
			];

		Panel->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 2.f, 0.f, 2.f))
			[ StatusText.ToSharedRef() ];

		// ------------------------------------------------------------------
		// Culture switcher (English / 简体中文 quick buttons + extra cultures)
		// ------------------------------------------------------------------
		CultureHeaderText = SNew(STextBlock).TextStyle(&HeaderStyle);
		Panel->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 8.f, 0.f, 2.f))
			[ CultureHeaderText.ToSharedRef() ];
		CultureRow = SNew(SHorizontalBox);
		Panel->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 2.f, 0.f, 2.f))
			[ CultureRow.ToSharedRef() ];
		CultureHintText = SNew(STextBlock)
			.TextStyle(&SmallStyle)
			.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
			.AutoWrapText(true);
		Panel->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 2.f, 0.f, 6.f))
			[ CultureHintText.ToSharedRef() ];

		// ------------------------------------------------------------------
		// Scan options
		// ------------------------------------------------------------------
		ScanHeaderText = SNew(STextBlock).TextStyle(&HeaderStyle);
		Panel->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 4.f, 0.f, 2.f))
			[ ScanHeaderText.ToSharedRef() ];

		IntervalLabelText = SNew(STextBlock).TextStyle(&SmallStyle);
		BudgetLabelText = SNew(STextBlock).TextStyle(&SmallStyle);
		ApplyScanButton = MakeButton([this]() { HandleApplyScanClicked(); return FReply::Handled(); }, ApplyScanButtonText);
		Panel->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 2.f, 0.f, 2.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[ IntervalLabelText.ToSharedRef() ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.f, 0.f, 18.f, 0.f))
					[ SNew(SBox).WidthOverride(70.f)[ IntervalBox.ToSharedRef() ] ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[ BudgetLabelText.ToSharedRef() ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.f, 0.f, 18.f, 0.f))
					[ SNew(SBox).WidthOverride(70.f)[ BudgetBox.ToSharedRef() ] ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[ ApplyScanButton.ToSharedRef() ]
			];
		Panel->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 2.f, 0.f, 6.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))[ StaticCheck.ToSharedRef() ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))[ EditableCheck.ToSharedRef() ]
			];

		// ------------------------------------------------------------------
		// Counts
		// ------------------------------------------------------------------
		CountsHeaderText = SNew(STextBlock).TextStyle(&HeaderStyle);
		Panel->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 4.f, 0.f, 2.f))
			[ CountsHeaderText.ToSharedRef() ];
		Panel->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 2.f, 0.f, 2.f))
			[ CountsText.ToSharedRef() ];

		OpenDictionaryButton = MakeButton([this]() { HandleOpenDictionaryClicked(); return FReply::Handled(); }, OpenDictionaryButtonText);
		StartPickerButton = MakeButton([this]() { HandleStartPickerClicked(); return FReply::Handled(); }, StartPickerButtonText);
		ReloadButton = MakeButton([this]() { HandleReloadClicked(); return FReply::Handled(); }, ReloadButtonText);
		RestoreButton = MakeButton([this]() { HandleRestoreClicked(); return FReply::Handled(); }, RestoreButtonText);
		Panel->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 2.f, 0.f, 6.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))
					[ OpenDictionaryButton.ToSharedRef() ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))
					[ StartPickerButton.ToSharedRef() ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))
					[ ReloadButton.ToSharedRef() ]
				+ SHorizontalBox::Slot().AutoWidth()
					[ RestoreButton.ToSharedRef() ]
			];

		// ------------------------------------------------------------------
		// Adapter / coverage status
		// ------------------------------------------------------------------
		CoverageHeaderText = SNew(STextBlock).TextStyle(&HeaderStyle);
		Panel->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 4.f, 0.f, 2.f))
			[ CoverageHeaderText.ToSharedRef() ];
		Panel->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 2.f, 0.f, 2.f))
			[ AdaptersText.ToSharedRef() ];

		Panel->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 6.f, 0.f, 2.f))
			[ MessageText.ToSharedRef() ];

		ChildSlot
			[
				SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
					.Padding(12.f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
							[ Panel ]
					]
			];

		RefreshLocalizedUi();

		if (UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem())
		{
			Subsystem->OnCultureApplied().AddRaw(this, &SPolyglotMainPanel::HandleCultureApplied);
		}
	}

private:
	TSharedRef<SButton> MakeButton(
		TFunction<FReply()>&& Handler,
		TSharedPtr<STextBlock>& OutLabelText)
	{
		TSharedRef<STextBlock> LabelText = SNew(STextBlock)
			.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"));
		OutLabelText = LabelText;
		return SNew(SButton)
			.OnClicked(FOnClicked::CreateLambda(MoveTemp(Handler)))
			[ LabelText ];
	}

	// -----------------------------------------------------------------------
	// Handlers
	// -----------------------------------------------------------------------

	FReply HandleOpenDictionaryClicked()
	{
		PolyglotUI::OpenDictionaryEditorWindow();
		return FReply::Handled();
	}

	FReply HandleStartPickerClicked()
	{
		PolyglotUI::TogglePickerMode();
		MessageText->SetText(PolyglotUI::UiText(
			TEXT("Picker mode toggled. Click any editor text to translate it; press Esc to leave."),
			TEXT("已切换拾取模式。点击任意界面文字进行翻译；按 Esc 退出。")));
		return FReply::Handled();
	}

	FReply HandleReloadClicked()
	{
		UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
		if (Subsystem)
		{
			Subsystem->ReloadDictionaries();
		}
		Refresh();
		MessageText->SetText(PolyglotUI::UiText(
			TEXT("Dictionaries reloaded."),
			TEXT("词典已重新加载。")));
		return FReply::Handled();
	}

	FReply HandleRestoreClicked()
	{
		UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
		if (Subsystem)
		{
			Subsystem->RestoreAllWidgets();
		}
		MessageText->SetText(PolyglotUI::UiText(
			TEXT("All patched widgets restored to their original text."),
			TEXT("所有被改写的控件已恢复为原始文本。")));
		return FReply::Handled();
	}

	FReply HandleApplyScanClicked()
	{
		UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
		if (!Subsystem)
		{
			return FReply::Handled();
		}

		const float Interval = FCString::Atof(*IntervalBox->GetText().ToString());
		const float Budget = FCString::Atof(*BudgetBox->GetText().ToString());
		Subsystem->SetScanParameters(
			FMath::Max(0.05f, Interval),
			FMath::Max(0.1f, Budget),
			StaticCheck->IsChecked(),
			EditableCheck->IsChecked());
		Refresh();
		MessageText->SetText(PolyglotUI::UiText(
			TEXT("Scan options applied."),
			TEXT("扫描选项已应用。")));
		return FReply::Handled();
	}

	void HandleCultureChosen(const FString& CultureCode)
	{
		UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
		if (Subsystem)
		{
			if (!Subsystem->RequestCulture(CultureCode))
			{
				MessageText->SetText(FText::Format(
					PolyglotUI::UiText(
						TEXT("Culture {0} is not available on this machine."),
						TEXT("本机不支持语言 {0}。")),
					FText::FromString(CultureCode)));
			}
		}
		Refresh();
	}

	// -----------------------------------------------------------------------
	// Localization / state refresh
	// -----------------------------------------------------------------------

	/** Called by the subsystem right after a culture switch/revert finished. */
	void HandleCultureApplied(const FString& /*NewEngineCulture*/)
	{
		RefreshLocalizedUi();
	}

	/** Re-apply every bilingual string (static labels + dynamic state). */
	void RefreshLocalizedUi()
	{
		using PolyglotUI::UiText;

		TitleText->SetText(UiText(TEXT("Easy language - Editor Localization"), TEXT("Easy language - 编辑器本地化")));
		EnabledLabelText->SetText(UiText(TEXT("Translations enabled"), TEXT("开启翻译")));
		CultureHeaderText->SetText(UiText(TEXT("Editor language"), TEXT("界面语言")));
		CultureHintText->SetText(UiText(
			TEXT("Switching happens live - the editor does not restart and the project is not reloaded. English / 简体中文 switch instantly."),
			TEXT("切换即时生效，编辑器无需重启、工程不会重载。English / 简体中文 一键互切。")));
		ScanHeaderText->SetText(UiText(TEXT("Scan layer options"), TEXT("扫描层选项")));
		IntervalLabelText->SetText(UiText(TEXT("Scan interval (s)"), TEXT("扫描间隔（秒）")));
		BudgetLabelText->SetText(UiText(TEXT("Frame budget (ms)"), TEXT("每帧预算（毫秒）")));
		StaticLabelText->SetText(UiText(
			TEXT("Translate static text (STextBlock / rich text)"),
			TEXT("翻译静态文本（STextBlock / 富文本）")));
		EditableLabelText->SetText(UiText(
			TEXT("Translate editable text boxes (slower, can disturb input)"),
			TEXT("翻译可编辑文本框（较慢，可能干扰输入）")));
		CountsHeaderText->SetText(UiText(TEXT("Dictionary"), TEXT("词典")));
		CoverageHeaderText->SetText(UiText(TEXT("Coverage in this editor session"), TEXT("本编辑器会话的覆盖情况")));

		ApplyScanButtonText->SetText(UiText(TEXT("Apply"), TEXT("应用")));
		OpenDictionaryButtonText->SetText(UiText(TEXT("Dictionary editor"), TEXT("词典编辑器")));
		StartPickerButtonText->SetText(UiText(TEXT("Pick text to translate"), TEXT("拾取文字翻译")));
		ReloadButtonText->SetText(UiText(TEXT("Reload dictionaries"), TEXT("重新加载词典")));
		RestoreButtonText->SetText(UiText(TEXT("Restore all widgets"), TEXT("还原所有控件")));

		Refresh();

		// Keep the OS window title in sync with the current language.
		const TSharedPtr<SWindow> OwningWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		if (OwningWindow.IsValid())
		{
			OwningWindow->SetTitle(UiText(TEXT("Easy language - Editor Localization"), TEXT("Easy language - 编辑器本地化")));
		}
	}

	void Refresh()
	{
		UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
		if (!Subsystem)
		{
			StatusText->SetText(PolyglotUI::UiText(
				TEXT("Polyglot subsystem is not available."),
				TEXT("本地化子系统不可用。")));
			return;
		}

		const bool bEnabled = Subsystem->IsEnabled();
		EnabledCheck->SetIsChecked(bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

		const FString Culture = Subsystem->GetActiveCulture();
		StatusText->SetText(FText::Format(
			PolyglotUI::UiText(
				TEXT("Enabled: {0}    Active culture: {1}    (default editor language is restored when you disable the plugin)"),
				TEXT("已启用：{0}    当前语言：{1}    （关闭插件时恢复编辑器默认语言）")),
			bEnabled ? PolyglotUI::UiText(TEXT("Yes"), TEXT("是")) : PolyglotUI::UiText(TEXT("No"), TEXT("否")),
			Culture.IsEmpty() ? FText::FromString(TEXT("-")) : FText::FromString(Culture)));

		// Culture quick buttons (labels are fixed local names, rebuilt on demand).
		CultureRow->ClearChildren();
		for (const FCultureChoice& Choice : CultureChoices())
		{
			const bool bActive = Choice.Code.Equals(Culture);
			CultureRow->AddSlot()
				.AutoWidth()
				.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
				[
					SNew(SButton)
						.OnClicked_Lambda([this, Code = Choice.Code]() { HandleCultureChosen(Code); return FReply::Handled(); })
						[
							SNew(STextBlock)
								.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
								.ColorAndOpacity(bActive ? FLinearColor(0.45f, 0.8f, 1.f) : FLinearColor(0.8f, 0.8f, 0.8f))
								.Text(Choice.Label)
						]
				];
		}

		IntervalBox->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), Subsystem->GetScanIntervalSeconds())));
		BudgetBox->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Subsystem->GetScanTimeBudgetMs())));
		StaticCheck->SetIsChecked(Subsystem->GetTranslateStaticTextBlocks() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
		EditableCheck->SetIsChecked(Subsystem->GetTranslateEditableText() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

		// Dictionary counts.
		FPolyglotDictionary* Dictionary = Subsystem->GetDictionary();
		if (Dictionary)
		{
			TArray<FPolyglotEntry> UserEntries;
			Dictionary->GetEntries(UserEntries, /*bUserOnly=*/true);
			CountsText->SetText(FText::Format(
				PolyglotUI::UiText(
					TEXT("{0} entries loaded, {1} of them user entries (written by the picker / dictionary editor)."),
					TEXT("已加载 {0} 条词条，其中 {1} 条为用户词条（由拾取器 / 词典编辑器写入）。")),
				FText::AsNumber(Dictionary->GetEntryCount()), FText::AsNumber(UserEntries.Num())));
		}
		else
		{
			CountsText->SetText(FText::GetEmpty());
		}

		// Adapter status.
		BuildAdapterLines();
	}

	void BuildAdapterLines()
	{
		const TArray<PolyglotAdapters::FAdapterStatus> Statuses = PolyglotAdapters::QueryAllAdapterStatuses();
		FString Text;
		for (const PolyglotAdapters::FAdapterStatus& Status : Statuses)
		{
			FString Line = Status.DisplayName;
			if (!Status.bModulesExist)
			{
				Line += PolyglotUI::IsChineseUi() ? TEXT("  [本引擎构建中无此模块]") : TEXT("  [modules absent in this engine build]");
			}
			else if (!Status.bModulesLoaded)
			{
				Line += PolyglotUI::IsChineseUi() ? TEXT("  [模块已加载，编辑器窗口未打开]") : TEXT("  [loaded, editor window not open]");
			}
			else
			{
				Line += PolyglotUI::IsChineseUi() ? TEXT("  [生效中]") : TEXT("  [active]");
			}
			Line += TEXT("\n   ");
			Line += Status.ScopeDescription;
			Text += Line + TEXT("\n");
		}
		AdaptersText->SetText(FText::FromString(Text.TrimEnd()));
	}

	// Widgets whose text is refreshed by RefreshLocalizedUi.
	TSharedPtr<STextBlock> TitleText;
	TSharedPtr<STextBlock> EnabledLabelText;
	TSharedPtr<STextBlock> CultureHeaderText;
	TSharedPtr<STextBlock> CultureHintText;
	TSharedPtr<STextBlock> ScanHeaderText;
	TSharedPtr<STextBlock> IntervalLabelText;
	TSharedPtr<STextBlock> BudgetLabelText;
	TSharedPtr<STextBlock> CountsHeaderText;
	TSharedPtr<STextBlock> CoverageHeaderText;

	TSharedPtr<SCheckBox> EnabledCheck;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<SHorizontalBox> CultureRow;
	TSharedPtr<SEditableTextBox> IntervalBox;
	TSharedPtr<SEditableTextBox> BudgetBox;
	TSharedPtr<SCheckBox> StaticCheck;
	TSharedPtr<STextBlock> StaticLabelText;
	TSharedPtr<SCheckBox> EditableCheck;
	TSharedPtr<STextBlock> EditableLabelText;
	TSharedPtr<STextBlock> CountsText;
	TSharedPtr<STextBlock> AdaptersText;
	TSharedPtr<STextBlock> MessageText;

	TSharedPtr<SButton> ApplyScanButton;
	TSharedPtr<STextBlock> ApplyScanButtonText;
	TSharedPtr<SButton> OpenDictionaryButton;
	TSharedPtr<STextBlock> OpenDictionaryButtonText;
	TSharedPtr<SButton> StartPickerButton;
	TSharedPtr<STextBlock> StartPickerButtonText;
	TSharedPtr<SButton> ReloadButton;
	TSharedPtr<STextBlock> ReloadButtonText;
	TSharedPtr<SButton> RestoreButton;
	TSharedPtr<STextBlock> RestoreButtonText;
};

namespace PolyglotUI
{
	static TWeakPtr<SWindow> MainPanelWindowWeak;

	void OpenMainPanel()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}

		const TSharedPtr<SWindow> Existing = MainPanelWindowWeak.Pin();
		if (Existing.IsValid())
		{
			Existing->BringToFront();
			return;
		}

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(UiText(TEXT("Easy language - Editor Localization"), TEXT("Easy language - 编辑器本地化")))
			.ClientSize(FVector2D(720.f, 720.f))
			.SizingRule(ESizingRule::UserSized)
			.SupportsMaximize(true)
			.SupportsMinimize(true)
			.AutoCenter(EAutoCenter::PreferredWorkArea);

		Window->SetContent(SNew(SPolyglotMainPanel));
		FSlateApplication::Get().AddWindow(Window);
		MainPanelWindowWeak = Window;
	}
}

#undef LOCTEXT_NAMESPACE
