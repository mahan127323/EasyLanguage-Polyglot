// Copyright 2026 BaiMaoMaLou. All Rights Reserved.
//
// Visual text picker.
//
// Entering picker mode opens a small HUD window. While the HUD is open an input
// processor watches the mouse: moving shows the hovered text (source + widget
// path) in the HUD, clicking captures that text and switches the HUD to a small
// translation form. Saving writes the entry to the user dictionary through
// UPolyglotSubsystem::AddOrUpdateUserEntry (which re-applies everything) and
// returns to hover mode. Esc / the Stop button leave picker mode.
//
// The picker only ever reads Slate widgets (through FPolyglotWidgetPatcher) and
// never writes to the engine or to any asset.

#include "PolyglotUIInternal.h"

#include "Containers/Ticker.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Internationalization/Text.h"
#include "Layout/WidgetPath.h"
#include "PolyglotCompat.h"
#include "PolyglotDictionary.h"
#include "PolyglotLog.h"
#include "PolyglotSubsystem.h"
#include "PolyglotWidgetPatcher.h"
#include "SPolyglotEntryDialog.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SPolyglotPicker"

namespace
{
	/** East-Asian text heuristic (mirrors the scanner's script test). */
	bool ContainsEastAsianText(const FString& InText)
	{
		for (const TCHAR Char : InText)
		{
			const uint32 CodePoint = static_cast<uint32>(Char);
			if ((CodePoint >= 0x2E80 && CodePoint <= 0x9FFF)
				|| (CodePoint >= 0x3040 && CodePoint <= 0x30FF)
				|| (CodePoint >= 0xAC00 && CodePoint <= 0xD7AF)
				|| (CodePoint >= 0xF900 && CodePoint <= 0xFAFF)
				|| (CodePoint >= 0x3400 && CodePoint <= 0x4DBF))
			{
				return true;
			}
		}
		return false;
	}

	bool IsCjkCulture(const FString& CultureName)
	{
		return CultureName.StartsWith(TEXT("zh"))
			|| CultureName.StartsWith(TEXT("ja"))
			|| CultureName.StartsWith(TEXT("ko"));
	}

	const FTextBlockStyle& NormalTextStyle()
	{
		return FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
	}

	const FTextBlockStyle& SmallTextStyle()
	{
		return FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText");
	}

	const FTextBlockStyle& HeaderTextStyle()
	{
		return FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("DetailsView.CategoryTextStyle");
	}

	// -----------------------------------------------------------------------
	// Data captured about the widget under the cursor.
	// -----------------------------------------------------------------------
	struct FPickedText
	{
		bool bFound = false;
		FString Source;
		FString WidgetType;
		FString WidgetPath;
		bool bAlreadyLocalized = false;

		/** Localisation identity recovered from a keyed FText ("" when the text is source-only). */
		FString Namespace;
		FString Key;
	};

	// -----------------------------------------------------------------------
	// HUD widget shown while picker mode is active.
	// -----------------------------------------------------------------------
	class SPolyglotPickerOverlay : public SCompoundWidget
	{
	public:
		using FSaveCallback = TFunction<void(const FPolyglotEntry&)>;
		using FCancelCallback = TFunction<void()>;
		using FExitCallback = TFunction<void()>;

		SLATE_BEGIN_ARGS(SPolyglotPickerOverlay) {}
			SLATE_ARGUMENT(FSaveCallback, OnSaveEntry)
			SLATE_ARGUMENT(FCancelCallback, OnCancelEdit)
			SLATE_ARGUMENT(FExitCallback, OnExitPicker)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

		/** Called by the input processor on every hovered-text change (scan mode only). */
		void SetHoverInfo(const FPickedText& InPicked);

		/** Switch to the "pick a text" screen. */
		void ShowScanMode(const FText& InMessage = FText::GetEmpty());

		/** Switch to the translation form for the captured entry. */
		void ShowEditMode(const FPolyglotEntry& InEntry, const FText& InNote);

		bool IsEditMode() const { return bEditMode; }

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

		/** Leave picker mode (Esc / Stop button). Invokes OnExitPicker when set. */
		FReply HandleExitPicker();

	private:
		TSharedRef<SWidget> BuildScanPanel();
		TSharedRef<SWidget> BuildEditPanel(const FPolyglotEntry& InEntry, const FText& InNote);

		void HandleSaveEntry(const FPolyglotEntry& Entry);
		void HandleCancelEdit();

		TSharedPtr<SBox> RootBox;
		TSharedPtr<SPolyglotEntryDialog> EditDialog;
		TSharedPtr<STextBlock> MessageText;
		TSharedPtr<STextBlock> HoverSourceText;
		TSharedPtr<STextBlock> HoverPathText;
		TSharedPtr<STextBlock> HoverTypeText;

		FSaveCallback OnSaveEntry;
		FCancelCallback OnCancelEdit;
		FExitCallback OnExitPicker;

		bool bEditMode = false;
		bool bPendingFocus = false;
		FText PendingNote;
	};

	void SPolyglotPickerOverlay::Construct(const FArguments& InArgs)
	{
		OnSaveEntry = InArgs._OnSaveEntry;
		OnCancelEdit = InArgs._OnCancelEdit;
		OnExitPicker = InArgs._OnExitPicker;

		RootBox = SNew(SBox);
		RootBox->SetContent(BuildScanPanel());

		ChildSlot
			[
				SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
					.Padding(8.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
											.TextStyle(&HeaderTextStyle())
											.Text(PolyglotUI::UiText(TEXT("Easy language Text Picker"), TEXT("Easy language 文字拾取器")))
									]
								+ SHorizontalBox::Slot()
									.FillWidth(1.f)
									[ SNew(SSpacer) ]
								+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(SButton)
											.OnClicked(FOnClicked::CreateRaw(this, &SPolyglotPickerOverlay::HandleExitPicker))
											.ToolTipText(PolyglotUI::UiText(TEXT("Leave picker mode (Esc)."), TEXT("退出拾取模式（Esc）。")))
											[
												SNew(STextBlock)
													.TextStyle(&SmallTextStyle())
													.Text(PolyglotUI::UiText(TEXT("Stop"), TEXT("停止")))
											]
									]
							]
						+ SVerticalBox::Slot()
							.FillHeight(1.f)
							.Padding(FMargin(0.f, 6.f, 0.f, 0.f))
							[ RootBox.ToSharedRef() ]
					]
			];
	}

	TSharedRef<SWidget> SPolyglotPickerOverlay::BuildScanPanel()
	{
		MessageText = SNew(STextBlock)
			.TextStyle(&SmallTextStyle())
			.AutoWrapText(true);

		HoverSourceText = SNew(STextBlock)
			.TextStyle(&NormalTextStyle())
			.AutoWrapText(true);

		HoverTypeText = SNew(STextBlock)
			.TextStyle(&SmallTextStyle())
			.AutoWrapText(true);

		HoverPathText = SNew(STextBlock)
			.TextStyle(&SmallTextStyle())
			.ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f))
			.AutoWrapText(true);

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
				[
					SNew(STextBlock)
						.TextStyle(&NormalTextStyle())
						.Text(PolyglotUI::UiText(
							TEXT("Move the mouse over any editor text, then click it to translate it into your user dictionary."),
							TEXT("把鼠标移到任意界面文字上，再点击它，即可把翻译写入你的用户词典。")))
						.AutoWrapText(true)
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.f, 2.f, 0.f, 4.f))
				[ MessageText.ToSharedRef() ]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.f, 2.f, 0.f, 2.f))
				[
					SNew(STextBlock)
						.TextStyle(&SmallTextStyle())
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
						.Text(PolyglotUI::UiText(TEXT("Hovered text"), TEXT("悬停文本")))
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(2.f, 0.f, 0.f, 4.f))
				[
					SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
						.Padding(8.f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
								[ HoverSourceText.ToSharedRef() ]
						]
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(2.f, 2.f, 0.f, 2.f))
				[ HoverTypeText.ToSharedRef() ]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(2.f, 0.f, 0.f, 0.f))
				[ HoverPathText.ToSharedRef() ];
	}

	TSharedRef<SWidget> SPolyglotPickerOverlay::BuildEditPanel(const FPolyglotEntry& InEntry, const FText& InNote)
	{
		EditDialog = SNew(SPolyglotEntryDialog)
			.InitialEntry(InEntry)
			.NoteText(InNote)
			.bSourceReadOnly(false)
			.bCultureReadOnly(true)
			.OnSave(PolyglotUI::FPolyglotEntrySaveDelegate::CreateRaw(this, &SPolyglotPickerOverlay::HandleSaveEntry))
			.OnCancel(PolyglotUI::FPolyglotEntryCancelDelegate::CreateRaw(this, &SPolyglotPickerOverlay::HandleCancelEdit));

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[ EditDialog.ToSharedRef() ]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.f, 4.f, 0.f, 0.f))
				[
					SNew(STextBlock)
						.TextStyle(&SmallTextStyle())
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
						.Text(PolyglotUI::UiText(
							TEXT("Save writes the entry and returns to picking. Esc cancels."),
							TEXT("保存会写入词条并返回拾取。Esc 取消。")))
				];
	}

	void SPolyglotPickerOverlay::SetHoverInfo(const FPickedText& InPicked)
	{
		if (bEditMode)
		{
			return;
		}
		if (InPicked.bFound)
		{
			HoverSourceText->SetText(FText::FromString(InPicked.Source));
			HoverTypeText->SetText(FText::Format(PolyglotUI::UiText(TEXT("Widget: {0}"), TEXT("控件：{0}")), FText::FromString(InPicked.WidgetType)));
			HoverPathText->SetText(FText::Format(PolyglotUI::UiText(TEXT("Path: {0}"), TEXT("路径：{0}")), FText::FromString(InPicked.WidgetPath)));

			FText Message;
			if (InPicked.bAlreadyLocalized)
			{
				Message = PolyglotUI::UiText(
					TEXT("This text already displays in the active culture. Its English source was found in your dictionary and pre-filled. Click it if you still want to change the translation."),
					TEXT("该文字当前已显示为活动语言。其英文原文已从词典找回并预填。若想改翻译，点击它即可。"));
			}
			MessageText->SetText(Message);
		}
		else
		{
			HoverSourceText->SetText(FText::GetEmpty());
			HoverTypeText->SetText(FText::GetEmpty());
			HoverPathText->SetText(FText::GetEmpty());
			MessageText->SetText(FText::GetEmpty());
		}
	}

	void SPolyglotPickerOverlay::ShowScanMode(const FText& InMessage)
	{
		bEditMode = false;
		EditDialog.Reset();
		RootBox->SetContent(BuildScanPanel());
		if (!InMessage.IsEmpty())
		{
			MessageText->SetText(InMessage);
		}
	}

	void SPolyglotPickerOverlay::ShowEditMode(const FPolyglotEntry& InEntry, const FText& InNote)
	{
		bEditMode = true;
		bPendingFocus = true;
		PendingNote = InNote;
		RootBox->SetContent(BuildEditPanel(InEntry, InNote));
	}

	void SPolyglotPickerOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		if (bPendingFocus && EditDialog.IsValid())
		{
			bPendingFocus = false;
			EditDialog->FocusTranslationBox();
		}
	}

	void SPolyglotPickerOverlay::HandleSaveEntry(const FPolyglotEntry& Entry)
	{
		if (OnSaveEntry)
		{
			OnSaveEntry(Entry);
		}
		else
		{
			ShowScanMode();
		}
	}

	void SPolyglotPickerOverlay::HandleCancelEdit()
	{
		if (OnCancelEdit)
		{
			OnCancelEdit();
		}
		else
		{
			ShowScanMode();
		}
	}

	FReply SPolyglotPickerOverlay::HandleExitPicker()
	{
		if (OnExitPicker)
		{
			OnExitPicker();
		}
		return FReply::Handled();
	}

	// -----------------------------------------------------------------------
	// Input processor that drives the picking.
	// -----------------------------------------------------------------------
	class FPolyglotPickerInputProcessor : public IInputProcessor
	{
	public:
		FPolyglotPickerInputProcessor()
		{
		}

		void SetHudWindow(const TSharedPtr<SWindow>& InWindow) { HudWindow = InWindow; }
		void SetOverlay(const TWeakPtr<SPolyglotPickerOverlay>& InOverlay) { Overlay = InOverlay; }

		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
		{
			// Intentionally nothing: picking is event driven.
		}

		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
		{
			if (InKeyEvent.GetKey() == EKeys::Escape)
			{
				// Editing -> cancel back to scan mode; scan mode -> leave picker.
				const TSharedPtr<SPolyglotPickerOverlay> Pinned = Overlay.Pin();
				if (Pinned.IsValid())
				{
					if (Pinned->IsEditMode())
					{
						Pinned->ShowScanMode(PolyglotUI::UiText(TEXT("Edit cancelled. Click any text to pick it."), TEXT("已取消编辑。点击任意文字进行拾取。")));
						return true;
					}
					Pinned->HandleExitPicker();
					return true;
				}
			}
			return false;
		}

		virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
		{
			const TSharedPtr<SPolyglotPickerOverlay> Pinned = Overlay.Pin();
			if (!Pinned.IsValid() || Pinned->IsEditMode())
			{
				return false;
			}

			const double Now = FPlatformTime::Seconds();
			const FVector2D ScreenPos = FVector2D(MouseEvent.GetScreenSpacePosition());
			if (Now - LastProbeTime < ProbeIntervalSeconds)
			{
				return false;
			}
			if (LastProbePos.Equals(ScreenPos, 2.f))
			{
				return false;
			}
			LastProbeTime = Now;
			LastProbePos = ScreenPos;

			const FPickedText Picked = ProbeWidgetAt(ScreenPos);
			Pinned->SetHoverInfo(Picked);
			return false;
		}

		virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return false;
			}

			const FVector2D ScreenPos = FVector2D(MouseEvent.GetScreenSpacePosition());
			const TSharedPtr<SPolyglotPickerOverlay> Pinned = Overlay.Pin();
			const TSharedPtr<SWindow> Hud = HudWindow.Pin();

			// Never steal events aimed at the HUD itself (its buttons/boxes work normally).
			if (Hud.IsValid() && Hud->GetRectInScreen().ContainsPoint(ScreenPos))
			{
				return false;
			}

			if (!Pinned.IsValid())
			{
				return false;
			}

			// While the edit form is up the underlying editor must not react to clicks.
			if (Pinned->IsEditMode())
			{
				return true;
			}

			const FPickedText Picked = ProbeWidgetAt(ScreenPos);
			if (!Picked.bFound)
			{
				return false;
			}

			// Compose the entry that the edit form will start from.
			UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
			FPolyglotEntry Entry;
			if (Subsystem)
			{
				Entry.Culture = Subsystem->GetActiveCulture();
				if (Entry.Culture.IsEmpty())
				{
					Entry.Culture = Polyglot::Compat::Culture_ChineseSimplified;
				}
			}
			else
			{
				Entry.Culture = Polyglot::Compat::Culture_ChineseSimplified;
			}

			// Picking while the editor shows English would store an "en" entry
			// that the Chinese scan pass can never hit. Redirect it to the default
			// translation culture and tell the user.
			bool bCultureRedirected = false;
			if (Entry.Culture.StartsWith(TEXT("en")))
			{
				Entry.Culture = Polyglot::Compat::Culture_ChineseSimplified;
				bCultureRedirected = true;
			}

			Entry.Source = Picked.Source;
			if (!Picked.Key.IsEmpty())
			{
				// Engine-localised text: carrying namespace/key lets the override
				// layer translate it too, which survives widget refreshes and
				// editor restarts.
				Entry.Namespace = Picked.Namespace;
				Entry.Key = Picked.Key;
			}
			Entry.Plugin = TEXT("Picked"); // human hint; overwritten by the user when wanted

			FText Note;
			if (Picked.bAlreadyLocalized)
			{
				Note = PolyglotUI::UiText(
					TEXT("This text already displays in the active culture. Its English source was recovered from your dictionary. Adjust the translation and press Save."),
					TEXT("该文字当前已显示为活动语言。其英文原文已从词典找回。调整译文后点“保存”。"));
			}
			else if (bCultureRedirected)
			{
				Note = PolyglotUI::UiText(
					TEXT("The editor is in English right now, so this entry was saved for Simplified Chinese (zh-Hans). Switch the editor to 简体中文 afterwards to see the translation."),
					TEXT("当前界面是英文，该词条已自动写入“简体中文”。之后把界面切到简体中文即可看到译文。"));
			}
			else if (!Entry.Key.IsEmpty())
			{
				Note = FText::Format(
					PolyglotUI::UiText(
						TEXT("Engine-localised text detected (Key: {0}). The translation is applied through the localisation layer, so it survives widget refreshes and editor restarts."),
						TEXT("检测到引擎本地化文本（Key：{0}）。译文将经本地化层生效，刷新界面与重启编辑器后依然保留。")),
					FText::FromString(Entry.Key));
			}
			else if (IsCjkCulture(Entry.Culture) && ContainsEastAsianText(Entry.Source))
			{
				Note = PolyglotUI::UiText(
					TEXT("Warning: the source still contains CJK characters. For a clean dictionary entry, switch the editor to English first, then pick again. You can still edit the source below."),
					TEXT("警告：源文本仍含中日韩字符。为保证词条整洁，请先切回英文再拾取。你也可以在下方直接修改源文本。"));
			}
			else
			{
				Note = FText::Format(
					PolyglotUI::UiText(TEXT("Pick complete. Widget: {0}  Path: {1}"), TEXT("拾取完成。控件：{0}  路径：{1}")),
					FText::FromString(Picked.WidgetType), FText::FromString(Picked.WidgetPath));
			}

			Pinned->ShowEditMode(Entry, Note);
			return true; // consume: the click was a "pick", not an editor action
		}

		virtual const TCHAR* GetDebugName() const override
		{
			return TEXT("PolyglotPicker");
		}

	private:
		/** Find the top-most widget under ScreenPos whose text we can read. */
		FPickedText ProbeWidgetAt(const FVector2D& ScreenPos)
		{
			FPickedText Result;
			if (!FSlateApplication::IsInitialized())
			{
				return Result;
			}

			FSlateApplication& SlateApp = FSlateApplication::Get();
			TArray<TSharedRef<SWindow>> Windows;
			SlateApp.GetAllVisibleWindowsOrdered(Windows);

			const TSharedPtr<SWindow> Hud = HudWindow.Pin();
			if (Hud.IsValid())
			{
				// Never treat our own HUD as pickable content.
				Windows.RemoveAll([&Hud](const TSharedRef<SWindow>& Candidate)
				{
					return Candidate == Hud;
				});
			}

			const FWidgetPath Path = SlateApp.LocateWindowUnderMouse(ScreenPos, Windows, /*bIgnoreEnabledStatus=*/true, /*UserIndex=*/0);
			if (!Path.IsValid())
			{
				return Result;
			}

			// Walk from the leaf back up and take the first readable text.
			for (int32 j = Path.Widgets.Num() - 1; j >= 0; --j)
			{
				const TSharedRef<SWidget>& Widget = Path.Widgets[j].Widget;
				FText Text;
				if (!FPolyglotWidgetPatcher::GetWidgetText(Widget, Text))
				{
					continue;
				}
				FString Display = Text.ToString();
				if (Display.IsEmpty())
				{
					continue;
				}
				Display.TrimEndInline();
				if (Display.IsEmpty())
				{
					continue;
				}

				Result.bFound = true;
				Result.WidgetType = Widget->GetType().ToString();
				Result.Source = Display;
				Result.WidgetPath = BuildPathDescription(Path, j);
				ResolveLocalizedSource(Result);
				CaptureLocalizationIdentity(Text, Result);
				return Result;
			}
			return Result;
		}

		/** "Window > SOverlay > ... > STextBlock" style description of the path. */
		static FString BuildPathDescription(const FWidgetPath& Path, int32 LeafIndex)
		{
			TArray<FString> Names;
			const int32 Start = FMath::Max(0, LeafIndex - 6); // keep the tail only
			for (int32 i = Start; i <= LeafIndex; ++i)
			{
				Names.Add(Path.Widgets[i].Widget->GetType().ToString());
			}
			FString Out;
			for (int32 i = 0; i < Names.Num(); ++i)
			{
				if (i > 0)
				{
					Out += TEXT(" > ");
				}
				Out += Names[i];
			}
			return Out;
		}

		/**
		 * When the displayed text is already written in the active CJK culture,
		 * try to recover the original English source from the dictionary (a row
		 * that translates to exactly this string).
		 */
		void ResolveLocalizedSource(FPickedText& InOut)
		{
			UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
			FPolyglotDictionary* Dictionary = Subsystem ? Subsystem->GetDictionary() : nullptr;
			if (!Dictionary || Subsystem->GetActiveCulture().IsEmpty())
			{
				return;
			}
			if (!IsCjkCulture(Subsystem->GetActiveCulture()) || !ContainsEastAsianText(InOut.Source))
			{
				return;
			}

			TArray<FPolyglotEntry> AllEntries;
			Dictionary->GetEntries(AllEntries, /*bUserOnly=*/false);
			for (const FPolyglotEntry& Entry : AllEntries)
			{
				if (Entry.Culture.Equals(Subsystem->GetActiveCulture())
					&& Entry.Translation.Equals(InOut.Source))
				{
					if (!Entry.Source.IsEmpty())
					{
						InOut.Source = Entry.Source;
						InOut.bAlreadyLocalized = true;
					}
					return;
				}
			}
		}

		/**
		 * When the picked text is keyed localised text (LOCTEXT / NSLOCTEXT /
		 * FText with an identity), recover namespace + key so the saved entry can
		 * drive the engine localisation override layer as well. Widgets translated
		 * that way keep their translation across widget refreshes and restarts.
		 * Text that is displayed in CJK carries no useful identity for us - we
		 * only translate the visible English leftovers.
		 */
		void CaptureLocalizationIdentity(const FText& Text, FPickedText& InOut)
		{
			if (ContainsEastAsianText(InOut.Source))
			{
				return;
			}

			const TOptional<FString> Namespace = FTextInspector::GetNamespace(Text);
			const TOptional<FString> Key = FTextInspector::GetKey(Text);
			if (Key.IsSet() && !Key.GetValue().IsEmpty())
			{
				InOut.Namespace = Namespace.IsSet() ? Namespace.GetValue() : FString();
				InOut.Key = Key.GetValue();
			}
		}

		TWeakPtr<SWindow> HudWindow;
		TWeakPtr<SPolyglotPickerOverlay> Overlay;
		double LastProbeTime = 0.0;
		FVector2D LastProbePos = FVector2D::ZeroVector;
		static constexpr double ProbeIntervalSeconds = 0.04;
	};

	// -----------------------------------------------------------------------
	// Picker mode controller (file-local singleton).
	// -----------------------------------------------------------------------
	class FPolyglotPickerMode
	{
	public:
		static FPolyglotPickerMode& Get()
		{
			static FPolyglotPickerMode Instance;
			return Instance;
		}

		bool IsActive() const
		{
			return InputProcessor.IsValid();
		}

		void Enter()
		{
			if (IsActive())
			{
				return;
			}
			if (!FSlateApplication::IsInitialized())
			{
				return;
			}

			// A pending exit (scheduled one tick ago) is now obsolete.
			bExitScheduled = false;

			FSlateApplication& SlateApp = FSlateApplication::Get();

			TSharedRef<SPolyglotPickerOverlay> Overlay = SNew(SPolyglotPickerOverlay)
				.OnSaveEntry(TFunction<void(const FPolyglotEntry&)>([this](const FPolyglotEntry& Entry) { HandleEntrySaved(Entry); }))
				.OnCancelEdit(TFunction<void()>([this]()
				{
					const TSharedPtr<SPolyglotPickerOverlay> Pinned = OverlayWeak.Pin();
					if (Pinned.IsValid())
					{
						Pinned->ShowScanMode(PolyglotUI::UiText(TEXT("Edit cancelled. Click any text to pick it."), TEXT("已取消编辑。点击任意文字进行拾取。")));
					}
				}))
				.OnExitPicker(TFunction<void()>([this]() { Exit(); }));
			OverlayWeak = Overlay;

			TSharedRef<SWindow> NewWindow = SNew(SWindow)
				.Title(PolyglotUI::UiText(TEXT("Easy language Text Picker"), TEXT("Easy language 文字拾取器")))
				.ClientSize(FVector2D(560.f, 360.f))
				.SizingRule(ESizingRule::UserSized)
				.SupportsMaximize(false)
				.SupportsMinimize(false)
				.HasCloseButton(false) // leave through Stop / Esc so the input processor is always cleaned up
				.CreateTitleBar(true);
			NewWindow->SetContent(Overlay);
			SlateApp.AddWindow(NewWindow);

			// Place near the top-right of the display that currently holds the cursor.
			const FVector2D CursorPos = SlateApp.GetCursorPos();
			const FSlateRect WorkArea = SlateApp.GetWorkArea(FSlateRect(CursorPos.X, CursorPos.Y, CursorPos.X + 1.f, CursorPos.Y + 1.f));
			const FVector2D RequestedSize(560.f, 360.f);
			NewWindow->MoveWindowTo(FVector2D(
				FMath::Max(WorkArea.Left, WorkArea.Right - RequestedSize.X - 12.f),
				WorkArea.Top + 24.f));

			WindowWeak = NewWindow;

			InputProcessor = MakeShared<FPolyglotPickerInputProcessor>();
			InputProcessor->SetHudWindow(NewWindow);
			InputProcessor->SetOverlay(Overlay);
			SlateApp.RegisterInputPreProcessor(InputProcessor);

			UE_LOG(LogPolyglot, Display, TEXT("Polyglot picker mode entered. Click any text to translate it; Esc to leave."));
		}

		void Exit()
		{
			if (!IsActive() || bExitScheduled)
			{
				return;
			}
			bExitScheduled = true;

			// Input processors can be entered from inside Slate's own processor
			// dispatch (Esc hotkey), so unregister on the next core tick rather
			// than synchronously - this keeps the processor list valid while the
			// current event is still being processed.
			if (FSlateApplication::IsInitialized())
			{
				FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateLambda([this](float /*DeltaTime*/)
					{
						const bool bPending = bExitScheduled;
						bExitScheduled = false;
						if (bPending)
						{
							DoExit();
						}
						return false; // one-shot
					}),
					0.05f);
			}
			else
			{
				bExitScheduled = false;
			}
		}

	private:
		void DoExit()
		{
			if (!IsActive())
			{
				return;
			}
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
			}
			InputProcessor.Reset();

			const TSharedPtr<SWindow> Window = WindowWeak.Pin();
			if (Window.IsValid() && FSlateApplication::IsInitialized())
			{
				Window->RequestDestroyWindow();
			}
			WindowWeak.Reset();
			OverlayWeak.Reset();

			UE_LOG(LogPolyglot, Display, TEXT("Polyglot picker mode exited."));
		}

	public:
		void HandleEntrySaved(const FPolyglotEntry& Entry)
		{
			UPolyglotSubsystem* Subsystem = PolyglotUI::GetSubsystem();
			if (!Subsystem)
			{
				ShowScanMessage(PolyglotUI::UiText(
					TEXT("Easy language subsystem is not available right now."),
					TEXT("Easy language 子系统当前不可用。")));
				return;
			}

			if (Subsystem->AddOrUpdateUserEntry(Entry))
			{
				if (!Entry.Key.IsEmpty())
				{
					ShowScanMessage(PolyglotUI::UiText(
						TEXT("Saved. This text runs through the engine localisation layer, so the translation now applies system-wide and survives refreshes."),
						TEXT("已保存。该文本走引擎本地化层，译文已全局生效并在刷新后保留。")));
				}
				else
				{
					ShowScanMessage(PolyglotUI::UiText(
						TEXT("Saved. The translation should now be visible on screen."),
						TEXT("已保存。译文现在应已显示在界面上。")));
				}
			}
			else
			{
				ShowScanMessage(PolyglotUI::UiText(
					TEXT("Could not save the entry (empty source/key/translation?)."),
					TEXT("无法保存词条（源文本/Key/译文是否为空？）。")));
			}
		}

	private:
		void ShowScanMessage(const FText& InMessage)
		{
			const TSharedPtr<SPolyglotPickerOverlay> Pinned = OverlayWeak.Pin();
			if (Pinned.IsValid())
			{
				Pinned->ShowScanMode(InMessage);
			}
		}

		TSharedPtr<FPolyglotPickerInputProcessor> InputProcessor;
		TWeakPtr<SWindow> WindowWeak;
		TWeakPtr<SPolyglotPickerOverlay> OverlayWeak;
		bool bExitScheduled = false;
	};
}

namespace PolyglotUI
{
	void TogglePickerMode()
	{
		FPolyglotPickerMode& Mode = FPolyglotPickerMode::Get();
		if (Mode.IsActive())
		{
			Mode.Exit();
		}
		else
		{
			Mode.Enter();
		}
	}
}

#undef LOCTEXT_NAMESPACE
