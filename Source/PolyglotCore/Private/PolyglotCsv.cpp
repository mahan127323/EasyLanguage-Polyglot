// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "PolyglotCsv.h"

#include "Misc/StringBuilder.h"

namespace PolyglotCsv
{
	static const TCHAR* HeaderLine = TEXT("Namespace,Key,Source,Translation,Culture,Plugin");

	FString EscapeCell(const FString& Cell)
	{
		if (!Cell.Contains(TEXT("\"")) && !Cell.Contains(TEXT(",")) && !Cell.Contains(TEXT("\n")) && !Cell.Contains(TEXT("\r")))
		{
			return Cell;
		}

		FString Escaped = Cell;
		Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""));
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}

	FString UnescapeCell(const FString& Cell)
	{
		if (Cell.Len() >= 2 && Cell[0] == TEXT('"') && Cell[Cell.Len() - 1] == TEXT('"'))
		{
			FString Inner = Cell.Mid(1, Cell.Len() - 2);
			Inner.ReplaceInline(TEXT("\"\""), TEXT("\""));
			return Inner;
		}
		return Cell;
	}

	static void SplitRow(const FString& Line, TArray<FString>& OutCells)
	{
		OutCells.Reset();
		if (Line.IsEmpty())
		{
			return;
		}

		bool bInQuotes = false;
		FString Current;
		for (int32 i = 0; i < Line.Len(); ++i)
		{
			const TCHAR Ch = Line[i];
			if (Ch == TEXT('"'))
			{
				if (bInQuotes && i + 1 < Line.Len() && Line[i + 1] == TEXT('"'))
				{
					Current.AppendChar(TEXT('"'));
					++i;
				}
				else
				{
					bInQuotes = !bInQuotes;
				}
			}
			else if (Ch == TEXT(',') && !bInQuotes)
			{
				OutCells.Add(Current);
				Current.Reset();
			}
			else
			{
				Current.AppendChar(Ch);
			}
		}
		OutCells.Add(Current);
	}

	bool ParseEntries(const FString& CsvText, const FString& DefaultCulture, TArray<FPolyglotEntry>& OutEntries)
	{
		OutEntries.Reset();

		TArray<FString> Lines;
		CsvText.ParseIntoArrayLines(Lines, /*bCullEmpty=*/false);

		// Skip optional UTF-8 BOM on the first line.
		int32 StartLine = 0;
		if (Lines.Num() > 0 && Lines[0].Len() > 0 && Lines[0][0] == TCHAR(0xFEFF))
		{
			Lines[0].RemoveAt(0);
		}

		if (Lines.Num() > 0)
		{
			TArray<FString> Header;
			SplitRow(Lines[0], Header);
			if (Header.Num() > 0 && Header[0].Equals(TEXT("Namespace"), ESearchCase::IgnoreCase))
			{
				StartLine = 1;
			}
		}

		for (int32 LineIndex = StartLine; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString& Line = Lines[LineIndex];
			if (Line.TrimStartAndEnd().IsEmpty())
			{
				continue;
			}

			TArray<FString> Cells;
			SplitRow(Line, Cells);
			if (Cells.Num() < 4)
			{
				continue;
			}

			FPolyglotEntry Entry;
			Entry.Namespace = UnescapeCell(Cells[0]);
			Entry.Key = UnescapeCell(Cells[1]);
			Entry.Source = UnescapeCell(Cells[2]);
			Entry.Translation = UnescapeCell(Cells[3]);

			Entry.Culture = DefaultCulture;
			if (Cells.Num() >= 5 && !Cells[4].IsEmpty())
			{
				Entry.Culture = UnescapeCell(Cells[4]);
			}
			if (Cells.Num() >= 6)
			{
				Entry.Plugin = UnescapeCell(Cells[5]);
			}

			if ((Entry.Source.IsEmpty() && Entry.Key.IsEmpty()) || Entry.Translation.IsEmpty())
			{
				continue;
			}

			OutEntries.Add(Entry);
		}

		return true;
	}

	FString WriteEntries(const TArray<FPolyglotEntry>& InEntries)
	{
		TStringBuilder<1024> Builder;
		Builder.Append(HeaderLine);
		Builder.Append(TEXT("\r\n"));

		for (const FPolyglotEntry& Entry : InEntries)
		{
			Builder.Append(EscapeCell(Entry.Namespace));
			Builder.AppendChar(TEXT(','));
			Builder.Append(EscapeCell(Entry.Key));
			Builder.AppendChar(TEXT(','));
			Builder.Append(EscapeCell(Entry.Source));
			Builder.AppendChar(TEXT(','));
			Builder.Append(EscapeCell(Entry.Translation));
			Builder.AppendChar(TEXT(','));
			Builder.Append(EscapeCell(Entry.Culture.IsEmpty() ? TEXT("zh-Hans") : Entry.Culture));
			Builder.AppendChar(TEXT(','));
			Builder.Append(EscapeCell(Entry.Plugin));
			Builder.Append(TEXT("\r\n"));
		}

		return Builder.ToString();
	}
}
