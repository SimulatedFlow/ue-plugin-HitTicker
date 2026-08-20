// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "HitTickerStyle.generated.h"

class UTexture2D;

/**
 * How one kind of number looks and moves. A style is an asset, not code: font, colour over life, pop
 * curve, arc, lifetime, prefix and suffix. Change the look of every crit in the game by touching one
 * asset, without recompiling and without opening a Blueprint.
 *
 * Everything a style evaluates is a pure function of the normalised age, which is what lets the ticker
 * skip a frame, drop a number for a frame and pick it back up without anything jumping.
 *
 * Every curve is optional. An empty curve falls back to a hand-written default that already looks like a
 * damage number — the plugin has to do something sensible before anyone has authored a single asset.
 */
UCLASS(BlueprintType)
class HITTICKER_API UHitTickerStyle : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UHitTickerStyle();

	// UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/**
	 * The name the game calls this style by: "Damage", "Crit", "Heal", "Miss"…
	 * Names, not asset references, so gameplay code never has to hard-reference a UI asset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName StyleName;

	/**
	 * Font to draw with. Leave it empty and the engine's default Slate font is used at the size below —
	 * HitTicker ships no font of its own, for licensing reasons.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateFontInfo Font;

	/** Point size. Also used for the engine default font when no font is set above. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text", meta = (ClampMin = "4.0", UIMax = "128.0"))
	float FontSize = 34.0f;

	/** Written in front of the number, e.g. "-" for damage or "+" for healing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FString Prefix;

	/** Written after the number, e.g. "%" or " dmg". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FString Suffix;

	/** Decimal places. 0 means 1234, 1 means 1234.5. Damage numbers are almost always 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text", meta = (ClampMin = "0", ClampMax = "3"))
	int32 DecimalPlaces = 0;

	/** Show 12500 as 12.5k. Keeps late-game numbers from covering the screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	bool bAbbreviateLargeValues = false;

	/** Base colour. Multiplied by Color Over Life when that curve has keys. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor BaseColor = FLinearColor::White;

	/** Optional colour tint across the entry's life. Empty means "no tint"; the alpha channel multiplies the fade. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FRuntimeCurveLinearColor ColorOverLife;

	/** Fraction of the lifetime the number stays fully opaque before it starts fading out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FadeStart = 0.65f;

	/** Outline thickness in pixels. 0 disables the outline. An outline is what makes a number readable on a bright wall. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour", meta = (ClampMin = "0", ClampMax = "16"))
	int32 OutlineSize = 2;

	/** Outline colour. Its alpha fades along with the text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

	/** Drop a shadow behind the text as well as, or instead of, the outline. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	bool bDrawShadow = false;

	/** Shadow offset in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour", meta = (EditCondition = "bDrawShadow"))
	FVector2D ShadowOffset = FVector2D(2.0f, 2.0f);

	/** Shadow colour. Its alpha fades along with the text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour", meta = (EditCondition = "bDrawShadow"))
	FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);

	/** Seconds a number of this style stays on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = "0.05", UIMax = "10.0"))
	float Lifetime = 1.1f;

	/** Pixels the number rises over its whole life, before the rise curve shapes it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (UIMin = "-300.0", UIMax = "300.0"))
	float RiseDistance = 110.0f;

	/** Pixels the number drifts sideways over its whole life. The direction comes from the entry's seed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (UIMin = "0.0", UIMax = "300.0"))
	float DriftDistance = 34.0f;

	/** Rise across the life, in [0..1] on both axes. Empty falls back to an ease-out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	FRuntimeFloatCurve RiseOverLife;

	/** Sideways drift across the life, in [0..1] on both axes. Empty falls back to a linear drift. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	FRuntimeFloatCurve DriftOverLife;

	/** Scale across the life. Empty falls back to a short pop-in — the thing that makes a hit feel like a hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	FRuntimeFloatCurve ScaleOverLife;

	/** Random pixels of scatter applied at spawn, so two simultaneous hits on one actor do not start on the same pixel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = "0.0", UIMax = "120.0"))
	float SpawnScatter = 14.0f;

	/**
	 * Fold repeat hits on the same target into one number that counts up, instead of stacking thirty
	 * separate numbers on one enemy. This is what keeps a bullet-heaven readable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour")
	bool bMergeSameTarget = true;

	/** Let bigger hits draw bigger. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour")
	bool bScaleWithDamage = false;

	/** The value that draws at scale 1 when Scale With Damage is on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "1.0", EditCondition = "bScaleWithDamage"))
	float ScaleReferenceValue = 100.0f;

	/** Upper end of the damage-driven scale, so one lucky 99999 does not fill the screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "1.0", ClampMax = "8.0", EditCondition = "bScaleWithDamage"))
	float MaxDamageScale = 1.75f;

	/** Take part in screen-space overlap avoidance. Off means this style always draws exactly where the hit was. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour")
	bool bAvoidOverlap = true;

	/**
	 * Added to the importance score the per-frame budget sorts by. Push crits and boss damage up so they
	 * are the numbers that survive when the budget bites.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (UIMin = "-200.0", UIMax = "200.0"))
	float ImportanceBias = 0.0f;

	/** Optional damage-type icon drawn to the left of the number. No icon ships with the plugin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icon")
	TObjectPtr<UTexture2D> Icon;

	/** Icon edge length in pixels, before the scale curve. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icon", meta = (ClampMin = "1.0", UIMax = "256.0"))
	float IconSize = 28.0f;

	/** Icon tint. Alpha fades along with the text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icon")
	FLinearColor IconColor = FLinearColor::White;

	/** Colour at the given normalised age, fade included. */
	FLinearColor EvaluateColor(float Alpha) const;

	/** Scale at the given normalised age, pop and damage scaling included. */
	float EvaluateScale(float Alpha, float Value) const;

	/** Screen-space offset at the given normalised age. Positive Y is down, so a rise is negative. */
	FVector2D EvaluateArc(float Alpha, float DriftSign) const;

	/** The font to draw with, at the given final size. Falls back to the engine's default Slate font. */
	FSlateFontInfo GetFontInfo(float SizeScale, const FLinearColor& InOutlineColor) const;

	/** Write prefix + value + suffix into a stack buffer. No FString is allocated on the draw path. */
	void AppendValueText(float Value, FStringBuilderBase& Out) const;
};
