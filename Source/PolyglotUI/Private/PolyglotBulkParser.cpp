// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "PolyglotBulkParser.h"

namespace PolyglotUI
{
	namespace
	{
		/** Normalise CRLF / CR / LF to plain LF. */
		FString NormaliseNewlines(const FString& InText)
		{
			FString Text = InText;
			Text.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
			Text.ReplaceInline(TEXT("\r"), TEXT("\n"));
			return Text;
		}

		/** Convert a single "source/translation" token into an entry. */
		bool TokenToEntry(FString Token, const FString& DefaultCulture, FPolyglotEntry& OutEntry)
		{
			Token = Token.TrimStartAndEnd();
			if (Token.IsEmpty())
			{
				return false;
			}

			// The pair separator is the last '/'. Both sides are trimmed.
			const int32 Slash = Token.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (Slash == INDEX_NONE || Slash == 0 || Slash == Token.Len() - 1)
			{
				return false;
			}

			FString Source = Token.Left(Slash).TrimStartAndEnd();
			FString Translation = Token.Mid(Slash + 1).TrimStartAndEnd();
			if (Source.IsEmpty() || Translation.IsEmpty())
			{
				return false;
			}

			OutEntry.Source = MoveTemp(Source);
			OutEntry.Translation = MoveTemp(Translation);
			OutEntry.Culture = DefaultCulture;
			OutEntry.Plugin = TEXT("Manual");
			return true;
		}
	}

	FBulkParseResult ParseBulkEntryText(const FString& Text, const FString& DefaultCulture, TArray<FPolyglotEntry>& OutEntries)
	{
		FBulkParseResult Result;

		const FString Normalised = NormaliseNewlines(Text);
		if (Normalised.IsEmpty())
		{
			return Result;
		}

		TArray<FString> Lines;
		Normalised.ParseIntoArray(Lines, TEXT("\n"), /*bCullEmpty=*/false);

		for (const FString& RawLine : Lines)
		{
			const FString Line = RawLine.TrimStartAndEnd();
			if (Line.IsEmpty())
			{
				continue;
			}

			// A line may carry several comma separated pairs.
			TArray<FString> CommaItems;
			Line.ParseIntoArray(CommaItems, TEXT(","), /*bCullEmpty=*/false);
			for (const FString& RawItem : CommaItems)
			{
				const FString Item = RawItem.TrimStartAndEnd();
				if (Item.IsEmpty())
				{
					continue;
				}
				++Result.ItemCount;

				FPolyglotEntry Entry;
				if (TokenToEntry(Item, DefaultCulture, Entry))
				{
					OutEntries.Add(MoveTemp(Entry));
					++Result.ValidCount;
				}
				else
				{
					++Result.SkippedCount;
				}
			}
		}

		return Result;
	}
}
