// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "PolyglotDictionary.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#include "PolyglotCompat.h"
#include "PolyglotFileUtil.h"
#include "PolyglotLog.h"
#include "PolyglotSettings.h"

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void FPolyglotDictionary::ReloadAll()
{
	RawEntries.Reset();
	CultureSourceIndex.Reset();
	CultureLocalizedIndex.Reset();

	LoadBuiltins();
	LoadUserDirectory();
	LoadUserMainFile();
	RebuildIndexes();

	UE_LOG(LogPolyglot, Display, TEXT("Polyglot dictionaries loaded: %d entries total."), RawEntries.Num());
}

void FPolyglotDictionary::ReloadUserFile()
{
	// Rebuild the user part only: strip every user entry and reload.
	for (int32 i = RawEntries.Num() - 1; i >= 0; --i)
	{
		if (RawEntries[i].bUser)
		{
			RawEntries.RemoveAt(i);
		}
	}
	LoadUserDirectory();
	LoadUserMainFile();
	RebuildIndexes();

	UE_LOG(LogPolyglot, Display, TEXT("Polyglot user dictionaries reloaded: %d entries total."), RawEntries.Num());
}

bool FPolyglotDictionary::TranslateSource(const FString& Culture, const FString& SourceText, FString& OutTranslated) const
{
	if (SourceText.IsEmpty())
	{
		return false;
	}

	if (const TMap<FString, FString>* CultureMap = CultureSourceIndex.Find(Culture))
	{
		if (const FString* Translation = CultureMap->Find(SourceText))
		{
			if (!Translation->IsEmpty())
			{
				OutTranslated = *Translation;
				return true;
			}
		}
	}
	return false;
}

bool FPolyglotDictionary::TranslateLocalized(const FString& Culture, const FString& Namespace, const FString& Key, FString& OutTranslated) const
{
	if (Key.IsEmpty())
	{
		return false;
	}

	const FString Token = MakeLocalizedToken(Namespace, Key);
	if (const TMap<FString, FString>* CultureMap = CultureLocalizedIndex.Find(Culture))
	{
		if (const FString* Translation = CultureMap->Find(Token))
		{
			if (!Translation->IsEmpty())
			{
				OutTranslated = *Translation;
				return true;
			}
		}
	}
	return false;
}

bool FPolyglotDictionary::HasSourceEntry(const FString& Culture, const FString& SourceText) const
{
	if (const TMap<FString, FString>* CultureMap = CultureSourceIndex.Find(Culture))
	{
		return CultureMap->Contains(SourceText);
	}
	return false;
}

bool FPolyglotDictionary::AddOrUpdateUserEntry(const FPolyglotEntry& InEntry)
{
	if (InEntry.Source.IsEmpty() && InEntry.Key.IsEmpty())
	{
		return false;
	}

	FPolyglotEntry Entry = InEntry;
	if (Entry.Culture.IsEmpty())
	{
		Entry.Culture = Polyglot::Compat::Culture_ChineseSimplified;
	}
	Entry.Plugin.TrimStartAndEndInline();

	// If the caller provided a "source only" entry we do not want an empty key
	// to silently match a localized entry in the user list - they are distinct.

	// Remove an existing entry with the exact same identity (empty translation
	// means "delete").
	for (int32 i = RawEntries.Num() - 1; i >= 0; --i)
	{
		const FRawEntry& Raw = RawEntries[i];
		if (!Raw.bUser)
		{
			continue;
		}
		if (Raw.Data.Culture.Equals(Entry.Culture) &&
			Raw.Data.Namespace.Equals(Entry.Namespace) &&
			Raw.Data.Key.Equals(Entry.Key) &&
			Raw.Data.Source.Equals(Entry.Source))
		{
			RawEntries.RemoveAt(i);
		}
	}

	if (!Entry.Translation.IsEmpty())
	{
		FRawEntry NewRaw;
		NewRaw.Data = Entry;
		NewRaw.bUser = true;
		RawEntries.Add(NewRaw);
	}

	bDirty = true;
	SaveUserFile();
	RebuildIndexes();
	return true;
}

bool FPolyglotDictionary::RemoveUserEntry(const FPolyglotEntry& InEntry)
{
	bool bRemoved = false;
	for (int32 i = RawEntries.Num() - 1; i >= 0; --i)
	{
		const FRawEntry& Raw = RawEntries[i];
		if (!Raw.bUser)
		{
			continue;
		}
		if (Raw.Data.Culture.Equals(InEntry.Culture) &&
			Raw.Data.Namespace.Equals(InEntry.Namespace) &&
			Raw.Data.Key.Equals(InEntry.Key) &&
			Raw.Data.Source.Equals(InEntry.Source))
		{
			RawEntries.RemoveAt(i);
			bRemoved = true;
		}
	}

	if (bRemoved)
	{
		bDirty = true;
		SaveUserFile();
		RebuildIndexes();
	}
	return bRemoved;
}

int32 FPolyglotDictionary::ImportUserEntries(const TArray<FPolyglotEntry>& InEntries)
{
	int32 Imported = 0;
	for (const FPolyglotEntry& InEntry : InEntries)
	{
		if (InEntry.Source.IsEmpty() && InEntry.Key.IsEmpty())
		{
			continue;
		}

		FPolyglotEntry Entry = InEntry;
		if (Entry.Culture.IsEmpty())
		{
			Entry.Culture = Polyglot::Compat::Culture_ChineseSimplified;
		}
		Entry.Plugin.TrimStartAndEndInline();

		// Same identity rule as AddOrUpdateUserEntry: remove then re-add.
		for (int32 i = RawEntries.Num() - 1; i >= 0; --i)
		{
			const FRawEntry& Raw = RawEntries[i];
			if (!Raw.bUser)
			{
				continue;
			}
			if (Raw.Data.Culture.Equals(Entry.Culture) &&
				Raw.Data.Namespace.Equals(Entry.Namespace) &&
				Raw.Data.Key.Equals(Entry.Key) &&
				Raw.Data.Source.Equals(Entry.Source))
			{
				RawEntries.RemoveAt(i);
			}
		}

		if (!Entry.Translation.IsEmpty())
		{
			FRawEntry NewRaw;
			NewRaw.Data = Entry;
			NewRaw.bUser = true;
			RawEntries.Add(NewRaw);
			++Imported;
		}
	}

	if (Imported > 0)
	{
		bDirty = true;
		SaveUserFile();
		RebuildIndexes();
	}
	return Imported;
}

void FPolyglotDictionary::GetEntries(TArray<FPolyglotEntry>& OutEntries, bool bUserOnly) const
{
	OutEntries.Reset(RawEntries.Num());
	for (const FRawEntry& Raw : RawEntries)
	{
		if (!bUserOnly || Raw.bUser)
		{
			OutEntries.Add(Raw.Data);
		}
	}
}

void FPolyglotDictionary::GetLocalizationEntries(TArray<FPolyglotEntry>& OutEntries) const
{
	OutEntries.Reset();
	for (const FRawEntry& Raw : RawEntries)
	{
		const FPolyglotEntry& Entry = Raw.Data;
		if (Entry.Key.IsEmpty() || Entry.Source.IsEmpty() || Entry.Translation.IsEmpty() || Entry.Culture.IsEmpty())
		{
			continue;
		}
		OutEntries.Add(Entry);
	}
}

int32 FPolyglotDictionary::GetEntryCount() const
{
	return RawEntries.Num();
}

FString FPolyglotDictionary::MakeLocalizedToken(const FString& Namespace, const FString& Key)
{
	return Namespace + TEXT("\x1F") + Key;
}

void FPolyglotDictionary::TouchUserFileTimestamp()
{
	const FString Path = UserDictionaryPath;
	if (!Path.IsEmpty() && IFileManager::Get().FileExists(*Path))
	{
		UserFileTimestamp = IFileManager::Get().GetTimeStamp(*Path);
	}
}

// ---------------------------------------------------------------------------
// Loading internals
// ---------------------------------------------------------------------------

void FPolyglotDictionary::LoadBuiltins()
{
	const FString BuiltinDir = PolyglotSettings::GetBuiltinDictionaryDir();
	if (BuiltinDir.IsEmpty() || !IFileManager::Get().DirectoryExists(*BuiltinDir))
	{
		UE_LOG(LogPolyglot, Warning, TEXT("Built-in dictionary directory not found: %s"), *BuiltinDir);
		return;
	}

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *BuiltinDir, TEXT("*.json"), true, false);
	Files.Sort();

	for (const FString& File : Files)
	{
		LoadJsonDictionaryFile(File, /*bUserFile=*/false);
	}
}

void FPolyglotDictionary::LoadUserDirectory()
{
	const FString UserDir = PolyglotSettings::GetUserDictionariesDir();
	if (UserDir.IsEmpty() || !IFileManager::Get().DirectoryExists(*UserDir))
	{
		return;
	}

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *UserDir, TEXT("*.json"), true, false);
	Files.Sort();

	const FString SettingsFile = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*PolyglotSettings::GetSettingsPath());
	for (const FString& File : Files)
	{
		const FString AbsFile = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*File);
		if (AbsFile.Equals(SettingsFile))
		{
			continue; // not a dictionary
		}
		// The main user file is loaded last (highest priority) by LoadUserMainFile.
		if (AbsFile.Equals(IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*PolyglotSettings::GetUserDictionaryPath())))
		{
			continue;
		}
		LoadJsonDictionaryFile(File, /*bUserFile=*/true);
	}
}

void FPolyglotDictionary::LoadUserMainFile()
{
	UserDictionaryPath = PolyglotSettings::GetUserDictionaryPath();
	if (!IFileManager::Get().FileExists(*UserDictionaryPath))
	{
		UserFileTimestamp = FDateTime::MinValue();
		return;
	}

	UserFileTimestamp = IFileManager::Get().GetTimeStamp(*UserDictionaryPath);
	LoadJsonDictionaryFile(UserDictionaryPath, /*bUserFile=*/true);
}

void FPolyglotDictionary::LoadJsonDictionaryFile(const FString& Path, bool bUserFile)
{
	TSharedPtr<FJsonObject> Root;
	if (!PolyglotFileUtil::LoadJsonFromFile(Path, Root))
	{
		UE_LOG(LogPolyglot, Warning, TEXT("Skipping unreadable dictionary file: %s"), *Path);
		return;
	}

	FString DefaultCulture;
	Root->TryGetStringField(TEXT("culture"), DefaultCulture);

	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!Root->TryGetArrayField(TEXT("entries"), Entries))
	{
		UE_LOG(LogPolyglot, Warning, TEXT("Dictionary file has no \"entries\" array: %s"), *Path);
		return;
	}

	int32 Loaded = 0;
	for (const TSharedPtr<FJsonValue>& Value : *Entries)
	{
		const TSharedPtr<FJsonObject>* EntryObj = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(EntryObj) || !(*EntryObj).IsValid())
		{
			continue;
		}

		const TSharedPtr<FJsonObject>& Obj = *EntryObj;
		FRawEntry Raw;
		Raw.bUser = bUserFile;
		Obj->TryGetStringField(TEXT("ns"), Raw.Data.Namespace);
		Obj->TryGetStringField(TEXT("key"), Raw.Data.Key);
		Obj->TryGetStringField(TEXT("source"), Raw.Data.Source);
		Obj->TryGetStringField(TEXT("culture"), Raw.Data.Culture);
		Obj->TryGetStringField(TEXT("plugin"), Raw.Data.Plugin);
		Obj->TryGetStringField(TEXT("scope"), Raw.Data.Scope);
		Obj->TryGetStringField(TEXT("comment"), Raw.Data.Comment);
		Obj->TryGetStringField(TEXT("translation"), Raw.Data.Translation);

		if (Raw.Data.Culture.IsEmpty())
		{
			Raw.Data.Culture = DefaultCulture;
		}
		if (Raw.Data.Culture.IsEmpty())
		{
			Raw.Data.Culture = Polyglot::Compat::Culture_ChineseSimplified;
		}

		// A valid entry has a source (and/or a key) plus a translation.
		if ((Raw.Data.Source.IsEmpty() && Raw.Data.Key.IsEmpty()) || Raw.Data.Translation.IsEmpty())
		{
			continue;
		}

		AddRawEntry(Raw);
		++Loaded;
	}

	UE_LOG(LogPolyglot, Verbose, TEXT("Loaded %d entries from %s"), Loaded, *Path);
}

void FPolyglotDictionary::AddRawEntry(const FRawEntry& InEntry)
{
	RawEntries.Add(InEntry);
}

void FPolyglotDictionary::RebuildIndexes()
{
	CultureSourceIndex.Reset();
	CultureLocalizedIndex.Reset();

	for (const FRawEntry& Raw : RawEntries)
	{
		const FPolyglotEntry& Entry = Raw.Data;
		if (Entry.Culture.IsEmpty() || Entry.Translation.IsEmpty())
		{
			continue;
		}

		if (!Entry.Source.IsEmpty())
		{
			// A second entry (user file loaded later) overrides the earlier one
			// thanks to the plain assignment below.
			CultureSourceIndex.FindOrAdd(Entry.Culture).Add(Entry.Source, Entry.Translation);
		}

		if (!Entry.Key.IsEmpty())
		{
			CultureLocalizedIndex.FindOrAdd(Entry.Culture).Add(
				MakeLocalizedToken(Entry.Namespace, Entry.Key), Entry.Translation);
		}
	}
}

void FPolyglotDictionary::SaveUserFile()
{
	if (!bDirty)
	{
		return;
	}
	bDirty = false;

	const FString Path = PolyglotSettings::GetUserDictionaryPath();
	if (!IFileManager::Get().DirectoryExists(*FPaths::GetPath(Path)))
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("comment"), TEXT("Polyglot user dictionary - edits made by the Polyglot picker / dictionary editor are stored here."));
	Root->SetStringField(TEXT("culture"), Polyglot::Compat::Culture_ChineseSimplified);

	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const FRawEntry& Raw : RawEntries)
	{
		if (!Raw.bUser)
		{
			continue;
		}

		const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("ns"), Raw.Data.Namespace);
		Obj->SetStringField(TEXT("key"), Raw.Data.Key);
		Obj->SetStringField(TEXT("source"), Raw.Data.Source);
		Obj->SetStringField(TEXT("culture"), Raw.Data.Culture);
		Obj->SetStringField(TEXT("translation"), Raw.Data.Translation);
		Obj->SetStringField(TEXT("plugin"), Raw.Data.Plugin);
		Obj->SetStringField(TEXT("scope"), Raw.Data.Scope);
		Obj->SetStringField(TEXT("comment"), Raw.Data.Comment);
		Entries.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Root->SetArrayField(TEXT("entries"), Entries);

	if (PolyglotFileUtil::SaveJsonToFile(Path, Root))
	{
		UserDictionaryPath = Path;
		TouchUserFileTimestamp();
	}
	else
	{
		UE_LOG(LogPolyglot, Error, TEXT("Failed to write user dictionary to %s"), *Path);
	}
}
