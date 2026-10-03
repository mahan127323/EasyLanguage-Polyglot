// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "PolyglotFileUtil.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Internationalization/Text.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace PolyglotFileUtil
{
	bool ReadUtf8FileToString(const FString& FilePath, FString& OutString)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *FilePath))
		{
			return false;
		}

		// Trim a UTF-8 BOM if present, then decode the rest as UTF-8.
		int32 Offset = 0;
		if (Bytes.Num() >= 3 && Bytes[0] == 0xEF && Bytes[1] == 0xBB && Bytes[2] == 0xBF)
		{
			Offset = 3;
		}

		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + Offset), Bytes.Num() - Offset);
		OutString = FString(Converter.Length(), Converter.Get());
		return true;
	}

	bool WriteUtf8StringToFile(const FString& FilePath, const FString& Content)
	{
		FTCHARToUTF8 Converter(*Content, Content.Len());
		TArrayView<const uint8> Bytes(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
		return FFileHelper::SaveArrayToFile(Bytes, *FilePath);
	}

	bool LoadJsonFromFile(const FString& FilePath, TSharedPtr<FJsonObject>& OutRoot)
	{
		FString Content;
		if (!ReadUtf8FileToString(FilePath, Content))
		{
			return false;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		return FJsonSerializer::Deserialize(Reader, OutRoot) && OutRoot.IsValid();
	}

	bool SaveJsonToFile(const FString& FilePath, const TSharedRef<FJsonObject>& Root)
	{
		FString Content;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
		if (!FJsonSerializer::Serialize(Root, Writer))
		{
			return false;
		}
		return WriteUtf8StringToFile(FilePath, Content);
	}
}
