// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "HitTickerHUD.h"

#include "HitTickerHUDComponent.h"

AHitTickerHUD::AHitTickerHUD()
{
	PrimaryActorTick.bCanEverTick = false;

	TickerComponent = CreateDefaultSubobject<UHitTickerHUDComponent>(TEXT("HitTickerComponent"));
}

void AHitTickerHUD::DrawHUD()
{
	Super::DrawHUD();

	if (bDrawHitTickerNumbers && TickerComponent)
	{
		// One line. That is the entire integration, whether it happens here or in a customer's own HUD.
		TickerComponent->DrawNumbers(Canvas);
	}
}
