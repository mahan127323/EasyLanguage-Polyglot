// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "PolyglotAdapters.h"

#include "Modules/ModuleManager.h"
#include "PolyglotLog.h"

class FPolyglotAdaptersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Logging is intentionally light: every probe is lazy (query only, no
		// editor module is loaded by us). The status list is queried by the UI
		// layer for the "About / support" dialog.
		UE_LOG(LogPolyglot, Verbose, TEXT("PolyglotAdapters module started."));
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FPolyglotAdaptersModule, PolyglotAdapters)
