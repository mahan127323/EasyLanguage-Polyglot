// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "PolyglotWidgetPatcher.h"

#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

#include "PolyglotCompat.h"
#include "PolyglotLog.h"

EPolyglotTextGroup FPolyglotWidgetPatcher::GetTextGroup(const FName& WidgetTypeName)
{
	if (WidgetTypeName == Polyglot::Compat::WidgetType::STextBlock()
		|| WidgetTypeName == Polyglot::Compat::WidgetType::SRichTextBlock()
		|| WidgetTypeName == Polyglot::Compat::WidgetType::SInlineEditableTextBlock())
	{
		return EPolyglotTextGroup::Static;
	}
	if (WidgetTypeName == Polyglot::Compat::WidgetType::SEditableText()
		|| WidgetTypeName == Polyglot::Compat::WidgetType::SEditableTextBox()
		|| WidgetTypeName == Polyglot::Compat::WidgetType::SMultiLineEditableText()
		|| WidgetTypeName == Polyglot::Compat::WidgetType::SMultiLineEditableTextBox())
	{
		return EPolyglotTextGroup::Editable;
	}
	return EPolyglotTextGroup::None;
}

bool FPolyglotWidgetPatcher::IsStaticTextType(const FName& WidgetTypeName)
{
	return GetTextGroup(WidgetTypeName) == EPolyglotTextGroup::Static;
}

bool FPolyglotWidgetPatcher::IsEditableTextType(const FName& WidgetTypeName)
{
	return GetTextGroup(WidgetTypeName) == EPolyglotTextGroup::Editable;
}

bool FPolyglotWidgetPatcher::GetWidgetText(const TSharedRef<SWidget>& Widget, FText& OutText)
{
	const FName Type = Widget->GetType();
	switch (GetTextGroup(Type))
	{
	case EPolyglotTextGroup::Static:
		if (Type == Polyglot::Compat::WidgetType::STextBlock())
		{
			OutText = StaticCastSharedRef<STextBlock>(Widget)->GetText();
			return true;
		}
		if (Type == Polyglot::Compat::WidgetType::SRichTextBlock())
		{
			OutText = StaticCastSharedRef<SRichTextBlock>(Widget)->GetText();
			return true;
		}
		if (Type == Polyglot::Compat::WidgetType::SInlineEditableTextBlock())
		{
			OutText = StaticCastSharedRef<SInlineEditableTextBlock>(Widget)->GetText();
			return true;
		}
		break;

	case EPolyglotTextGroup::Editable:
		if (Type == Polyglot::Compat::WidgetType::SEditableText())
		{
			OutText = StaticCastSharedRef<SEditableText>(Widget)->GetText();
			return true;
		}
		if (Type == Polyglot::Compat::WidgetType::SEditableTextBox())
		{
			OutText = StaticCastSharedRef<SEditableTextBox>(Widget)->GetText();
			return true;
		}
		if (Type == Polyglot::Compat::WidgetType::SMultiLineEditableText())
		{
			OutText = StaticCastSharedRef<SMultiLineEditableText>(Widget)->GetText();
			return true;
		}
		if (Type == Polyglot::Compat::WidgetType::SMultiLineEditableTextBox())
		{
			OutText = StaticCastSharedRef<SMultiLineEditableTextBox>(Widget)->GetText();
			return true;
		}
		break;

	default:
		break;
	}
	return false;
}

bool FPolyglotWidgetPatcher::SetWidgetText(const TSharedRef<SWidget>& Widget, const FText& NewText)
{
	const FName Type = Widget->GetType();
	switch (GetTextGroup(Type))
	{
	case EPolyglotTextGroup::Static:
		if (Type == Polyglot::Compat::WidgetType::STextBlock())
		{
			StaticCastSharedRef<STextBlock>(Widget)->SetText(NewText);
			return true;
		}
		if (Type == Polyglot::Compat::WidgetType::SRichTextBlock())
		{
			// Rich text uses inline markup; strings containing markup would
			// destroy styled content, so refuse them - visibly, not silently.
			const FString Plain = NewText.ToString();
			if (Plain.Contains(TEXT("<")) || Plain.Contains(TEXT(">")))
			{
				UE_LOG(LogPolyglot, Verbose,
					TEXT("SRichTextBlock skipped: translation contains markup characters ('%s')."),
					*Plain.Left(120));
				return false;
			}
			StaticCastSharedRef<SRichTextBlock>(Widget)->SetText(TAttribute<FText>(NewText));
			return true;
		}
		if (Type == Polyglot::Compat::WidgetType::SInlineEditableTextBlock())
		{
			// Both edit and display mode share this text; rewriting the display
			// text is the same "original -> translation" operation as STextBlock.
			StaticCastSharedRef<SInlineEditableTextBlock>(Widget)->SetText(NewText);
			return true;
		}
		break;

	case EPolyglotTextGroup::Editable:
		if (Type == Polyglot::Compat::WidgetType::SEditableText())
		{
			StaticCastSharedRef<SEditableText>(Widget)->SetText(TAttribute<FText>(NewText));
			return true;
		}
		if (Type == Polyglot::Compat::WidgetType::SEditableTextBox())
		{
			StaticCastSharedRef<SEditableTextBox>(Widget)->SetText(TAttribute<FText>(NewText));
			return true;
		}
		if (Type == Polyglot::Compat::WidgetType::SMultiLineEditableText())
		{
			StaticCastSharedRef<SMultiLineEditableText>(Widget)->SetText(TAttribute<FText>(NewText));
			return true;
		}
		if (Type == Polyglot::Compat::WidgetType::SMultiLineEditableTextBox())
		{
			StaticCastSharedRef<SMultiLineEditableTextBox>(Widget)->SetText(TAttribute<FText>(NewText));
			return true;
		}
		break;

	default:
		break;
	}
	return false;
}
