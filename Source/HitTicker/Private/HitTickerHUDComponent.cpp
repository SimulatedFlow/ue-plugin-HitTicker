// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "HitTickerHUDComponent.h"

#include "Engine/World.h"
#include "HitTickerSubsystem.h"

UHitTickerHUDComponent::UHitTickerHUDComponent()
{
	// Nothing to tick: the subsystem owns the ages, this component only forwards a canvas.
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = false;
}

void UHitTickerHUDComponent::DrawNumbers(UCanvas* Canvas)
{
	if (!Canvas)
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		if (UHitTickerSubsystem* Ticker = World->GetSubsystem<UHitTickerSubsystem>())
		{
			Ticker->DrawNumbers(Canvas);
		}
	}
}
