// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "HitTicker.h"
#include "HitTickerLog.h"

DEFINE_LOG_CATEGORY(LogHitTicker);

#define LOCTEXT_NAMESPACE "FHitTickerModule"

void FHitTickerModule::StartupModule()
{
	UE_LOG(LogHitTicker, Log, TEXT("HitTicker started."));
}

void FHitTickerModule::ShutdownModule()
{
	UE_LOG(LogHitTicker, Log, TEXT("HitTicker shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHitTickerModule, HitTicker)
