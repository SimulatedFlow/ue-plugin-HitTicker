// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "HitTickerSettings.h"

UHitTickerSettings::UHitTickerSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("HitTicker");
}

FName UHitTickerSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FName UHitTickerSettings::GetSectionName() const
{
	return TEXT("HitTicker");
}

const UHitTickerSettings& UHitTickerSettings::Get()
{
	const UHitTickerSettings* Settings = GetDefault<UHitTickerSettings>();
	check(Settings);
	return *Settings;
}
