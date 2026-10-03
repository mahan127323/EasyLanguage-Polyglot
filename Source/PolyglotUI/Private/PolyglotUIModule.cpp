// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "PolyglotUIInternal.h"

#include "Brushes/SlateDynamicImageBrush.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Internationalization/Text.h"
#include "Framework/Commands/UIAction.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "PolyglotCompat.h"
#include "PolyglotLog.h"
#include "PolyglotSubsystem.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Textures/SlateIcon.h"
#include "ToolMenuDelegates.h"
#include "ToolMenuEntry.h"
#include "ToolMenuMisc.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogPolyglotUI, Log, All);

#define LOCTEXT_NAMESPACE "PolyglotUIModule"

namespace PolyglotUI
{
	UPolyglotSubsystem* GetSubsystem()
	{
#if WITH_EDITOR
		if (GEditor)
		{
			return UPolyglotSubsystem::Get();
		}
#endif
		return nullptr;
	}

	bool IsChineseUi()
	{
		if (UPolyglotSubsystem* Subsystem = GetSubsystem())
		{
			return Subsystem->GetEngineLanguage().StartsWith(TEXT("zh"));
		}
		return false;
	}

	FText UiText(const TCHAR* English, const TCHAR* SimplifiedChinese)
	{
		return FText::FromString(IsChineseUi() ? SimplifiedChinese : English);
	}

	TSharedPtr<SWindow> GetParentWindow()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return nullptr;
		}
		const TArray<TSharedRef<SWindow>> Windows = FSlateApplication::Get().GetInteractiveTopLevelWindows();
		return Windows.Num() > 0 ? TSharedPtr<SWindow>(Windows[0]) : TSharedPtr<SWindow>();
	}

	void Notify(const FText& Message)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}
		FNotificationInfo Info(Message);
		Info.bFireAndForget = true;
		Info.ExpireDuration = 4.0f;
		Info.FadeOutDuration = 0.5f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	void ToggleLanguage()
	{
		UPolyglotSubsystem* Subsystem = GetSubsystem();
		if (!Subsystem)
		{
			return;
		}
		if (!Subsystem->IsEnabled())
		{
			Subsystem->SetEnabled(true);
		}
		const FString Current = Subsystem->GetActiveCulture();
		const FString Target = Current.StartsWith(TEXT("zh")) ? Polyglot::Compat::Culture_English
			: Polyglot::Compat::Culture_ChineseSimplified;
		if (Subsystem->RequestCulture(Target))
		{
			Notify(FText::Format(
				UiText(
					TEXT("Editor language switched to {0}."),
					TEXT("编辑器语言已切换到 {0}。")),
				FText::FromString(Target)));
		}
		else
		{
			Notify(UiText(
				TEXT("Culture is not available in this engine."),
				TEXT("当前引擎不支持该语言。")));
		}
	}
}

// ---------------------------------------------------------------------------
// Keyboard shortcuts (Ctrl+Alt+L toggle, Ctrl+Alt+P picker).
// A lightweight IInputProcessor avoids the FUICommandInfo ceremony while still
// working in every editor context (including popups and full-screen modes).
// ---------------------------------------------------------------------------
namespace
{
	// -----------------------------------------------------------------------
	// Easy language icon: loaded at runtime from Resources/Icon128.png, decoded
	// to BGRA and registered as a dynamic Slate image. Used by the uplugin,
	// the editor toolbar button and the Tools menu commands.
	// -----------------------------------------------------------------------
	const FName EasyLanguageStyleSetName(TEXT("EasyLanguageStyle"));
	const FName EasyLanguageIconStyleName(TEXT("EasyLanguage.Icon"));
	TSharedPtr<FSlateStyleSet> EasyLanguageStyleSet;

	void RegisterEasyLanguageIconStyle()
	{
		if (EasyLanguageStyleSet.IsValid())
		{
			return;
		}

		EasyLanguageStyleSet = MakeShared<FSlateStyleSet>(EasyLanguageStyleSetName);

		if (FSlateApplication::IsInitialized())
		{
			FString IconPath;
			if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Polyglot")))
			{
				IconPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("Icon128.png"));
			}

			TArray<uint8> FileData;
			if (!IconPath.IsEmpty() && FFileHelper::LoadFileToArray(FileData, *IconPath))
			{
				IImageWrapperModule& ImageWrapperModule =
					FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
				TSharedPtr<IImageWrapper> PngWrapper =
					ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

				if (PngWrapper.IsValid()
					&& PngWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
				{
					const int32 ImgWidth = PngWrapper->GetWidth();
					const int32 ImgHeight = PngWrapper->GetHeight();
					if (ImgWidth > 0 && ImgHeight > 0)
					{
						// Ask for BGRA directly: the PNG decoder maps it to
						// png_set_bgr + alpha expansion, which is fully supported.
						TArray64<uint8> Raw;
						if (PngWrapper->GetRaw(ERGBFormat::BGRA, 8, Raw) && Raw.Num() > 0)
						{
							TArray<uint8> BgraBytes;
							BgraBytes.SetNumUninitialized((int32)Raw.Num());
							FMemory::Memcpy(BgraBytes.GetData(), Raw.GetData(), Raw.Num());
							if (FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(
								EasyLanguageIconStyleName, (uint32)ImgWidth, (uint32)ImgHeight, BgraBytes))
							{
								EasyLanguageStyleSet->Set(
									EasyLanguageIconStyleName,
									new FSlateDynamicImageBrush(EasyLanguageIconStyleName, FVector2f(20.0f, 20.0f)));
							}
						}
					}
				}
			}
		}

		FSlateStyleRegistry::RegisterSlateStyle(*EasyLanguageStyleSet);
	}

	void UnregisterEasyLanguageIconStyle()
	{
		if (EasyLanguageStyleSet.IsValid())
		{
			FSlateStyleRegistry::UnRegisterSlateStyle(EasyLanguageStyleSetName);
			EasyLanguageStyleSet.Reset();
		}
	}

	FSlateIcon MakeEasyLanguageIcon()
	{
		if (EasyLanguageStyleSet.IsValid())
		{
			return FSlateIcon(EasyLanguageStyleSetName, EasyLanguageIconStyleName);
		}
		return FSlateIcon();
	}

	class FPolyglotHotkeyProcessor : public IInputProcessor
	{
	public:
		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
		{
			if (InKeyEvent.GetKey() == EKeys::L && InKeyEvent.IsControlDown() && InKeyEvent.IsAltDown())
			{
				PolyglotUI::ToggleLanguage();
				return true;
			}
			if (InKeyEvent.GetKey() == EKeys::P && InKeyEvent.IsControlDown() && InKeyEvent.IsAltDown())
			{
				PolyglotUI::TogglePickerMode();
				return true;
			}
			return false;
		}

		virtual const TCHAR* GetDebugName() const override
		{
			return TEXT("PolyglotHotkeys");
		}
	};

	const FName PolyglotOwnerName(TEXT("PolyglotUI"));

	void RegisterPolyglotMenus()
	{
		UToolMenus* ToolMenus = UToolMenus::Get();
		if (!ToolMenus)
		{
			return;
		}

		FToolMenuOwnerScoped OwnerScope(PolyglotOwnerName);

		// Tools menu: 4 Easy language commands sharing the plugin icon.
		if (UToolMenu* ToolsMenu = ToolMenus->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")))
		{
			FToolMenuSection& Section = ToolsMenu->AddSection(TEXT("EasyLanguage"), LOCTEXT("EasyLanguageSection", "Easy language"));

			// NOTE: this engine version's AddMenuEntry expects
			// (Name, Label, ToolTip, Icon, Action, ...).
			auto AddCommand = [&Section](const FName& Name, const FText& Label, const FText& Tooltip, void (*Function)())
			{
				Section.AddMenuEntry(
					Name,
					Label,
					Tooltip,
					MakeEasyLanguageIcon(),
					FToolUIActionChoice(FExecuteAction::CreateStatic(Function)));
			};

			AddCommand(
				TEXT("PolyglotToggle"),
				LOCTEXT("EasyLanguageToggleLabel", "Easy language: Toggle Chinese / English"),
				LOCTEXT("EasyLanguageToggleTooltip", "Enable Easy language and flip between Simplified Chinese and English without restarting (Ctrl+Alt+L)."),
				&PolyglotUI::ToggleLanguage);

			AddCommand(
				TEXT("PolyglotPanel"),
				LOCTEXT("EasyLanguagePanelLabel", "Easy language Panel..."),
				LOCTEXT("EasyLanguagePanelTooltip", "Open the Easy language control panel (language, dictionary, settings)."),
				&PolyglotUI::OpenMainPanel);

			AddCommand(
				TEXT("PolyglotDictionary"),
				LOCTEXT("EasyLanguageDictionaryLabel", "Open Dictionary Editor..."),
				LOCTEXT("EasyLanguageDictionaryTooltip", "Browse and edit dictionary entries (built-in read-only, user entries editable)."),
				&PolyglotUI::OpenDictionaryEditorWindow);

			AddCommand(
				TEXT("PolyglotPicker"),
				LOCTEXT("EasyLanguagePickerLabel", "Pick Text To Translate..."),
				LOCTEXT("EasyLanguagePickerTooltip", "Click any editor text to add its translation to your user dictionary (Ctrl+Alt+P)."),
				&PolyglotUI::TogglePickerMode);
		}

		// Editor main toolbar: a single icon button that opens the panel.
		if (UToolMenu* AssetsToolBar = ToolMenus->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.AssetsToolBar")))
		{
			FToolMenuSection& ToolbarSection = AssetsToolBar->AddSection(
				TEXT("EasyLanguageToolbar"),
				LOCTEXT("EasyLanguageToolbarSectionLabel", "Easy language"),
				FToolMenuInsert(TEXT("Content"), EToolMenuInsertType::After));

			ToolbarSection.AddEntry(FToolMenuEntry::InitToolBarButton(
				TEXT("PolyglotToolbarPanel"),
				FToolUIActionChoice(FExecuteAction::CreateStatic(&PolyglotUI::OpenMainPanel)),
				LOCTEXT("EasyLanguageToolbarLabel", "Easy language"),
				LOCTEXT("EasyLanguageToolbarTooltip", "Open the Easy language control panel: switch the editor language on the fly or translate third-party plugin text."),
				MakeEasyLanguageIcon()));
		}
	}

	class FPolyglotUIModule : public IModuleInterface
	{
	public:
		virtual void StartupModule() override
		{
			RegisterEasyLanguageIconStyle();

			// The editor may still be starting its UI when this module loads.
			// Registering through the ToolMenus startup callback guarantees the
			// LevelEditor main menu exists.
			StartupCallbackHandle = UToolMenus::RegisterStartupCallback(
				FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterPolyglotMenus));

			if (FSlateApplication::IsInitialized())
			{
				Hotkeys = MakeShared<FPolyglotHotkeyProcessor>();
				FSlateApplication::Get().RegisterInputPreProcessor(Hotkeys);
			}

			UE_LOG(LogPolyglotUI, Log, TEXT("PolyglotUI started. Ctrl+Alt+L = language toggle, Ctrl+Alt+P = picker."));
		}

		virtual void ShutdownModule() override
		{
			if (FSlateApplication::IsInitialized() && Hotkeys.IsValid())
			{
				FSlateApplication::Get().UnregisterInputPreProcessor(Hotkeys);
				Hotkeys.Reset();
			}

			if (StartupCallbackHandle.IsValid() && UToolMenus::IsToolMenuUIEnabled())
			{
				UToolMenus::UnRegisterStartupCallback(StartupCallbackHandle);
				StartupCallbackHandle.Reset();
				UToolMenus::Get()->UnregisterOwner(PolyglotOwnerName);
			}

			UnregisterEasyLanguageIconStyle();
		}

	private:
		TSharedPtr<FPolyglotHotkeyProcessor> Hotkeys;
		FDelegateHandle StartupCallbackHandle;
	};
}

IMPLEMENT_MODULE(FPolyglotUIModule, PolyglotUI)

#undef LOCTEXT_NAMESPACE
