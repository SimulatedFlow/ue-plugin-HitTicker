// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "HitTickerTypes.generated.h"

class AActor;

/**
 * Bit flags stored in FHitTickerEntry::Flags.
 * Deliberately a plain uint8 bitmask and not a UENUM: the entry is a POD that the draw pass walks by the
 * hundred every frame, and nothing here is ever exposed to Blueprint one flag at a time.
 */
namespace EHitTickerEntryFlag
{
	enum Type : uint8
	{
		None = 0,

		/** Critical hit. Survives the budget cut longer than a normal hit and may use a bigger scale. */
		Crit = 1 << 0,

		/** Healing rather than damage. Only meaningful to the style that draws it. */
		Heal = 1 << 1,

		/** Damage over time / periodic tick. Usually a quieter style. */
		OverTime = 1 << 2,

		/** Draw Text instead of Value — MISS, BLOCK, IMMUNE. */
		TextOnly = 1 << 3,

		/** The screen-space dodge has been resolved once and must not be recomputed (it would jitter). */
		DodgeResolved = 1 << 4,

		/** At least one further hit was folded into this entry. */
		Merged = 1 << 5,

		/** Drift sideways to the left instead of the right. Derived from the entry seed at spawn. */
		DriftLeft = 1 << 6,
	};
}

/**
 * What the ticker did. Read it from Blueprint through UHitTickerStatics::GetHitTickerStats, or put it on
 * screen with `HitTicker.Stats 1`.
 *
 * Live and BufferHighWater are levels; DrawnThisFrame, DroppedByBudget, CulledByDistance and DrawMs are
 * per-frame and reset at the start of every draw pass; Merged counts up until the next ClearAll.
 */
USTRUCT(BlueprintType)
struct FHitTickerStats
{
	GENERATED_BODY()

	/** Numbers currently alive in the ring buffer. Can never exceed Max Live Numbers — that is the point. */
	UPROPERTY(BlueprintReadOnly, Category = "HitTicker")
	int32 Live = 0;

	/** Numbers actually drawn in the most recent canvas pass. Can never exceed Max Drawn Per Frame. */
	UPROPERTY(BlueprintReadOnly, Category = "HitTicker")
	int32 DrawnThisFrame = 0;

	/** Hits folded into an existing number instead of taking a slot of their own, since the last ClearAll. */
	UPROPERTY(BlueprintReadOnly, Category = "HitTicker")
	int32 Merged = 0;

	/** Visible numbers the per-frame budget refused to draw in the most recent pass, least important first. */
	UPROPERTY(BlueprintReadOnly, Category = "HitTicker")
	int32 DroppedByBudget = 0;

	/** Numbers skipped in the most recent pass: too far away, behind the camera, or off screen. Never projected, never sorted. */
	UPROPERTY(BlueprintReadOnly, Category = "HitTicker")
	int32 CulledByDistance = 0;

	/** Wall-clock milliseconds spent inside the most recent draw pass, projection and sorting included. */
	UPROPERTY(BlueprintReadOnly, Category = "HitTicker")
	float DrawMs = 0.0f;

	/** Highest slot count the ring buffer ever reached. If this equals Max Live Numbers, the ring is doing its job. */
	UPROPERTY(BlueprintReadOnly, Category = "HitTicker")
	int32 BufferHighWater = 0;
};

/**
 * One floating number. Plain struct on purpose — no UObject, no widget, no shared pointer.
 *
 * The whole plugin exists because the usual answer to "show a damage number" is a UUserWidget per hit:
 * an object to construct, a Slate tree to build, a layout pass to run and a font atlas to touch, thirty
 * times a second. An entry here is 96-ish bytes in an array that was allocated once, and its motion is a
 * pure function of (Age, style, seed) — so a skipped frame changes nothing and nothing needs saving
 * between frames except the age.
 */
struct FHitTickerEntry
{
	/** World point the hit landed on. Refreshed from Target every frame when a target is set. */
	FVector WorldOrigin = FVector::ZeroVector;

	/** WorldOrigin minus the target's location at spawn, so a number rides a moving target instead of hanging in the air. */
	FVector TargetOffset = FVector::ZeroVector;

	/**
	 * Accumulated screen-space offset: the sideways dodge that keeps two numbers from landing on top of
	 * each other. Resolved exactly once, on the first frame this entry is drawn, and then left alone —
	 * a dodge recomputed every frame is a number that vibrates.
	 */
	FVector2D ScreenOffset = FVector2D::ZeroVector;

	/** The number to show. Zero for text-only entries (MISS, BLOCK). */
	float Value = 0.0f;

	/** Seconds since this entry was spawned or last merged into. The only state the motion needs. */
	float Age = 0.0f;

	/** Seconds this entry lives, copied from its style at spawn. Zero means the slot is free. */
	float Lifetime = 0.0f;

	/** Index into the subsystem's resolved style list. */
	uint8 StyleIndex = 0;

	/** EHitTickerEntryFlag bitmask. */
	uint8 Flags = EHitTickerEntryFlag::None;

	/** How many hits are rolled into this entry. 1 for a plain hit. */
	uint16 MergeCount = 1;

	/** Stable per-entry randomness: drift direction and spawn jitter come from here, so replays match. */
	uint32 Seed = 0;

	/** Who was hit. Weak, so deleting the actor while its numbers are still flying is a non-event. */
	TWeakObjectPtr<AActor> Target;

	/** Target and style folded into one key. Zero means this entry does not take part in merging. */
	uint32 MergeKey = 0;

	/** Text for text-only entries. FName so that repeating "MISS" thirty times a second allocates nothing. */
	FName Text;

	/** True while this slot holds a number that has not run out its lifetime. */
	FORCEINLINE bool IsAlive() const { return Lifetime > 0.0f && Age < Lifetime; }

	/** Normalised position in the entry's life, 0 at spawn, 1 at death. */
	FORCEINLINE float GetAlpha() const { return Lifetime > 0.0f ? FMath::Clamp(Age / Lifetime, 0.0f, 1.0f) : 1.0f; }

	FORCEINLINE bool HasFlag(EHitTickerEntryFlag::Type Flag) const { return (Flags & Flag) != 0; }
};
