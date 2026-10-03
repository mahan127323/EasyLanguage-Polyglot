// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SWidget;

/**
 * Which display-text group a widget belongs to. The group decides whether the
 * widget is patched at all: text blocks are display-only and always safe, while
 * editable boxes participate in user input / renaming and are only patched when
 * the user explicitly opts in (default: off).
 */
enum class EPolyglotTextGroup : uint8
{
	/** Not a text-bearing widget this plugin patches. */
	None,
	/** Display-only text (STextBlock, SRichTextBlock, ...). */
	Static,
	/** User-editable text (SEditableText, SEditableTextBox, ...). */
	Editable,
};

/**
 * FPolyglotWidgetPatcher
 *
 * Central place that reads / rewrites the displayed text of Slate widgets. The
 * scanner only ever works with SWidget; every cast happens here against the
 * widget's GetType() FName (see PolyglotCompat.h WidgetType namespace).
 *
 * Rewriting always goes through SetText(TAttribute<FText>) so the change is a
 * normal, invalidation-aware text update. A patched widget displays whatever we
 * wrote (the scanner keeps the original in its record table to restore later).
 */
class POLYGLOTCORE_API FPolyglotWidgetPatcher
{
public:
	FPolyglotWidgetPatcher() = delete;

	/** Group of a widget given its GetType() name (None when unsupported). */
	static EPolyglotTextGroup GetTextGroup(const FName& WidgetTypeName);

	/** True when this is a display-only (static) text widget. */
	static bool IsStaticTextType(const FName& WidgetTypeName);

	/** True when this is an editable text widget. */
	static bool IsEditableTextType(const FName& WidgetTypeName);

	/**
	 * Read the current displayed text of a supported widget.
	 * @return false when the widget is not a supported text widget.
	 */
	static bool GetWidgetText(const TSharedRef<SWidget>& Widget, FText& OutText);

	/**
	 * Replace the displayed text of a supported widget.
	 * @return true when the widget was rewritten.
	 */
	static bool SetWidgetText(const TSharedRef<SWidget>& Widget, const FText& NewText);

	/** Convenience: replaces the text only when the widget is supported. */
	static bool SetWidgetText(const TSharedRef<SWidget>& Widget, const FString& NewText)
	{
		return SetWidgetText(Widget, FText::FromString(NewText));
	}
};
