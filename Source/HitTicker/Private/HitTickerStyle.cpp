// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "HitTickerStyle.h"

#include "Misc/StringBuilder.h"
#include "Styling/CoreStyle.h"

namespace HitTickerStyleDefaults
{
	/** True when a runtime curve actually has something to say. An empty curve evaluates to zero, which would mean "invisible". */
	static bool HasKeys(const FRuntimeFloatCurve& Curve)
	{
		const FRichCurve* Rich = Curve.GetRichCurveConst();
		return Rich != nullptr && Rich->GetNumKeys() > 0;
	}

	static bool HasKeys(const FRuntimeCurveLinearColor& Curve)
	{
		return Curve.ExternalCurve != nullptr || Curve.ColorCurves[0].GetNumKeys() > 0;
	}

	/** Ease-out rise: quick off the mark, drifting to a stop. What a damage number does in every game that ships. */
	static float DefaultRise(float Alpha)
	{
		const float Inv = 1.0f - Alpha;
		return 1.0f - Inv * Inv;
	}

	/** Linear sideways drift. Combined with the per-entry direction this is what fans a burst out. */
	static float DefaultDrift(float Alpha)
	{
		return Alpha;
	}

	/**
	 * Pop-in: 0.55 -> 1.15 in the first eighth of the life, settling to 1.0 by a third.
	 * This is the whole reason a merged number reads as a new hit — merging resets the age, so the pop replays.
	 */
	static float DefaultScale(float Alpha)
	{
		if (Alpha < 0.12f)
		{
			return FMath::Lerp(0.55f, 1.15f, Alpha / 0.12f);
		}
		if (Alpha < 0.32f)
		{
			return FMath::Lerp(1.15f, 1.0f, (Alpha - 0.12f) / 0.2f);
		}
		return 1.0f;
	}
}

UHitTickerStyle::UHitTickerStyle()
{
}

FPrimaryAssetId UHitTickerStyle::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("HitTickerStyle"), GetFName());
}

FLinearColor UHitTickerStyle::EvaluateColor(float Alpha) const
{
	FLinearColor Result = BaseColor;

	if (HitTickerStyleDefaults::HasKeys(ColorOverLife))
	{
		Result *= ColorOverLife.GetLinearColorValue(Alpha);
	}

	// The fade is applied on top of whatever the curve said, so an author can tint without having to
	// remember to author an alpha channel that reaches zero.
	if (Alpha > FadeStart && FadeStart < 1.0f)
	{
		Result.A *= 1.0f - FMath::Clamp((Alpha - FadeStart) / (1.0f - FadeStart), 0.0f, 1.0f);
	}

	return Result;
}

float UHitTickerStyle::EvaluateScale(float Alpha, float Value) const
{
	float Scale = HitTickerStyleDefaults::HasKeys(ScaleOverLife)
		? ScaleOverLife.GetRichCurveConst()->Eval(Alpha)
		: HitTickerStyleDefaults::DefaultScale(Alpha);

	if (bScaleWithDamage && ScaleReferenceValue > 0.0f)
	{
		// Square root, not linear: ten times the damage should read as noticeably bigger, not ten times bigger.
		const float Ratio = FMath::Sqrt(FMath::Max(FMath::Abs(Value), 0.0f) / ScaleReferenceValue);
		Scale *= FMath::Clamp(Ratio, 0.6f, MaxDamageScale);
	}

	return FMath::Max(Scale, 0.01f);
}

FVector2D UHitTickerStyle::EvaluateArc(float Alpha, float DriftSign) const
{
	const float Rise = HitTickerStyleDefaults::HasKeys(RiseOverLife)
		? RiseOverLife.GetRichCurveConst()->Eval(Alpha)
		: HitTickerStyleDefaults::DefaultRise(Alpha);

	const float Drift = HitTickerStyleDefaults::HasKeys(DriftOverLife)
		? DriftOverLife.GetRichCurveConst()->Eval(Alpha)
		: HitTickerStyleDefaults::DefaultDrift(Alpha);

	// Screen space has Y pointing down, so rising means subtracting.
	return FVector2D(Drift * DriftDistance * DriftSign, -Rise * RiseDistance);
}

FSlateFontInfo UHitTickerStyle::GetFontInfo(float SizeScale, const FLinearColor& InOutlineColor) const
{
	const float FinalSize = FMath::Max(FontSize * SizeScale, 1.0f);

	FSlateFontInfo Result = Font;
	if (Result.HasValidFont())
	{
		Result.Size = FinalSize;
	}
	else
	{
		// No font ships with HitTicker on purpose (licensing), so an unconfigured style falls back to the
		// engine's own Slate font rather than drawing nothing at all.
		Result = FCoreStyle::GetDefaultFontStyle("Bold", FinalSize);
	}

	if (OutlineSize > 0)
	{
		// The outline is scaled with the text; a fixed-pixel outline on a scaled-up crit looks like a hairline.
		Result.OutlineSettings.OutlineSize = FMath::Max(1, FMath::RoundToInt(OutlineSize * SizeScale));
		Result.OutlineSettings.OutlineColor = InOutlineColor;
	}
	else
	{
		Result.OutlineSettings.OutlineSize = 0;
	}

	return Result;
}

void UHitTickerStyle::AppendValueText(float Value, FStringBuilderBase& Out) const
{
	Out.Append(*Prefix);

	const float Magnitude = FMath::Abs(Value);

	if (bAbbreviateLargeValues && Magnitude >= 1000.0f)
	{
		static const TCHAR* Units[] = { TEXT("k"), TEXT("M"), TEXT("B") };

		float Scaled = Magnitude;
		int32 UnitIndex = -1;
		while (Scaled >= 1000.0f && UnitIndex < 2)
		{
			Scaled /= 1000.0f;
			++UnitIndex;
		}

		Out.Appendf(TEXT("%.1f%s"), Scaled, Units[FMath::Clamp(UnitIndex, 0, 2)]);
	}
	else if (DecimalPlaces <= 0)
	{
		Out.Appendf(TEXT("%d"), FMath::RoundToInt(Magnitude));
	}
	else
	{
		Out.Appendf(TEXT("%.*f"), FMath::Clamp(DecimalPlaces, 0, 3), Magnitude);
	}

	Out.Append(*Suffix);
}
