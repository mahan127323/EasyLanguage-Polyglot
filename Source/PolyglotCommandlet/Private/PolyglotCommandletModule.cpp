// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "PolyglotLog.h"

// LogPolyglot is defined and exported by PolyglotCore (see PolyglotLog.h); every
// other module simply imports the same category object.

class FPolyglotCommandletModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogPolyglot, Log, TEXT("PolyglotCommandlet module started."));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogPolyglot, Log, TEXT("PolyglotCommandlet module shut down."));
	}
};

IMPLEMENT_MODULE(FPolyglotCommandletModule, PolyglotCommandlet)
