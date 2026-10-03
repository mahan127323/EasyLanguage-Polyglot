// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/**
 * Log category shared by every Polyglot module.
 *
 * The category object is defined (DEFINE_LOG_CATEGORY) inside PolyglotCore and
 * exported so that the adapters / UI / commandlet modules can share the same
 * verbosity toggles. Decorating the extern declaration with POLYGLOTCORE_API is
 * what makes it importable from the PolyglotCore DLL on Windows.
 */
POLYGLOTCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogPolyglot, Log, All);
