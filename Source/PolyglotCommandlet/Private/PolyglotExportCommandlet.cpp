// Copyright 2026 BaiMaoMaLou. All Rights Reserved.
//
// UPolyglotExportCommandlet
//
// Headless "untranslated scan" exporter. Walks every compiled .locres pair that
// has an "en" culture and a sibling Chinese culture ("zh-Hans" / "zh" / "zh-CN")
// across:
//   - Engine/Content/Localization  (Editor, Engine, ... targets)
//   - every engine plugin's Content/Localization
//   - this project's Plugins/*/Content/Localization (third-party dictionaries)
//
// For every source entry that exists in the "en" file but is NOT covered by any
// Chinese culture file it writes one CSV row (namespace, key, source, empty
// translation). The translator fills the translation column and re-imports the
// file with the dictionary editor / CSV import.
//
// Usage (project with the plugin installed):
//   UnrealEditor-Cmd.exe <YourProject>.uproject -run=PolyglotExport
//       -Output="<abs path to out.csv>" -unattended -nop4 -nosplash
// Default output: <Project>/Saved/Polyglot/untranslated.csv

#include "PolyglotExportCommandlet.h"

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PolyglotCsv.h"
#include "PolyglotFileUtil.h"
#include "PolyglotLog.h"
#include "PolyglotTypes.h"

namespace
{
	const TCHAR* const DefaultCulture = TEXT("zh-Hans");

	/** Returns true when the culture folder name is a Chinese one. */
	bool IsChineseCulture(const FString& CultureName)
	{
		return CultureName == TEXT("zh") || CultureName == TEXT("zh-Hans")
			|| CultureName == TEXT("zh-CN") || CultureName == TEXT("zh-Hant");
	}

	/** Folder name of the file itself (the culture segment in a LocRes path). */
	FString CultureFromPath(const FString& FilePath)
	{
		FString ParentDir = FPaths::GetPath(FilePath);
		return FPaths::GetCleanFilename(ParentDir);
	}
}

UPolyglotExportCommandlet::UPolyglotExportCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;

	HelpDescription = TEXT("Polyglot untranslated-text scanner: diffs English vs Chinese .locres data and writes a CSV of every entry that still needs a translation.");
	HelpUsage = TEXT("-Output=<path> (optional, default: <Project>/Saved/Polyglot/untranslated.csv)");
}

int32 UPolyglotExportCommandlet::Main(const FString& Params)
{
	FString OutputPath;
	FParse::Value(*Params, TEXT("Output="), OutputPath);
	if (OutputPath.IsEmpty())
	{
		OutputPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Polyglot"), TEXT("untranslated.csv"));
	}

	// ------------------------------------------------------------------
	// 1. Collect every *.locres under the supported roots.
	// ------------------------------------------------------------------
	TArray<FString> Roots;
	Roots.AddUnique(FPaths::Combine(FPaths::EngineContentDir(), TEXT("Localization")));
	Roots.AddUnique(FPaths::EnginePluginsDir());
	if (FPaths::IsProjectFilePathSet())
	{
		Roots.AddUnique(FPaths::ProjectPluginsDir());
	}

	TArray<FString> AllLocResFiles;
	IFileManager& FileManager = IFileManager::Get();
	for (const FString& Root : Roots)
	{
		if (!FPaths::DirectoryExists(Root))
		{
			continue;
		}
		TArray<FString> Found;
		FileManager.FindFilesRecursive(Found, *Root, TEXT("*.locres"), /*bFiles=*/true, /*bDirectories=*/false, /*bClearFileNames=*/false);
		UE_LOG(LogPolyglot, Display, TEXT("Scanning %s (%d locres file(s))"), *Root, Found.Num());
		AllLocResFiles.Append(Found);
	}

	if (AllLocResFiles.Num() == 0)
	{
		UE_LOG(LogPolyglot, Error, TEXT("No .locres files found - cannot run the untranslated scan."));
		return 1;
	}

	// ------------------------------------------------------------------
	// 2. Group by "target base directory" so the cultures of one target
	//    are diffed against each other.
	// ------------------------------------------------------------------
	struct FTargetCultures
	{
		FString Base;
		TArray<FString> EnFiles;
		TArray<FString> ZhFiles;
	};
	TMap<FString, TSharedRef<FTargetCultures>> TargetMap;

	for (const FString& FilePath : AllLocResFiles)
	{
		const FString CultureDir = CultureFromPath(FilePath);
		const FString Base = FPaths::GetPath(FPaths::GetPath(FilePath)); // <...>/Target

		TSharedRef<FTargetCultures>& Target = TargetMap.FindOrAdd(Base);
		Target->Base = Base;

		if (CultureDir == TEXT("en"))
		{
			Target->EnFiles.Add(FilePath);
		}
		else if (IsChineseCulture(CultureDir))
		{
			Target->ZhFiles.Add(FilePath);
		}
	}

	// ------------------------------------------------------------------
	// 3. Load English sources + the already translated Chinese keys.
	// ------------------------------------------------------------------
	struct FSourceRow
	{
		FString Namespace;
		FString Key;
		FString Source;
	};
	TMap<FTextId, FSourceRow> Sources; // every known (ns,key) from "en"
	TSet<FTextId> ChineseCovered;      // every (ns,key) that already has zh data

	auto CollectEntries = [](const FString& LocResPath, TMap<FTextId, FSourceRow>& OutSources)
	{
		FTextLocalizationResource Resource;
		if (!Resource.LoadFromFile(LocResPath, /*Priority=*/0))
		{
			UE_LOG(LogPolyglot, Warning, TEXT("Could not load LocRes file: %s"), *LocResPath);
			return 0;
		}
		int32 Count = 0;
		for (const TPair<FTextId, FTextLocalizationResource::FEntry>& Pair : Resource.Entries)
		{
			const FTextId& Id = Pair.Key;
			const FTextLocalizationResource::FEntry& Entry = Pair.Value;
			if (!Entry.LocalizedString.IsValid())
			{
				continue;
			}
			FSourceRow Row;
			Row.Namespace = Id.GetNamespace().ToString();
			Row.Key = Id.GetKey().ToString();
			Row.Source = *Entry.LocalizedString;
			if (Row.Source.IsEmpty())
			{
				continue;
			}
			OutSources.Add(Id, MoveTemp(Row));
			++Count;
		}
		return Count;
	};

	auto CollectKeys = [](const FString& LocResPath, TSet<FTextId>& OutCovered)
	{
		FTextLocalizationResource Resource;
		if (!Resource.LoadFromFile(LocResPath, /*Priority=*/0))
		{
			UE_LOG(LogPolyglot, Warning, TEXT("Could not load LocRes file: %s"), *LocResPath);
			return 0;
		}
		int32 Count = 0;
		for (const TPair<FTextId, FTextLocalizationResource::FEntry>& Pair : Resource.Entries)
		{
			OutCovered.Add(Pair.Key);
			++Count;
		}
		return Count;
	};

	int32 TotalEn = 0;
	int32 TotalZh = 0;

	for (const TPair<FString, TSharedRef<FTargetCultures>>& Pair : TargetMap)
	{
		const FTargetCultures& Target = Pair.Value.Get();
		for (const FString& EnFile : Target.EnFiles)
		{
			TotalEn += CollectEntries(EnFile, Sources);
		}
		for (const FString& ZhFile : Target.ZhFiles)
		{
			TotalZh += CollectKeys(ZhFile, ChineseCovered);
		}
	}

	UE_LOG(LogPolyglot, Display, TEXT("Parsed %d English entries and %d Chinese entries across %d target(s)."),
		TotalEn, TotalZh, TargetMap.Num());

	// ------------------------------------------------------------------
	// 4. Build the "missing translation" rows.
	// ------------------------------------------------------------------
	TArray<FPolyglotEntry> Missing;
	for (const TPair<FTextId, FSourceRow>& Pair : Sources)
	{
		if (ChineseCovered.Contains(Pair.Key))
		{
			continue;
		}
		FPolyglotEntry Entry;
		Entry.Namespace = Pair.Value.Namespace;
		Entry.Key = Pair.Value.Key;
		Entry.Source = Pair.Value.Source;
		Entry.Culture = DefaultCulture;
		Entry.Plugin = TEXT("untitled");
		Missing.Add(MoveTemp(Entry));
	}

	// Stable, review-friendly order.
	Missing.Sort([](const FPolyglotEntry& A, const FPolyglotEntry& B)
	{
		if (A.Namespace != B.Namespace)
		{
			return A.Namespace < B.Namespace;
		}
		return A.Key < B.Key;
	});

	// ------------------------------------------------------------------
	// 5. Write the CSV.
	// ------------------------------------------------------------------
	const FString CsvText = PolyglotCsv::WriteEntries(Missing);
	if (!PolyglotFileUtil::WriteUtf8StringToFile(OutputPath, CsvText))
	{
		UE_LOG(LogPolyglot, Error, TEXT("Could not write CSV to %s"), *OutputPath);
		return 1;
	}

	UE_LOG(LogPolyglot, Display, TEXT("Untranslated scan done: %d rows written to %s"),
		Missing.Num(), *OutputPath);
	UE_LOG(LogPolyglot, Display, TEXT("Fill the Translation column in the CSV, then use Polyglot > Dictionary Editor > Import CSV to feed it back."));
	return 0;
}
