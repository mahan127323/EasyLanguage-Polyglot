// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "PolyglotSettings.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#include "PolyglotFileUtil.h"
#include "PolyglotLog.h"

namespace PolyglotSettings
{
	static const FString RootDirName = TEXT("Polyglot");
	static const TCHAR* SettingsFileName = TEXT("settings.json");
	static const TCHAR* UserDictionaryFileName = TEXT("user_dictionary.json");

	static void EnsureDirectory(const FString& DirectoryPath)
	{
		if (!IFileManager::Get().DirectoryExists(*DirectoryPath))
		{
			IFileManager::Get().MakeDirectory(*DirectoryPath, /*Tree=*/true);
		}
	}

	FString GetProjectPolyglotDir()
	{
		return FPaths::Combine(FPaths::ProjectConfigDir(), RootDirName);
	}

	FString GetSettingsPath()
	{
		return FPaths::Combine(GetProjectPolyglotDir(), SettingsFileName);
	}

	FString GetUserDictionaryPath()
	{
		return FPaths::Combine(GetProjectPolyglotDir(), UserDictionaryFileName);
	}

	FString GetUserDictionariesDir()
	{
		return GetProjectPolyglotDir();
	}

	FString GetBuiltinDictionaryDir()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Polyglot"));
		if (Plugin.IsValid())
		{
			return FPaths::Combine(Plugin->GetContentDir(), TEXT("Dictionaries"));
		}

		// Fallback for very early startup: try the module location relative path.
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("Polyglot"), TEXT("Content"), TEXT("Dictionaries"));
	}

	FString GetSavedPolyglotDir()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), RootDirName);
	}

	FPolyglotState LoadState()
	{
		FPolyglotState State;
		State.Reset();

		TSharedPtr<FJsonObject> Root;
		const FString SettingsPath = GetSettingsPath();
		if (!IFileManager::Get().FileExists(*SettingsPath))
		{
			return State;
		}

		if (!PolyglotFileUtil::LoadJsonFromFile(SettingsPath, Root))
		{
			UE_LOG(LogPolyglot, Warning, TEXT("Polyglot settings file exists but could not be parsed: %s"), *SettingsPath);
			return State;
		}

		State.bEnabled = Root->GetBoolField(TEXT("enabled"));
		Root->TryGetStringField(TEXT("culture"), State.Culture);
		State.ScanIntervalSeconds = static_cast<float>(Root->GetNumberField(TEXT("scan_interval_seconds")));
		if (State.ScanIntervalSeconds <= 0.0f)
		{
			State.ScanIntervalSeconds = 0.5f;
		}
		State.ScanTimeBudgetMs = static_cast<float>(Root->GetNumberField(TEXT("scan_time_budget_ms")));
		if (State.ScanTimeBudgetMs <= 0.0f)
		{
			State.ScanTimeBudgetMs = 2.0f;
		}
		Root->TryGetBoolField(TEXT("translate_static_text"), State.bTranslateStaticTextBlocks);
		Root->TryGetBoolField(TEXT("translate_editable_text"), State.bTranslateEditableText);

		return State;
	}

	void SaveState(const FPolyglotState& InState)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetBoolField(TEXT("enabled"), InState.bEnabled);
		Root->SetStringField(TEXT("culture"), InState.Culture);
		Root->SetNumberField(TEXT("scan_interval_seconds"), InState.ScanIntervalSeconds);
		Root->SetNumberField(TEXT("scan_time_budget_ms"), InState.ScanTimeBudgetMs);
		Root->SetBoolField(TEXT("translate_static_text"), InState.bTranslateStaticTextBlocks);
		Root->SetBoolField(TEXT("translate_editable_text"), InState.bTranslateEditableText);

		EnsureDirectory(GetProjectPolyglotDir());
		if (!PolyglotFileUtil::SaveJsonToFile(GetSettingsPath(), Root))
		{
			UE_LOG(LogPolyglot, Warning, TEXT("Failed to write Polyglot settings to %s"), *GetSettingsPath());
		}
	}
}
