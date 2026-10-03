// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "PolyglotCompat.h"

#include "Modules/ModuleManager.h"

namespace Polyglot::Compat
{
	bool IsModuleLoaded(const FString& ModuleName)
	{
		return FModuleManager::Get().IsModuleLoaded(*ModuleName);
	}

	bool TryLoadModule(const FString& ModuleName)
	{
		return FModuleManager::Get().ModuleExists(*ModuleName)
			&& FModuleManager::Get().LoadModule(*ModuleName) != nullptr;
	}
}
