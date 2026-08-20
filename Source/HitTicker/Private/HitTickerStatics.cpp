// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "HitTickerStatics.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HitTickerSubsystem.h"

UHitTickerSubsystem* UHitTickerStatics::GetHitTicker(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UHitTickerSubsystem>() : nullptr;
}

void UHitTickerStatics::AddDamageNumber(const UObject* WorldContextObject, FVector WorldLocation, float Damage, AActor* Target)
{
	if (UHitTickerSubsystem* Ticker = GetHitTicker(WorldContextObject))
	{
		Ticker->AddDamage(WorldLocation, Damage, Target);
	}
}

void UHitTickerStatics::AddHealNumber(const UObject* WorldContextObject, FVector WorldLocation, float Healing, AActor* Target)
{
	if (UHitTickerSubsystem* Ticker = GetHitTicker(WorldContextObject))
	{
		Ticker->AddHeal(WorldLocation, Healing, Target);
	}
}

void UHitTickerStatics::AddCritNumber(const UObject* WorldContextObject, FVector WorldLocation, float Damage, AActor* Target)
{
	if (UHitTickerSubsystem* Ticker = GetHitTicker(WorldContextObject))
	{
		Ticker->AddCrit(WorldLocation, Damage, Target);
	}
}

void UHitTickerStatics::AddDamageOverTimeNumber(const UObject* WorldContextObject, FVector WorldLocation, float Damage, AActor* Target)
{
	if (UHitTickerSubsystem* Ticker = GetHitTicker(WorldContextObject))
	{
		Ticker->AddDamageOverTime(WorldLocation, Damage, Target);
	}
}

void UHitTickerStatics::AddCombatText(const UObject* WorldContextObject, FVector WorldLocation, FName Text, FName StyleName, AActor* Target)
{
	if (UHitTickerSubsystem* Ticker = GetHitTicker(WorldContextObject))
	{
		Ticker->AddText(WorldLocation, Text, StyleName, Target);
	}
}

void UHitTickerStatics::AddStyledNumber(const UObject* WorldContextObject, FVector WorldLocation, float Value, FName StyleName, AActor* Target)
{
	if (UHitTickerSubsystem* Ticker = GetHitTicker(WorldContextObject))
	{
		Ticker->AddNumber(WorldLocation, Value, StyleName, Target);
	}
}

FHitTickerStats UHitTickerStatics::GetHitTickerStats(const UObject* WorldContextObject)
{
	if (const UHitTickerSubsystem* Ticker = GetHitTicker(WorldContextObject))
	{
		return Ticker->GetStats();
	}
	return FHitTickerStats();
}

void UHitTickerStatics::ClearAll(const UObject* WorldContextObject)
{
	if (UHitTickerSubsystem* Ticker = GetHitTicker(WorldContextObject))
	{
		Ticker->ClearAll();
	}
}

void UHitTickerStatics::ClearForTarget(const UObject* WorldContextObject, AActor* Target)
{
	if (UHitTickerSubsystem* Ticker = GetHitTicker(WorldContextObject))
	{
		Ticker->ClearForTarget(Target);
	}
}

void UHitTickerStatics::SetBudget(const UObject* WorldContextObject, int32 MaxDrawnPerFrame)
{
	if (UHitTickerSubsystem* Ticker = GetHitTicker(WorldContextObject))
	{
		Ticker->SetBudget(MaxDrawnPerFrame);
	}
}

void UHitTickerStatics::SetMergeEnabled(const UObject* WorldContextObject, bool bMergeEnabled)
{
	if (UHitTickerSubsystem* Ticker = GetHitTicker(WorldContextObject))
	{
		Ticker->SetMergeEnabled(bMergeEnabled);
	}
}

void UHitTickerStatics::SetShowStats(const UObject* WorldContextObject, bool bShowStats)
{
	if (UHitTickerSubsystem* Ticker = GetHitTicker(WorldContextObject))
	{
		Ticker->SetShowStats(bShowStats);
	}
}

void UHitTickerStatics::SetStressRate(const UObject* WorldContextObject, float NumbersPerSecond)
{
	if (UHitTickerSubsystem* Ticker = GetHitTicker(WorldContextObject))
	{
		Ticker->SetStressRate(NumbersPerSecond);
	}
}
