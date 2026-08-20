// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HitTickerHUD.generated.h"

class UHitTickerHUDComponent;

/**
 * The ready-made HUD class: set it as your game mode's HUD Class and floating numbers draw, with nothing
 * else to wire up.
 *
 * This is the main draw path and it is deliberately AHUD-based: DrawHUD runs in a cooked Shipping build,
 * which is where the numbers have to work. The editor-viewport preview in the subsystem is the second
 * attachment point, not this one.
 *
 * Already have your own HUD class? Do not replace it — add UHitTickerHUDComponent to it and call
 * DrawNumbers(Canvas) from your own DrawHUD instead. Both paths end in the same subsystem call.
 */
UCLASS(meta = (DisplayName = "HitTicker HUD"))
class HITTICKER_API AHitTickerHUD : public AHUD
{
	GENERATED_BODY()

public:
	AHitTickerHUD();

	// AHUD interface
	virtual void DrawHUD() override;

	/** Draw the floating numbers when this HUD renders. Off means the HUD behaves like a plain AHUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitTicker")
	bool bDrawHitTickerNumbers = true;

protected:
	/** Present so that a Blueprint child of this class can reach the same call the C++ path uses. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HitTicker")
	TObjectPtr<UHitTickerHUDComponent> TickerComponent;
};
