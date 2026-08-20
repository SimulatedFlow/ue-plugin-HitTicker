// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HitTickerSettings.generated.h"

class UHitTickerStyle;

/**
 * Project-wide defaults for the ticker.
 * Edit under Project Settings > Plugins > HitTicker; stored in DefaultGame.ini.
 *
 * Buffer size is read once, when a world's subsystem comes up — the ring is allocated exactly once and
 * never grows. Everything else can be moved at runtime through the subsystem's setters or the
 * HitTicker.* console commands without touching the config.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "HitTicker"))
class HITTICKER_API UHitTickerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UHitTickerSettings();

	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

	/** Convenience accessor. Never returns null. */
	static const UHitTickerSettings& Get();

	/**
	 * Size of the ring buffer, allocated once per world. This is a hard ceiling on how many numbers can
	 * be alive at the same time: hit 513 times in one lifetime window and the oldest number is
	 * overwritten, not the newest dropped — the freshest hit is always the one that matters.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "8", UIMax = "4096"))
	int32 MaxLiveNumbers = 512;

	/**
	 * Hard cap on numbers drawn in one canvas pass. Whatever does not fit is dropped by importance:
	 * near before far, crit before normal, young before old.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "0", UIMax = "1024"))
	int32 MaxDrawnPerFrame = 128;

	/** Numbers further from the camera than this are not projected at all. */
	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "0.0", UIMax = "100000.0"))
	float MaxDrawDistance = 6000.0f;

	/**
	 * Seconds a number stays open for merging. A further hit on the same target with the same style
	 * inside this window is added to it instead of spawning a new number, and resets the window.
	 * Thirty hits a second become one number counting up rather than an unreadable pile.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Merging", meta = (ClampMin = "0.0", UIMax = "3.0"))
	float MergeWindow = 0.25f;

	/** Master switch for merging. Individual styles can still opt out with their own b Merge Same Target. */
	UPROPERTY(Config, EditAnywhere, Category = "Merging")
	bool bMergeEnabled = true;

	/**
	 * Push numbers sideways so two of them do not land on the same pixels. A coarse occupancy grid, not
	 * an n² comparison: the cost does not change when the number of numbers doubles.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Layout")
	bool bAvoidOverlap = true;

	/** Cell size of the occupancy grid in pixels. Roughly one number wide and one number tall. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (EditCondition = "bAvoidOverlap"))
	FIntPoint AvoidanceCellSize = FIntPoint(76, 30);

	/** How many neighbouring cells a number may be pushed into before it gives up and draws where it is. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1", ClampMax = "8", EditCondition = "bAvoidOverlap"))
	int32 AvoidanceSearchRadius = 3;

	/**
	 * The styles this project knows about, addressed by their Style Name. Any of the six standard names
	 * (Damage, Crit, Heal, DamageOverTime, Miss, Block) missing from this list gets a built-in fallback
	 * style at runtime, so the plugin draws something sensible before a single asset has been authored.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Styles", meta = (AllowedClasses = "/Script/HitTicker.HitTickerStyle"))
	TArray<TSoftObjectPtr<UHitTickerStyle>> Styles;

	/** Style used when a call names a style that does not exist. */
	UPROPERTY(Config, EditAnywhere, Category = "Styles")
	FName DefaultStyleName = FName(TEXT("Damage"));

	/**
	 * Draw numbers in an editor viewport with nothing playing.
	 * This is what makes a store screenshot or a camera fly-through possible without pressing Play.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Editor")
	bool bEnableEditorPreview = true;

	/** Show the statistics box as soon as a world's ticker comes up. Same as HitTicker.Stats 1. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bShowStatsByDefault = false;

	/** Radius around the camera focus that HitTicker.Stress scatters its numbers in. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug", meta = (ClampMin = "50.0", UIMax = "20000.0"))
	float StressRadius = 900.0f;

	/** Distance in front of the camera that HitTicker.Stress centres its numbers on. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug", meta = (ClampMin = "50.0", UIMax = "20000.0"))
	float StressDistance = 1400.0f;
};
