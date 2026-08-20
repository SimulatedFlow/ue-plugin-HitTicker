// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "HitTickerHUDComponent.generated.h"

class UCanvas;

/**
 * The way into an existing project that already has its own HUD class.
 *
 * Add this component to your HUD actor and call DrawNumbers(Canvas) from your own DrawHUD — one line —
 * and the ticker draws in the same canvas pass as the rest of your HUD. Nobody should have to swap out
 * their HUD class to use a damage-number plugin, so this is a peer of AHitTickerHUD, not a fallback.
 *
 * The component does no work of its own: no tick, no state. It is a named place to hang the call.
 */
UCLASS(ClassGroup = (HitTicker), meta = (BlueprintSpawnableComponent, DisplayName = "HitTicker HUD Component"))
class HITTICKER_API UHitTickerHUDComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHitTickerHUDComponent();

	/**
	 * Draw every visible floating number onto the canvas.
	 *
	 * Call this from your HUD's DrawHUD (Blueprint: from the Receive Draw HUD event, passing the Canvas):
	 *
	 *     void AMyHUD::DrawHUD()
	 *     {
	 *         Super::DrawHUD();
	 *         HitTickerComponent->DrawNumbers(Canvas);
	 *     }
	 */
	UFUNCTION(BlueprintCallable, Category = "HitTicker")
	void DrawNumbers(UCanvas* Canvas);
};
