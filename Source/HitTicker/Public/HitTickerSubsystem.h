// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HitTickerTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "HitTickerSubsystem.generated.h"

class AActor;
class APlayerController;
class UCanvas;
class UHitTickerStyle;

/**
 * The ticker. One per world, ticks itself, owns every floating number in that world.
 *
 * The usual way to show a damage number is one UUserWidget per hit. That means, per hit: allocate an
 * object, build a Slate tree, run a layout pass, touch the font atlas, and hand the whole thing to the
 * garbage collector a second later. Thirty hits a second — a bullet-heaven, a tower defence, an ARPG
 * with area damage — and that is a measurable hitch, which is exactly why studios rewrite it.
 *
 * HitTicker replaces all of it with three things:
 *
 *   1. A ring buffer of fixed size, allocated once when the world comes up. Adding a number while the
 *      buffer is full overwrites the oldest entry rather than dropping the newest — the freshest hit is
 *      the one the player is looking for. Nothing is allocated while the game runs.
 *   2. One canvas pass per frame that projects, sorts, budgets and draws. Text goes out as a string view
 *      built in a stack buffer, so even the formatting allocates nothing.
 *   3. A hard per-frame budget. Past it, numbers are dropped by importance — near before far, crit
 *      before normal, young before old — so the display degrades in the order a player would choose.
 *
 * Motion is a pure function of (age, style, seed). No per-entry velocity, no integration, no drift: a
 * number that was not drawn for four frames lands exactly where it would have been if it had been.
 */
UCLASS()
class HITTICKER_API UHitTickerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override;

	// ------------------------------------------------------------------------------------------------
	// Spawning numbers
	// ------------------------------------------------------------------------------------------------

	/**
	 * Show a number at a world point.
	 *
	 * @param WorldLocation Where the hit landed. If Target is set, this is remembered as an offset from
	 *                      the target so the number rides along with it.
	 * @param Value         The number to show. Its sign is ignored; use the style's prefix for "-" or "+".
	 * @param StyleName     Which style to draw it in. Unknown names fall back to the default style.
	 * @param Target        Optional. Needed for merging and for numbers that follow a moving actor.
	 * @return The ring slot the number landed in, or INDEX_NONE if there was no style to draw it with.
	 */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (AdvancedDisplay = "3"))
	int32 AddNumber(FVector WorldLocation, float Value, FName StyleName, AActor* Target = nullptr);

	/** Damage number in the "Damage" style. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (AdvancedDisplay = "2"))
	int32 AddDamage(FVector WorldLocation, float Value, AActor* Target = nullptr);

	/** Healing number in the "Heal" style. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (AdvancedDisplay = "2"))
	int32 AddHeal(FVector WorldLocation, float Value, AActor* Target = nullptr);

	/** Critical hit in the "Crit" style. Crits outrank normal hits when the per-frame budget bites. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (AdvancedDisplay = "2"))
	int32 AddCrit(FVector WorldLocation, float Value, AActor* Target = nullptr);

	/** Periodic damage in the "DamageOverTime" style. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (AdvancedDisplay = "2"))
	int32 AddDamageOverTime(FVector WorldLocation, float Value, AActor* Target = nullptr);

	/**
	 * Words instead of a number: MISS, BLOCK, IMMUNE, RESIST.
	 * The text is interned as an FName, so repeating the same word costs nothing after the first time.
	 */
	UFUNCTION(BlueprintCallable, Category = "HitTicker", meta = (AdvancedDisplay = "2"))
	int32 AddText(FVector WorldLocation, FName Text, FName StyleName, AActor* Target = nullptr);

	/** Drop every live number. The ring keeps its memory; only the contents go. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker")
	void ClearAll();

	/** Drop every live number belonging to one actor, e.g. when it despawns without a death animation. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker")
	int32 ClearForTarget(AActor* Target);

	// ------------------------------------------------------------------------------------------------
	// Drawing
	// ------------------------------------------------------------------------------------------------

	/**
	 * Draw every visible number onto the canvas. This is the whole renderer.
	 *
	 * Call it from your own HUD class:  Ticker->DrawNumbers(Canvas);  — one line, in DrawHUD.
	 * Or use AHitTickerHUD / UHitTickerHUDComponent, which do exactly that for you.
	 */
	UFUNCTION(BlueprintCallable, Category = "HitTicker")
	void DrawNumbers(UCanvas* Canvas);

	// ------------------------------------------------------------------------------------------------
	// Knobs
	// ------------------------------------------------------------------------------------------------

	/** Change the hard per-frame draw budget. Takes effect on the next canvas pass. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker")
	void SetBudget(int32 InMaxDrawnPerFrame);

	/** Current per-frame draw budget. */
	UFUNCTION(BlueprintPure, Category = "HitTicker")
	int32 GetBudget() const { return MaxDrawnPerFrame; }

	/** Distance beyond which numbers are not drawn. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker")
	void SetMaxDrawDistance(float InMaxDrawDistance);

	/** Current draw distance limit. */
	UFUNCTION(BlueprintPure, Category = "HitTicker")
	float GetMaxDrawDistance() const { return MaxDrawDistance; }

	/** Master switch for same-target merging. Styles can still opt out individually. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker")
	void SetMergeEnabled(bool bInMergeEnabled);

	/** True while same-target merging is allowed at all. */
	UFUNCTION(BlueprintPure, Category = "HitTicker")
	bool IsMergeEnabled() const { return bMergeEnabled; }

	/** Seconds a number stays open for merging. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker")
	void SetMergeWindow(float InMergeWindow);

	/** Current merge window in seconds. */
	UFUNCTION(BlueprintPure, Category = "HitTicker")
	float GetMergeWindow() const { return MergeWindow; }

	/** Show or hide the on-screen statistics box. */
	UFUNCTION(BlueprintCallable, Category = "HitTicker")
	void SetShowStats(bool bInShowStats) { bShowStats = bInShowStats; }

	/** True while the statistics box is being drawn. */
	UFUNCTION(BlueprintPure, Category = "HitTicker")
	bool IsShowingStats() const { return bShowStats; }

	/** What the ticker did on the most recent frame. */
	UFUNCTION(BlueprintPure, Category = "HitTicker")
	const FHitTickerStats& GetStats() const { return Stats; }

	/** Numbers currently alive. Same as GetStats().Live, without the copy. */
	UFUNCTION(BlueprintPure, Category = "HitTicker")
	int32 GetLiveCount() const { return Stats.Live; }

	/** Ring buffer size for this world. Fixed when the world came up; changing the setting needs a restart. */
	UFUNCTION(BlueprintPure, Category = "HitTicker")
	int32 GetCapacity() const { return Ring.Num(); }

	/**
	 * Fire numbers at a steady rate around the camera, for measuring. Zero switches it off.
	 * The same thing HitTicker.Stress does — and the tool the store screenshots are made with.
	 */
	UFUNCTION(BlueprintCallable, Category = "HitTicker|Debug")
	void SetStressRate(float NumbersPerSecond);

	/** Current stress-test rate in numbers per second. */
	UFUNCTION(BlueprintPure, Category = "HitTicker|Debug")
	float GetStressRate() const { return StressRate; }

	/** Style asset registered under this name, or null. */
	UFUNCTION(BlueprintPure, Category = "HitTicker")
	UHitTickerStyle* GetStyleByName(FName StyleName) const;

	/** Every style this world knows about, built-in fallbacks included. */
	UFUNCTION(BlueprintPure, Category = "HitTicker")
	TArray<FName> GetStyleNames() const;

private:
	/** One candidate for this frame's draw pass: an already-projected entry with its importance score. */
	struct FDrawCandidate
	{
		int32 Slot = INDEX_NONE;
		FVector2D ScreenPos = FVector2D::ZeroVector;
		float Score = 0.0f;
		float Scale = 1.0f;
	};

	/** Pull the runtime knobs out of the project settings and size the ring. Called once, on Initialize. */
	void ApplySettings();

	/** Load the configured style assets and fill in built-in fallbacks for anything missing. */
	void ResolveStyles();

	/** Build a transient style so the plugin draws something sensible before any asset has been authored. */
	UHitTickerStyle* CreateBuiltinStyle(FName StyleName, const FLinearColor& Color, float Size, const FString& Prefix);

	/** Index into Styles, or the default style's index, or INDEX_NONE when there are no styles at all. */
	int32 FindStyleIndex(FName StyleName) const;

	/** Take the next free ring slot, overwriting the oldest entry when the ring is full. */
	int32 AllocateSlot();

	/** Fold a hit into an existing number when one is still open for it. Returns the slot, or INDEX_NONE. */
	int32 TryMerge(uint32 MergeKey, AActor* Target, float Value, const FVector& WorldLocation);

	/** Drop a slot out of the merge lookup, if it is the one the key points at. */
	void ForgetMergeKey(int32 Slot);

	/** Age every entry, retire the expired ones, and walk the ring head past the dead ones at the front. */
	void AgeEntries(float DeltaTime);

	/** Spawn the stress-test numbers due this frame. */
	void UpdateStress(float DeltaTime);

	/** The draw pass itself, shared by the HUD path and the editor preview path. */
	void DrawPass(UCanvas* Canvas);

	/** Project, cull, score and sort; fills DrawCandidates. */
	void GatherCandidates(UCanvas* Canvas, const FVector& ViewLocation, const FVector& ViewForward);

	/** Resolve the sticky sideways dodge for candidates that do not have one yet. */
	void ResolveOverlaps(UCanvas* Canvas);

	/** Draw one candidate. */
	void DrawCandidate(UCanvas* Canvas, const FDrawCandidate& Candidate);

	/** The statistics box, drawn in the same pass so it costs one more text item and nothing else. */
	void DrawStatsBox(UCanvas* Canvas) const;

	/** Best available camera position and direction for this canvas. */
	bool ResolveView(UCanvas* Canvas, FVector& OutLocation, FVector& OutForward) const;

#if WITH_EDITOR
	/**
	 * Editor-viewport draw callback. Only compiled WITH_EDITOR — see the comment on the registration.
	 */
	void OnEditorPreviewDraw(UCanvas* Canvas, APlayerController* PlayerController);
#endif

	/**
	 * The ring. Allocated exactly once, in Initialize, and never resized: no allocation while the game
	 * runs is the whole product, and a TArray that can grow is a TArray that will grow at the worst
	 * possible moment. Full means the oldest entry is overwritten — never the newest one dropped.
	 */
	TArray<FHitTickerEntry> Ring;

	/** Index of the oldest live entry. */
	int32 RingHead = 0;

	/** Number of slots in use, from RingHead forward. Never exceeds Ring.Num(). */
	int32 RingCount = 0;

	/** Merge key to ring slot, so folding a repeat hit into a live number stays O(1) at any hit rate. Reserved up front. */
	TMap<uint32, int32> MergeLookup;

	/** Resolved styles. Indexed by FHitTickerEntry::StyleIndex, which is a uint8 — hence the 255 cap. */
	UPROPERTY()
	TArray<TObjectPtr<UHitTickerStyle>> Styles;

	/** Style name to index into Styles. */
	TMap<FName, int32> StyleByName;

	/** This frame's draw list. Reserved to the ring size in Initialize, then only ever reset. */
	TArray<FDrawCandidate> DrawCandidates;

	/** Screen-space occupancy grid for overlap avoidance. Resized only when the viewport size changes. */
	TArray<uint8> AvoidanceGrid;
	FIntPoint AvoidanceGridDims = FIntPoint::ZeroValue;
	FIntPoint AvoidanceViewportSize = FIntPoint::ZeroValue;

	/** Runtime copies of the project settings. */
	int32 MaxDrawnPerFrame = 128;
	float MaxDrawDistance = 6000.0f;
	float MaxDrawDistanceSq = 0.0f;
	float MergeWindow = 0.25f;
	bool bMergeEnabled = true;
	bool bAvoidOverlap = true;
	FIntPoint AvoidanceCellSize = FIntPoint(76, 30);
	int32 AvoidanceSearchRadius = 3;
	FName DefaultStyleName = FName(TEXT("Damage"));
	float StressRadius = 900.0f;
	float StressDistance = 1400.0f;

	/** Index of DefaultStyleName in Styles, resolved once. */
	int32 DefaultStyleIndex = INDEX_NONE;

	/** On-screen statistics box. */
	bool bShowStats = false;

	/** Stress-test rate and its fractional carry, so 1.5 numbers a second really means 1.5. */
	float StressRate = 0.0f;
	float StressAccumulator = 0.0f;

	/** Counter feeding the per-entry seed. Wraps harmlessly. */
	uint32 SpawnCounter = 0;

	/** Last camera position and direction seen by a draw pass. Used by the stress test and by distance culling. */
	FVector CachedViewLocation = FVector::ZeroVector;
	FVector CachedViewForward = FVector::ForwardVector;
	bool bHasCachedView = false;

	/** Frame the last real HUD pass drew on, so the editor preview never draws on top of the game's own pass. */
	uint64 LastGameDrawFrame = 0;

	/** Handle for the editor-only preview draw. */
	FDelegateHandle EditorPreviewHandle;

	/** Published through GetStats(). */
	FHitTickerStats Stats;
};
