// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"

class UPolyglotSubsystem;
class SWindow;

/**
 * Internal helpers shared between the PolyglotUI translation units. The UI is
 * kept deliberately thin: every actual action goes through UPolyglotSubsystem,
 * so nothing here may hold pipeline state.
 */
namespace PolyglotUI
{
	/** Valid subsystem (requires the editor subsystem collection to be up). */
	UPolyglotSubsystem* GetSubsystem();

	/** Parent window used for our modeless dialogs (first interactive window or null). */
	TSharedPtr<SWindow> GetParentWindow();

	/** Show a transient Slate notification (bottom right). */
	void Notify(const FText& Message);

	/** One-click English <-> Chinese toggle (enables first when off). */
	void ToggleLanguage();

	/** Open the main control panel. */
	void OpenMainPanel();

	/** Open the dictionary editor window. */
	void OpenDictionaryEditorWindow();

	/** Enter / leave text-picking mode (visual translation picker). */
	void TogglePickerMode();

	// ------------------------------------------------------------------------
	// Bilingual (English / Simplified Chinese) UI helpers.
	// ------------------------------------------------------------------------

	/** True when the editor is currently displaying a Chinese (zh*) UI. */
	bool IsChineseUi();

	/** Pick the English or Simplified-Chinese text for the current engine culture. */
	FText UiText(const TCHAR* English, const TCHAR* SimplifiedChinese);
}
