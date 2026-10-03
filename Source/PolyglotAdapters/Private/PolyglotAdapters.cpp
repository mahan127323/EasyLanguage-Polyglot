// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#include "PolyglotAdapters.h"

#include "Modules/ModuleManager.h"

namespace PolyglotAdapters
{
	namespace
	{
		// Probe a module without ever forcing it to load. Loading the big editor
		// modules just to check them would change the editor's startup behaviour;
		// "exists" is enough for the status we report.
		bool ModuleExists(const FString& ModuleName)
		{
			return FModuleManager::Get().ModuleExists(*ModuleName);
		}

		bool ModuleIsLoaded(const FString& ModuleName)
		{
			return FModuleManager::Get().IsModuleLoaded(*ModuleName);
		}

		void FillStatus(FAdapterStatus& Status)
		{
			if (Status.Modules.IsEmpty())
			{
				return;
			}

			Status.bModulesExist = true;
			Status.bModulesLoaded = true;
			for (const FString& ModuleName : Status.Modules)
			{
				Status.bModulesExist = Status.bModulesExist && ModuleExists(ModuleName);
				Status.bModulesLoaded = Status.bModulesLoaded && ModuleIsLoaded(ModuleName);
			}
		}
	}

	TArray<FAdapterStatus> QueryAllAdapterStatuses()
	{
		TArray<FAdapterStatus> Statuses;
		Statuses.Add(QueryNiagaraStatus());
		Statuses.Add(QueryMaterialStatus());
		Statuses.Add(QueryBlueprintStatus());
		Statuses.Add(QueryContentBrowserStatus());
		for (FAdapterStatus& Status : Statuses)
		{
			FillStatus(Status);
		}
		return Statuses;
	}
}
