// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HitTickerTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HitTickerStatics.generated.h"

class AActor;
class UHitTickerSubsystem;

/**
 * Blueprint shortcuts, so an ability graph never has to spell out the subsystem lookup to show a number.
 * Every one of these is a null-safe no-op when the world has no ticker (a dedicated server, for instance).
 */
UCLASS()
class HITTICKER_API UHitTickerStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The ticker for the given context's world, or null. */
	UFUNCTION(BlueprintPure, Category = "HitTicker", meta = (WorldContext = "WorldContextObject", DisplayName = "Get HitTicker"))
	static UHitTickerSubsystem* GetHitTicker(const UObject* WorldContextObject);

	/**
	 * Show a damage number at a world point.
	 * Pass the actor that was hit whenever you have it: that is what lets repeat hits merge into one
	 * counting-up number and what makes the number ride along with a moving enemy.
	 */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "3"))
	static void AddDamageNumber(const UObject* WorldContextObject, FVector WorldLocation, float Damage, AActor* Target = nullptr);

	/** Show a healing number at a world point. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "3"))
	static void AddHealNumber(const UObject* WorldContextObject, FVector WorldLocation, float Healing, AActor* Target = nullptr);

	/** Show a critical hit. Crits outrank normal hits when the per-frame budget bites. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "3"))
	static void AddCritNumber(const UObject* WorldContextObject, FVector WorldLocation, float Damage, AActor* Target = nullptr);

	/** Show a damage-over-time tick. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "3"))
	static void AddDamageOverTimeNumber(const UObject* WorldContextObject, FVector WorldLocation, float Damage, AActor* Target = nullptr);

	/** Show words instead of a number: MISS, BLOCK, IMMUNE. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "4"))
	static void AddCombatText(const UObject* WorldContextObject, FVector WorldLocation, FName Text, FName StyleName, AActor* Target = nullptr);

	/** Show a number in a named style of your own. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "4"))
	static void AddStyledNumber(const UObject* WorldContextObject, FVector WorldLocation, float Value, FName StyleName, AActor* Target = nullptr);

	/** What the ticker did on the most recent frame. Everything zero if the world has no ticker. */
	UFUNCTION(BlueprintPure, Category = "HitTicker", meta = (WorldContext = "WorldContextObject"))
	static FHitTickerStats GetHitTickerStats(const UObject* WorldContextObject);

	/** Drop every number currently on screen. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (WorldContext = "WorldContextObject"))
	static void ClearAll(const UObject* WorldContextObject);

	/** Drop every number belonging to one actor, e.g. when it despawns. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (WorldContext = "WorldContextObject"))
	static void ClearForTarget(const UObject* WorldContextObject, AActor* Target);

	/** Change the hard per-frame draw budget. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (WorldContext = "WorldContextObject"))
	static void SetBudget(const UObject* WorldContextObject, int32 MaxDrawnPerFrame);

	/** Master switch for folding repeat hits on one target into one number. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (WorldContext = "WorldContextObject"))
	static void SetMergeEnabled(const UObject* WorldContextObject, bool bMergeEnabled);

	/** Show or hide the on-screen statistics box. Same as HitTicker.Stats. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (WorldContext = "WorldContextObject"))
	static void SetShowStats(const UObject* WorldContextObject, bool bShowStats);

	/** Fire numbers at a steady rate around the camera, for measuring. Zero switches it off. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker|Debug", meta = (WorldContext = "WorldContextObject"))
	static void SetStressRate(const UObject* WorldContextObject, float NumbersPerSecond);
};
