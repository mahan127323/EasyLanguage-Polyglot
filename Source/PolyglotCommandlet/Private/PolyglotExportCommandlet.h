// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "PolyglotExportCommandlet.generated.h"

/**
 * Headless "untranslated scan" exporter (see the .cpp file for details).
 * Diffs English vs Chinese .locres data and writes a CSV of every entry
 * that still needs a translation.
 */
UCLASS()
class UPolyglotExportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UPolyglotExportCommandlet();

	virtual int32 Main(const FString& Params) override;
};
