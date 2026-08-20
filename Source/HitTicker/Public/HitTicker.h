// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

/** HitTicker — pooled, budgeted floating combat numbers drawn on UCanvas (runtime module). */
class FHitTickerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
