// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "HitTickerSubsystem.h"

#include "Application/SlateApplicationBase.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GlobalRenderResources.h"
#include "HAL/IConsoleManager.h"
#include "HitTickerLog.h"
#include "HitTickerSettings.h"
#include "HitTickerStyle.h"
#include "Misc/StringBuilder.h"
#include "Rendering/SlateRenderer.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "Stats/Stats.h"
#include "TextureResource.h"
#include "UObject/ObjectKey.h"

DECLARE_STATS_GROUP(TEXT("HitTicker"), STATGROUP_HitTicker, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("HitTicker Draw"), STAT_HitTickerDraw, STATGROUP_HitTicker);
DECLARE_DWORD_COUNTER_STAT(TEXT("Numbers Drawn"), STAT_HitTickerDrawn, STATGROUP_HitTicker);
DECLARE_DWORD_COUNTER_STAT(TEXT("Numbers Merged"), STAT_HitTickerMerged, STATGROUP_HitTicker);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Numbers Live"), STAT_HitTickerLive, STATGROUP_HitTicker);

namespace HitTickerNames
{
	static const FName Damage(TEXT("Damage"));
	static const FName Crit(TEXT("Crit"));
	static const FName Heal(TEXT("Heal"));
	static const FName DamageOverTime(TEXT("DamageOverTime"));
	static const FName Miss(TEXT("Miss"));
	static const FName Block(TEXT("Block"));
}

namespace HitTickerDraw
{
	/**
	 * Width and height of a piece of text in the given font.
	 *
	 * The measure service is the same cache Slate itself uses, so measuring a hundred numbers a frame is a
	 * hundred hash lookups and no glyph work. The fallback is only reached in a build with no Slate
	 * application at all (a dedicated server that somehow got here), where nothing is drawn anyway.
	 */
	static FVector2D MeasureText(FStringView Text, const FSlateFontInfo& FontInfo)
	{
		if (FSlateApplicationBase::IsInitialized())
		{
			if (FSlateRenderer* Renderer = FSlateApplicationBase::Get().GetRenderer())
			{
				const UE::Slate::FDeprecateVector2DResult Measured =
					Renderer->GetFontMeasureService()->Measure(Text, FontInfo, 1.0f);
				return FVector2D(Measured.X, Measured.Y);
			}
		}

		return FVector2D(Text.Len() * FontInfo.Size * 0.55f, FontInfo.Size * 1.15f);
	}

	/** Deterministic [0,1) from a seed. The entries carry the seed, so a replay draws the same scatter. */
	static float SeedToUnitFloat(uint32 Seed, uint32 Salt)
	{
		uint32 Hash = HashCombine(Seed, Salt);
		Hash ^= Hash >> 15;
		Hash *= 0x2c1b3c6du;
		Hash ^= Hash >> 12;
		return static_cast<float>(Hash & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
	}
}

namespace HitTickerConsole
{
	/**
	 * Console commands apply to every world that has a ticker, not just the one the console happens to be
	 * bound to. HitTicker.Stress 600 then does the same thing whether it is typed during PIE or in the
	 * editor with nothing playing — which is exactly the case the store screenshots are made in.
	 */
	static void ForEachSubsystem(UWorld* World, TFunctionRef<void(UHitTickerSubsystem&)> Func)
	{
		TArray<UHitTickerSubsystem*, TInlineAllocator<4>> Found;

		if (World)
		{
			if (UHitTickerSubsystem* Subsystem = World->GetSubsystem<UHitTickerSubsystem>())
			{
				Found.Add(Subsystem);
			}
		}

		if (Found.Num() == 0 && GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.World())
				{
					if (UHitTickerSubsystem* Subsystem = Context.World()->GetSubsystem<UHitTickerSubsystem>())
					{
						Found.AddUnique(Subsystem);
					}
				}
			}
		}

		for (UHitTickerSubsystem* Subsystem : Found)
		{
			Func(*Subsystem);
		}
	}

	static bool ParseBool(const TArray<FString>& Args, bool bDefault)
	{
		if (Args.Num() == 0)
		{
			return bDefault;
		}
		return Args[0].ToBool() || Args[0] == TEXT("1");
	}

	static FAutoConsoleCommandWithWorldAndArgs GStats(
		TEXT("HitTicker.Stats"),
		TEXT("HitTicker.Stats 0|1 - show the on-screen ticker statistics box."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const bool bShow = ParseBool(Args, true);
			ForEachSubsystem(World, [bShow](UHitTickerSubsystem& Subsystem) { Subsystem.SetShowStats(bShow); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GBudget(
		TEXT("HitTicker.Budget"),
		TEXT("HitTicker.Budget <n> - hard cap on numbers drawn per frame."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() == 0)
			{
				ForEachSubsystem(World, [](UHitTickerSubsystem& Subsystem)
				{
					UE_LOG(LogHitTicker, Display, TEXT("HitTicker.Budget = %d"), Subsystem.GetBudget());
				});
				return;
			}
			const int32 Budget = FCString::Atoi(*Args[0]);
			ForEachSubsystem(World, [Budget](UHitTickerSubsystem& Subsystem) { Subsystem.SetBudget(Budget); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GClear(
		TEXT("HitTicker.Clear"),
		TEXT("HitTicker.Clear - drop every number currently on screen."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			ForEachSubsystem(World, [](UHitTickerSubsystem& Subsystem) { Subsystem.ClearAll(); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GStress(
		TEXT("HitTicker.Stress"),
		TEXT("HitTicker.Stress <numbers_per_second> - fire numbers at the camera at a steady rate. 0 stops it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const float Rate = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f;
			ForEachSubsystem(World, [Rate](UHitTickerSubsystem& Subsystem) { Subsystem.SetStressRate(Rate); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GMerge(
		TEXT("HitTicker.Merge"),
		TEXT("HitTicker.Merge 0|1 - master switch for folding repeat hits on one target into one number."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const bool bMerge = ParseBool(Args, true);
			ForEachSubsystem(World, [bMerge](UHitTickerSubsystem& Subsystem) { Subsystem.SetMergeEnabled(bMerge); });
		}));
}

// -------------------------------------------------------------------------------------------------------
// Lifetime
// -------------------------------------------------------------------------------------------------------

void UHitTickerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ApplySettings();
	ResolveStyles();

#if WITH_EDITOR
	// Second attachment point, editor only, and deliberately not the main one.
	//
	// The main path is AHUD::DrawHUD (see AHitTickerHUD and UHitTickerHUDComponent), because that is the
	// path that exists in a cooked Shipping build: UDebugDrawService is compiled out there entirely.
	//
	// This registration is still not a toy. The store images and the store video for this plugin are made
	// as a camera move through an editor viewport with nothing playing — and what does not draw there
	// cannot be advertised. So the numbers have to fly in an editor viewport as well, which is what this
	// gives us, at zero cost to the shipping build.
	if (UHitTickerSettings::Get().bEnableEditorPreview)
	{
		EditorPreviewHandle = UDebugDrawService::Register(
			TEXT("Game"),
			FDebugDrawDelegate::CreateUObject(this, &UHitTickerSubsystem::OnEditorPreviewDraw));
	}
#endif
}

void UHitTickerSubsystem::Deinitialize()
{
#if WITH_EDITOR
	if (EditorPreviewHandle.IsValid())
	{
		UDebugDrawService::Unregister(EditorPreviewHandle);
		EditorPreviewHandle.Reset();
	}
#endif

	Ring.Reset();
	RingHead = 0;
	RingCount = 0;
	MergeLookup.Reset();
	DrawCandidates.Reset();
	AvoidanceGrid.Reset();
	Styles.Reset();
	StyleByName.Reset();

	Super::Deinitialize();
}

bool UHitTickerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Editor is in the list on purpose: numbers have to fly in the viewport without PIE.
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::Editor;
}

bool UHitTickerSubsystem::IsTickableInEditor() const
{
	return UHitTickerSettings::Get().bEnableEditorPreview;
}

TStatId UHitTickerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHitTickerSubsystem, STATGROUP_Tickables);
}

void UHitTickerSubsystem::ApplySettings()
{
	const UHitTickerSettings& Settings = UHitTickerSettings::Get();

	MaxDrawnPerFrame = FMath::Max(0, Settings.MaxDrawnPerFrame);
	MaxDrawDistance = FMath::Max(0.0f, Settings.MaxDrawDistance);
	MaxDrawDistanceSq = MaxDrawDistance * MaxDrawDistance;
	MergeWindow = FMath::Max(0.0f, Settings.MergeWindow);
	bMergeEnabled = Settings.bMergeEnabled;
	bAvoidOverlap = Settings.bAvoidOverlap;
	AvoidanceCellSize.X = FMath::Max(8, Settings.AvoidanceCellSize.X);
	AvoidanceCellSize.Y = FMath::Max(8, Settings.AvoidanceCellSize.Y);
	AvoidanceSearchRadius = FMath::Clamp(Settings.AvoidanceSearchRadius, 1, 8);
	DefaultStyleName = Settings.DefaultStyleName;
	StressRadius = Settings.StressRadius;
	StressDistance = Settings.StressDistance;
	bShowStats = Settings.bShowStatsByDefault;

	// The one allocation this plugin makes. Everything after this point runs inside it.
	const int32 Capacity = FMath::Clamp(Settings.MaxLiveNumbers, 8, 65536);
	Ring.Empty(Capacity);
	Ring.SetNum(Capacity);
	RingHead = 0;
	RingCount = 0;

	// Reserved to the ring size so that folding a hit into a live number never reallocates either.
	MergeLookup.Empty(Capacity);
	DrawCandidates.Empty(Capacity);

	Stats = FHitTickerStats();
}

// -------------------------------------------------------------------------------------------------------
// Styles
// -------------------------------------------------------------------------------------------------------

void UHitTickerSubsystem::ResolveStyles()
{
	Styles.Reset();
	StyleByName.Reset();

	const UHitTickerSettings& Settings = UHitTickerSettings::Get();

	for (const TSoftObjectPtr<UHitTickerStyle>& SoftStyle : Settings.Styles)
	{
		if (SoftStyle.IsNull())
		{
			continue;
		}

		UHitTickerStyle* Style = SoftStyle.LoadSynchronous();
		if (!Style)
		{
			UE_LOG(LogHitTicker, Warning, TEXT("Style '%s' could not be loaded and is ignored."), *SoftStyle.ToString());
			continue;
		}

		const FName Name = Style->StyleName.IsNone() ? Style->GetFName() : Style->StyleName;
		if (StyleByName.Contains(Name))
		{
			UE_LOG(LogHitTicker, Warning, TEXT("Two styles claim the name '%s'; the second one is ignored."), *Name.ToString());
			continue;
		}

		// StyleIndex is a uint8 in the entry, so 255 styles is the ceiling. Nobody will find it.
		if (Styles.Num() >= 255)
		{
			UE_LOG(LogHitTicker, Warning, TEXT("More than 255 styles configured; '%s' and everything after it is ignored."), *Name.ToString());
			break;
		}

		StyleByName.Add(Name, Styles.Num());
		Styles.Add(Style);
	}

	// Built-in fallbacks. The plugin has to draw something sensible on a project where nobody has authored
	// a style yet — an installed plugin that shows nothing reads as a broken plugin.
	struct FBuiltin
	{
		FName Name;
		FLinearColor Color;
		float Size;
		const TCHAR* Prefix;
	};

	static const FBuiltin Builtins[] =
	{
		{ HitTickerNames::Damage,         FLinearColor(1.00f, 0.92f, 0.70f, 1.0f), 34.0f, TEXT("") },
		{ HitTickerNames::Crit,           FLinearColor(1.00f, 0.45f, 0.10f, 1.0f), 52.0f, TEXT("") },
		{ HitTickerNames::Heal,           FLinearColor(0.35f, 1.00f, 0.45f, 1.0f), 34.0f, TEXT("+") },
		{ HitTickerNames::DamageOverTime, FLinearColor(0.70f, 0.45f, 1.00f, 1.0f), 26.0f, TEXT("") },
		{ HitTickerNames::Miss,           FLinearColor(0.75f, 0.78f, 0.85f, 1.0f), 30.0f, TEXT("") },
		{ HitTickerNames::Block,          FLinearColor(0.55f, 0.75f, 1.00f, 1.0f), 30.0f, TEXT("") },
	};

	for (const FBuiltin& Builtin : Builtins)
	{
		if (StyleByName.Contains(Builtin.Name) || Styles.Num() >= 255)
		{
			continue;
		}

		if (UHitTickerStyle* Style = CreateBuiltinStyle(Builtin.Name, Builtin.Color, Builtin.Size, Builtin.Prefix))
		{
			StyleByName.Add(Builtin.Name, Styles.Num());
			Styles.Add(Style);
		}
	}

	DefaultStyleIndex = INDEX_NONE;
	if (const int32* Found = StyleByName.Find(DefaultStyleName))
	{
		DefaultStyleIndex = *Found;
	}
	else if (Styles.Num() > 0)
	{
		DefaultStyleIndex = 0;
	}
}

UHitTickerStyle* UHitTickerSubsystem::CreateBuiltinStyle(FName StyleName, const FLinearColor& Color, float Size, const FString& Prefix)
{
	UHitTickerStyle* Style = NewObject<UHitTickerStyle>(this, NAME_None, RF_Transient);
	if (!Style)
	{
		return nullptr;
	}

	Style->StyleName = StyleName;
	Style->BaseColor = Color;
	Style->FontSize = Size;
	Style->Prefix = Prefix;
	Style->OutlineSize = 2;

	if (StyleName == HitTickerNames::Crit)
	{
		Style->Lifetime = 1.4f;
		Style->RiseDistance = 150.0f;
		Style->bScaleWithDamage = true;
		Style->ImportanceBias = 60.0f;
		Style->Suffix = TEXT("!");
	}
	else if (StyleName == HitTickerNames::DamageOverTime)
	{
		Style->Lifetime = 0.85f;
		Style->RiseDistance = 70.0f;
	}
	else if (StyleName == HitTickerNames::Miss || StyleName == HitTickerNames::Block)
	{
		Style->Lifetime = 0.9f;
		Style->RiseDistance = 80.0f;
		Style->bMergeSameTarget = false;
	}

	return Style;
}

int32 UHitTickerSubsystem::FindStyleIndex(FName StyleName) const
{
	if (const int32* Found = StyleByName.Find(StyleName))
	{
		return *Found;
	}
	return DefaultStyleIndex;
}

UHitTickerStyle* UHitTickerSubsystem::GetStyleByName(FName StyleName) const
{
	if (const int32* Found = StyleByName.Find(StyleName))
	{
		return Styles[*Found];
	}
	return nullptr;
}

TArray<FName> UHitTickerSubsystem::GetStyleNames() const
{
	TArray<FName> Result;
	Result.Reserve(Styles.Num());
	for (const TObjectPtr<UHitTickerStyle>& Style : Styles)
	{
		if (Style)
		{
			Result.Add(Style->StyleName.IsNone() ? Style->GetFName() : Style->StyleName);
		}
	}
	return Result;
}

// -------------------------------------------------------------------------------------------------------
// Spawning
// -------------------------------------------------------------------------------------------------------

namespace
{
	/** Target and style folded into one key. Never zero — zero means "this entry does not merge". */
	static uint32 MakeMergeKey(const AActor* Target, int32 StyleIndex)
	{
		uint32 Key = HashCombine(GetTypeHash(TObjectKey<AActor>(Target)), static_cast<uint32>(StyleIndex) + 1u);
		return Key == 0 ? 1u : Key;
	}
}

int32 UHitTickerSubsystem::AllocateSlot()
{
	const int32 Capacity = Ring.Num();
	check(Capacity > 0);

	int32 Slot;
	if (RingCount < Capacity)
	{
		Slot = (RingHead + RingCount) % Capacity;
		++RingCount;
	}
	else
	{
		// Full. Overwrite the oldest entry and move the head on. The alternative — refusing the new hit —
		// would mean the player stops seeing the damage they are dealing right now, which is the one number
		// they actually care about.
		Slot = RingHead;
		ForgetMergeKey(Slot);
		RingHead = (RingHead + 1) % Capacity;
	}

	Stats.BufferHighWater = FMath::Max(Stats.BufferHighWater, RingCount);
	return Slot;
}

void UHitTickerSubsystem::ForgetMergeKey(int32 Slot)
{
	const uint32 Key = Ring[Slot].MergeKey;
	if (Key == 0)
	{
		return;
	}

	if (const int32* Found = MergeLookup.Find(Key))
	{
		if (*Found == Slot)
		{
			MergeLookup.Remove(Key);
		}
	}
}

int32 UHitTickerSubsystem::TryMerge(uint32 MergeKey, AActor* Target, float Value, const FVector& WorldLocation)
{
	const int32* Found = MergeLookup.Find(MergeKey);
	if (!Found)
	{
		return INDEX_NONE;
	}

	const int32 Slot = *Found;
	if (!Ring.IsValidIndex(Slot))
	{
		MergeLookup.Remove(MergeKey);
		return INDEX_NONE;
	}

	FHitTickerEntry& Entry = Ring[Slot];

	// The slot may have been recycled by the ring since the key was added, so everything is re-checked.
	if (!Entry.IsAlive() || Entry.MergeKey != MergeKey || Entry.Target.Get() != Target || Entry.Age >= MergeWindow)
	{
		MergeLookup.Remove(MergeKey);
		return INDEX_NONE;
	}

	Entry.Value += Value;
	Entry.MergeCount = static_cast<uint16>(FMath::Min<int32>(Entry.MergeCount + 1, MAX_uint16));
	Entry.Flags |= EHitTickerEntryFlag::Merged;
	Entry.WorldOrigin = WorldLocation;
	if (Target)
	{
		Entry.TargetOffset = WorldLocation - Target->GetActorLocation();
	}

	// Resetting the age is what makes the merge window a sliding one: as long as hits keep arriving inside
	// MergeWindow of the last one, this stays a single number that counts up and holds near the target, and
	// it flies off only once the hits stop. It also replays the pop for free, because the pop is the first
	// fraction of the scale curve. Thirty hits a second read as a growing total instead of a wall of digits.
	Entry.Age = 0.0f;

	++Stats.Merged;
	INC_DWORD_STAT(STAT_HitTickerMerged);

	return Slot;
}

int32 UHitTickerSubsystem::AddNumber(FVector WorldLocation, float Value, FName StyleName, AActor* Target)
{
	if (Ring.Num() == 0)
	{
		return INDEX_NONE;
	}

	const int32 StyleIndex = FindStyleIndex(StyleName);
	if (StyleIndex == INDEX_NONE || !Styles.IsValidIndex(StyleIndex) || !Styles[StyleIndex])
	{
		return INDEX_NONE;
	}

	const UHitTickerStyle* Style = Styles[StyleIndex];

	uint32 MergeKey = 0;
	if (bMergeEnabled && Style->bMergeSameTarget && Target)
	{
		MergeKey = MakeMergeKey(Target, StyleIndex);

		const int32 MergedSlot = TryMerge(MergeKey, Target, Value, WorldLocation);
		if (MergedSlot != INDEX_NONE)
		{
			return MergedSlot;
		}
	}

	const int32 Slot = AllocateSlot();

	FHitTickerEntry& Entry = Ring[Slot];
	Entry = FHitTickerEntry();
	Entry.WorldOrigin = WorldLocation;
	Entry.Value = Value;
	Entry.Age = 0.0f;
	Entry.Lifetime = FMath::Max(Style->Lifetime, 0.05f);
	Entry.StyleIndex = static_cast<uint8>(StyleIndex);
	Entry.MergeCount = 1;
	Entry.Seed = ++SpawnCounter;
	Entry.Target = Target;
	Entry.MergeKey = MergeKey;

	if (HitTickerDraw::SeedToUnitFloat(Entry.Seed, 0x9e37u) < 0.5f)
	{
		Entry.Flags |= EHitTickerEntryFlag::DriftLeft;
	}

	if (Target)
	{
		Entry.TargetOffset = WorldLocation - Target->GetActorLocation();
	}

	if (MergeKey != 0)
	{
		MergeLookup.Add(MergeKey, Slot);
	}

	return Slot;
}

int32 UHitTickerSubsystem::AddDamage(FVector WorldLocation, float Value, AActor* Target)
{
	return AddNumber(WorldLocation, Value, HitTickerNames::Damage, Target);
}

int32 UHitTickerSubsystem::AddHeal(FVector WorldLocation, float Value, AActor* Target)
{
	const int32 Slot = AddNumber(WorldLocation, Value, HitTickerNames::Heal, Target);
	if (Ring.IsValidIndex(Slot))
	{
		Ring[Slot].Flags |= EHitTickerEntryFlag::Heal;
	}
	return Slot;
}

int32 UHitTickerSubsystem::AddCrit(FVector WorldLocation, float Value, AActor* Target)
{
	const int32 Slot = AddNumber(WorldLocation, Value, HitTickerNames::Crit, Target);
	if (Ring.IsValidIndex(Slot))
	{
		Ring[Slot].Flags |= EHitTickerEntryFlag::Crit;
	}
	return Slot;
}

int32 UHitTickerSubsystem::AddDamageOverTime(FVector WorldLocation, float Value, AActor* Target)
{
	const int32 Slot = AddNumber(WorldLocation, Value, HitTickerNames::DamageOverTime, Target);
	if (Ring.IsValidIndex(Slot))
	{
		Ring[Slot].Flags |= EHitTickerEntryFlag::OverTime;
	}
	return Slot;
}

int32 UHitTickerSubsystem::AddText(FVector WorldLocation, FName Text, FName StyleName, AActor* Target)
{
	const int32 Slot = AddNumber(WorldLocation, 0.0f, StyleName, Target);
	if (Ring.IsValidIndex(Slot))
	{
		FHitTickerEntry& Entry = Ring[Slot];
		Entry.Text = Text;
		Entry.Flags |= EHitTickerEntryFlag::TextOnly;
	}
	return Slot;
}

void UHitTickerSubsystem::ClearAll()
{
	// The ring keeps its memory; only the contents go. Clearing must never give a slot back to the allocator.
	for (FHitTickerEntry& Entry : Ring)
	{
		Entry = FHitTickerEntry();
	}

	RingHead = 0;
	RingCount = 0;
	MergeLookup.Reset();
	DrawCandidates.Reset();

	Stats.Live = 0;
	Stats.DrawnThisFrame = 0;
	Stats.Merged = 0;
	Stats.DroppedByBudget = 0;
	Stats.CulledByDistance = 0;
}

int32 UHitTickerSubsystem::ClearForTarget(AActor* Target)
{
	if (!Target || Ring.Num() == 0)
	{
		return 0;
	}

	const int32 Capacity = Ring.Num();
	int32 Removed = 0;

	for (int32 Index = 0; Index < RingCount; ++Index)
	{
		const int32 Slot = (RingHead + Index) % Capacity;
		FHitTickerEntry& Entry = Ring[Slot];
		if (Entry.IsAlive() && Entry.Target.Get() == Target)
		{
			ForgetMergeKey(Slot);
			Entry.Lifetime = 0.0f;
			Entry.MergeKey = 0;
			Entry.Target.Reset();
			++Removed;
		}
	}

	return Removed;
}

// -------------------------------------------------------------------------------------------------------
// Tick
// -------------------------------------------------------------------------------------------------------

void UHitTickerSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AgeEntries(DeltaTime);
	UpdateStress(DeltaTime);

	SET_DWORD_STAT(STAT_HitTickerLive, Stats.Live);
}

void UHitTickerSubsystem::AgeEntries(float DeltaTime)
{
	const int32 Capacity = Ring.Num();
	if (Capacity == 0 || RingCount == 0)
	{
		Stats.Live = 0;
		return;
	}

	int32 LiveCount = 0;

	for (int32 Index = 0; Index < RingCount; ++Index)
	{
		const int32 Slot = (RingHead + Index) % Capacity;
		FHitTickerEntry& Entry = Ring[Slot];

		if (Entry.Lifetime <= 0.0f)
		{
			continue;
		}

		Entry.Age += DeltaTime;

		if (Entry.Age >= Entry.Lifetime)
		{
			ForgetMergeKey(Slot);
			Entry.Lifetime = 0.0f;
			Entry.MergeKey = 0;
			Entry.Target.Reset();
		}
		else
		{
			++LiveCount;
		}
	}

	// Entries of different styles have different lifetimes, so they do not always die in the order they
	// were born. Dead entries in the middle simply sit there costing nothing until the head reaches them.
	while (RingCount > 0 && Ring[RingHead].Lifetime <= 0.0f)
	{
		RingHead = (RingHead + 1) % Capacity;
		--RingCount;
	}

	Stats.Live = LiveCount;
}

void UHitTickerSubsystem::UpdateStress(float DeltaTime)
{
	if (StressRate <= 0.0f)
	{
		StressAccumulator = 0.0f;
		return;
	}

	if (!bHasCachedView)
	{
		// Nothing has drawn yet, so there is no camera to fire at. Wait a frame.
		return;
	}

	StressAccumulator += StressRate * DeltaTime;

	int32 Count = FMath::FloorToInt(StressAccumulator);
	if (Count <= 0)
	{
		return;
	}

	StressAccumulator -= static_cast<float>(Count);

	// No point spawning more numbers in one frame than the ring can hold; they would only overwrite
	// each other before anything ever drew them.
	Count = FMath::Min(Count, Ring.Num());

	const FVector Center = CachedViewLocation + CachedViewForward * StressDistance;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector Position = Center + FMath::VRand() * FMath::FRandRange(0.0f, StressRadius);
		const float Roll = FMath::FRand();

		if (Roll < 0.06f)
		{
			AddText(Position, FName(TEXT("MISS")), HitTickerNames::Miss, nullptr);
		}
		else if (Roll < 0.12f)
		{
			AddText(Position, FName(TEXT("BLOCK")), HitTickerNames::Block, nullptr);
		}
		else if (Roll < 0.22f)
		{
			AddHeal(Position, FMath::FRandRange(20.0f, 90.0f), nullptr);
		}
		else if (Roll < 0.38f)
		{
			AddCrit(Position, FMath::FRandRange(200.0f, 1200.0f), nullptr);
		}
		else
		{
			AddDamage(Position, FMath::FRandRange(8.0f, 180.0f), nullptr);
		}
	}
}

// -------------------------------------------------------------------------------------------------------
// Knobs
// -------------------------------------------------------------------------------------------------------

void UHitTickerSubsystem::SetBudget(int32 InMaxDrawnPerFrame)
{
	MaxDrawnPerFrame = FMath::Max(0, InMaxDrawnPerFrame);
}

void UHitTickerSubsystem::SetMaxDrawDistance(float InMaxDrawDistance)
{
	MaxDrawDistance = FMath::Max(0.0f, InMaxDrawDistance);
	MaxDrawDistanceSq = MaxDrawDistance * MaxDrawDistance;
}

void UHitTickerSubsystem::SetMergeEnabled(bool bInMergeEnabled)
{
	bMergeEnabled = bInMergeEnabled;
	if (!bMergeEnabled)
	{
		MergeLookup.Reset();
	}
}

void UHitTickerSubsystem::SetMergeWindow(float InMergeWindow)
{
	MergeWindow = FMath::Max(0.0f, InMergeWindow);
}

void UHitTickerSubsystem::SetStressRate(float NumbersPerSecond)
{
	StressRate = FMath::Max(0.0f, NumbersPerSecond);
	StressAccumulator = 0.0f;
}

// -------------------------------------------------------------------------------------------------------
// Drawing
// -------------------------------------------------------------------------------------------------------

void UHitTickerSubsystem::DrawNumbers(UCanvas* Canvas)
{
	if (!Canvas)
	{
		return;
	}

	// Remembering the frame lets the editor preview stand aside whenever a real HUD has already drawn.
	LastGameDrawFrame = GFrameCounter;

	DrawPass(Canvas);
}

void UHitTickerSubsystem::DrawPass(UCanvas* Canvas)
{
	SCOPE_CYCLE_COUNTER(STAT_HitTickerDraw);

	const double StartSeconds = FPlatformTime::Seconds();

	Stats.DrawnThisFrame = 0;
	Stats.DroppedByBudget = 0;
	Stats.CulledByDistance = 0;
	DrawCandidates.Reset();

	FVector ViewLocation = CachedViewLocation;
	FVector ViewForward = CachedViewForward;
	if (ResolveView(Canvas, ViewLocation, ViewForward))
	{
		CachedViewLocation = ViewLocation;
		CachedViewForward = ViewForward;
		bHasCachedView = true;
	}

	if (RingCount > 0 && bHasCachedView && Canvas->SceneView)
	{
		GatherCandidates(Canvas, ViewLocation, ViewForward);

		// Importance order: near before far, crit before normal, young before old. The budget cuts from
		// the back of this list, so what a player loses first is the number they were least likely to read.
		DrawCandidates.Sort([](const FDrawCandidate& A, const FDrawCandidate& B) { return A.Score > B.Score; });

		if (DrawCandidates.Num() > MaxDrawnPerFrame)
		{
			Stats.DroppedByBudget = DrawCandidates.Num() - MaxDrawnPerFrame;
			DrawCandidates.SetNum(MaxDrawnPerFrame, EAllowShrinking::No);
		}

		ResolveOverlaps(Canvas);

		// Back to front, so the most important number ends up on top of the pile.
		for (int32 Index = DrawCandidates.Num() - 1; Index >= 0; --Index)
		{
			DrawCandidate(Canvas, DrawCandidates[Index]);
		}

		Stats.DrawnThisFrame = DrawCandidates.Num();
		INC_DWORD_STAT_BY(STAT_HitTickerDrawn, Stats.DrawnThisFrame);
	}

	if (bShowStats)
	{
		DrawStatsBox(Canvas);
	}

	Stats.DrawMs = static_cast<float>((FPlatformTime::Seconds() - StartSeconds) * 1000.0);
}

bool UHitTickerSubsystem::ResolveView(UCanvas* Canvas, FVector& OutLocation, FVector& OutForward) const
{
	if (Canvas && Canvas->SceneView)
	{
		OutLocation = Canvas->SceneView->ViewMatrices.GetViewOrigin();
		OutForward = Canvas->SceneView->GetViewDirection();
		return true;
	}

	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (const APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
			{
				OutLocation = CameraManager->GetCameraLocation();
				OutForward = CameraManager->GetCameraRotation().Vector();
				return true;
			}
		}
	}

	return false;
}

void UHitTickerSubsystem::GatherCandidates(UCanvas* Canvas, const FVector& ViewLocation, const FVector& ViewForward)
{
	const int32 Capacity = Ring.Num();
	const float ViewportX = static_cast<float>(Canvas->SizeX);
	const float ViewportY = static_cast<float>(Canvas->SizeY);

	// A number whose anchor is a little outside the viewport can still be partly visible once the arc has
	// carried it; a number a long way outside cannot.
	constexpr float ScreenMargin = 250.0f;

	for (int32 Index = 0; Index < RingCount; ++Index)
	{
		const int32 Slot = (RingHead + Index) % Capacity;
		FHitTickerEntry& Entry = Ring[Slot];

		if (!Entry.IsAlive() || !Styles.IsValidIndex(Entry.StyleIndex) || !Styles[Entry.StyleIndex])
		{
			continue;
		}

		const UHitTickerStyle* Style = Styles[Entry.StyleIndex];

		// A target that was destroyed while its numbers were still flying resolves to null here and the
		// number simply finishes its life at the last world point it knew about. No crash, no dangling actor.
		if (const AActor* Target = Entry.Target.Get())
		{
			Entry.WorldOrigin = Target->GetActorLocation() + Entry.TargetOffset;
		}

		const FVector ToNumber = Entry.WorldOrigin - ViewLocation;

		// Behind the camera: not projected, not sorted, not drawn. The cheapest test comes first.
		if (FVector::DotProduct(ToNumber, ViewForward) <= 10.0)
		{
			++Stats.CulledByDistance;
			continue;
		}

		const double DistanceSq = ToNumber.SizeSquared();
		if (MaxDrawDistanceSq > 0.0f && DistanceSq > static_cast<double>(MaxDrawDistanceSq))
		{
			++Stats.CulledByDistance;
			continue;
		}

		const FVector Projected = Canvas->Project(Entry.WorldOrigin);

		const float Alpha = Entry.GetAlpha();
		const float DriftSign = Entry.HasFlag(EHitTickerEntryFlag::DriftLeft) ? -1.0f : 1.0f;

		// Everything below is a pure function of the age: no integration, no per-entry velocity, nothing
		// that a dropped frame could desynchronise.
		FVector2D ScreenPos(Projected.X, Projected.Y);
		ScreenPos += Style->EvaluateArc(Alpha, DriftSign);
		ScreenPos += Entry.ScreenOffset;

		if (Style->SpawnScatter > 0.0f)
		{
			ScreenPos.X += (HitTickerDraw::SeedToUnitFloat(Entry.Seed, 0x51edu) - 0.5f) * Style->SpawnScatter;
			ScreenPos.Y += (HitTickerDraw::SeedToUnitFloat(Entry.Seed, 0x1b83u) - 0.5f) * Style->SpawnScatter * 0.5f;
		}

		if (ScreenPos.X < -ScreenMargin || ScreenPos.X > ViewportX + ScreenMargin ||
			ScreenPos.Y < -ScreenMargin || ScreenPos.Y > ViewportY + ScreenMargin)
		{
			++Stats.CulledByDistance;
			continue;
		}

		FDrawCandidate Candidate;
		Candidate.Slot = Slot;
		Candidate.ScreenPos = ScreenPos;
		Candidate.Scale = Style->EvaluateScale(Alpha, Entry.Value);

		float Score = Style->ImportanceBias;
		Score += Entry.HasFlag(EHitTickerEntryFlag::Crit) ? 60.0f : 0.0f;
		Score += Entry.MergeCount > 1 ? 15.0f : 0.0f;
		Score += 40.0f * (1.0f - Alpha);
		if (MaxDrawDistanceSq > 0.0f)
		{
			const float DistanceRatio = FMath::Sqrt(static_cast<float>(DistanceSq)) / FMath::Max(MaxDrawDistance, 1.0f);
			Score += 80.0f * (1.0f - FMath::Clamp(DistanceRatio, 0.0f, 1.0f));
		}
		Candidate.Score = Score;

		DrawCandidates.Add(Candidate);
	}
}

void UHitTickerSubsystem::ResolveOverlaps(UCanvas* Canvas)
{
	if (!bAvoidOverlap || DrawCandidates.Num() == 0)
	{
		return;
	}

	const FIntPoint ViewportSize(Canvas->SizeX, Canvas->SizeY);
	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return;
	}

	// The grid is only resized when the viewport is, which is never during play. Everything else is a memset.
	if (ViewportSize != AvoidanceViewportSize)
	{
		AvoidanceViewportSize = ViewportSize;
		AvoidanceGridDims.X = FMath::Max(1, FMath::CeilToInt(static_cast<float>(ViewportSize.X) / AvoidanceCellSize.X));
		AvoidanceGridDims.Y = FMath::Max(1, FMath::CeilToInt(static_cast<float>(ViewportSize.Y) / AvoidanceCellSize.Y));
		AvoidanceGrid.SetNumUninitialized(AvoidanceGridDims.X * AvoidanceGridDims.Y, EAllowShrinking::No);
	}

	FMemory::Memzero(AvoidanceGrid.GetData(), AvoidanceGrid.Num());

	auto CellIndexOf = [this](const FVector2D& Position, FIntPoint& OutCell) -> bool
	{
		OutCell.X = FMath::FloorToInt(Position.X / AvoidanceCellSize.X);
		OutCell.Y = FMath::FloorToInt(Position.Y / AvoidanceCellSize.Y);
		return OutCell.X >= 0 && OutCell.X < AvoidanceGridDims.X && OutCell.Y >= 0 && OutCell.Y < AvoidanceGridDims.Y;
	};

	// Pass one: numbers that already own a dodge keep it and claim their cell, so newcomers move around
	// them instead of the other way round. A dodge that is recomputed every frame is a number that shakes.
	for (const FDrawCandidate& Candidate : DrawCandidates)
	{
		const FHitTickerEntry& Entry = Ring[Candidate.Slot];
		if (!Entry.HasFlag(EHitTickerEntryFlag::DodgeResolved))
		{
			continue;
		}

		FIntPoint Cell;
		if (CellIndexOf(Candidate.ScreenPos, Cell))
		{
			AvoidanceGrid[Cell.Y * AvoidanceGridDims.X + Cell.X] = 1;
		}
	}

	// Pass two: everyone else, most important first, so the number worth reading gets the spot it asked for.
	for (FDrawCandidate& Candidate : DrawCandidates)
	{
		FHitTickerEntry& Entry = Ring[Candidate.Slot];
		if (Entry.HasFlag(EHitTickerEntryFlag::DodgeResolved))
		{
			continue;
		}

		const UHitTickerStyle* Style = Styles[Entry.StyleIndex];
		if (!Style->bAvoidOverlap)
		{
			Entry.Flags |= EHitTickerEntryFlag::DodgeResolved;
			continue;
		}

		FIntPoint BaseCell;
		if (!CellIndexOf(Candidate.ScreenPos, BaseCell))
		{
			Entry.Flags |= EHitTickerEntryFlag::DodgeResolved;
			continue;
		}

		bool bPlaced = false;

		// Rings of increasing radius around the wanted cell, upwards first: a number pushed up still reads
		// as belonging to the hit underneath it, one pushed sideways reads as a different hit.
		for (int32 Radius = 0; Radius <= AvoidanceSearchRadius && !bPlaced; ++Radius)
		{
			for (int32 OffsetY = -Radius; OffsetY <= Radius && !bPlaced; ++OffsetY)
			{
				for (int32 OffsetX = -Radius; OffsetX <= Radius && !bPlaced; ++OffsetX)
				{
					if (FMath::Max(FMath::Abs(OffsetX), FMath::Abs(OffsetY)) != Radius)
					{
						continue;
					}

					const FIntPoint Cell(BaseCell.X + OffsetX, BaseCell.Y + OffsetY);
					if (Cell.X < 0 || Cell.X >= AvoidanceGridDims.X || Cell.Y < 0 || Cell.Y >= AvoidanceGridDims.Y)
					{
						continue;
					}

					uint8& Occupancy = AvoidanceGrid[Cell.Y * AvoidanceGridDims.X + Cell.X];
					if (Occupancy != 0)
					{
						continue;
					}

					Occupancy = 1;

					const FVector2D Dodge(
						static_cast<float>(OffsetX * AvoidanceCellSize.X),
						static_cast<float>(OffsetY * AvoidanceCellSize.Y));

					// Sticky for the rest of this entry's life: it travels with the arc from here on.
					Entry.ScreenOffset += Dodge;
					Candidate.ScreenPos += Dodge;
					bPlaced = true;
				}
			}
		}

		Entry.Flags |= EHitTickerEntryFlag::DodgeResolved;
	}
}

void UHitTickerSubsystem::DrawCandidate(UCanvas* Canvas, const FDrawCandidate& Candidate)
{
	const FHitTickerEntry& Entry = Ring[Candidate.Slot];
	const UHitTickerStyle* Style = Styles[Entry.StyleIndex];

	const float Alpha = Entry.GetAlpha();
	const FLinearColor Color = Style->EvaluateColor(Alpha);

	FLinearColor OutlineColor = Style->OutlineColor;
	OutlineColor.A *= Color.A;

	const FSlateFontInfo FontInfo = Style->GetFontInfo(Candidate.Scale, OutlineColor);

	// Formatted into a stack buffer and handed to the canvas as a view. Nothing on this path allocates.
	TStringBuilder<64> Builder;
	if (Entry.HasFlag(EHitTickerEntryFlag::TextOnly))
	{
		Entry.Text.AppendString(Builder);
	}
	else
	{
		Style->AppendValueText(Entry.Value, Builder);
	}

	const FStringView TextView = Builder.ToView();
	if (TextView.IsEmpty())
	{
		return;
	}

	const FVector2D TextSize = HitTickerDraw::MeasureText(TextView, FontInfo);
	const FVector2D TextPosition = Candidate.ScreenPos - TextSize * 0.5f;

	if (UTexture2D* Icon = Style->Icon)
	{
		if (FTextureResource* Resource = Icon->GetResource())
		{
			const float IconSize = FMath::Max(Style->IconSize * Candidate.Scale, 1.0f);
			FLinearColor IconColor = Style->IconColor;
			IconColor.A *= Color.A;

			FCanvasTileItem IconItem(
				FVector2D(TextPosition.X - IconSize - 4.0f, Candidate.ScreenPos.Y - IconSize * 0.5f),
				Resource,
				FVector2D(IconSize, IconSize),
				IconColor);
			IconItem.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(IconItem);
		}
	}

	FCanvasTextStringViewItem TextItem(TextPosition, TextView, FontInfo, Color);
	if (Style->bDrawShadow)
	{
		FLinearColor ShadowColor = Style->ShadowColor;
		ShadowColor.A *= Color.A;
		TextItem.EnableShadow(ShadowColor, Style->ShadowOffset * Candidate.Scale);
	}

	Canvas->DrawItem(TextItem);
}

void UHitTickerSubsystem::DrawStatsBox(UCanvas* Canvas) const
{
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font)
	{
		return;
	}

	constexpr float BoxX = 24.0f;
	constexpr float BoxY = 90.0f;
	constexpr float BoxWidth = 268.0f;
	constexpr float LineHeight = 15.0f;
	constexpr int32 LineCount = 8;

	FCanvasTileItem Background(
		FVector2D(BoxX - 8.0f, BoxY - 8.0f),
		GWhiteTexture,
		FVector2D(BoxWidth, LineCount * LineHeight + 16.0f),
		FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	Background.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Background);

	float LineY = BoxY;

	auto DrawLine = [&](FStringView Line, const FLinearColor& Color)
	{
		FCanvasTextStringViewItem Item(FVector2D(BoxX, LineY), Line, Font, Color);
		Canvas->DrawItem(Item);
		LineY += LineHeight;
	};

	const FLinearColor Heading(0.55f, 0.85f, 1.0f, 1.0f);
	const FLinearColor Body(0.9f, 0.9f, 0.9f, 1.0f);

	TStringBuilder<128> Line;

	Line.Reset();
	Line.Append(TEXT("HitTicker"));
	DrawLine(Line.ToView(), Heading);

	Line.Reset();
	Line.Appendf(TEXT("Live              %d / %d"), Stats.Live, Ring.Num());
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Drawn this frame  %d / %d"), Stats.DrawnThisFrame, MaxDrawnPerFrame);
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Dropped by budget %d"), Stats.DroppedByBudget);
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Culled            %d"), Stats.CulledByDistance);
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Merged            %d  (merge %s)"), Stats.Merged, bMergeEnabled ? TEXT("on") : TEXT("off"));
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Buffer high water %d"), Stats.BufferHighWater);
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Draw              %.3f ms   stress %.0f/s"), Stats.DrawMs, StressRate);
	DrawLine(Line.ToView(), Body);
}

#if WITH_EDITOR
void UHitTickerSubsystem::OnEditorPreviewDraw(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (!Canvas || !UHitTickerSettings::Get().bEnableEditorPreview)
	{
		return;
	}

	// A real HUD pass has already drawn this frame — that one wins, always.
	if (LastGameDrawFrame == GFrameCounter)
	{
		return;
	}

	// The debug draw service is engine-wide: every world's ticker is called for every viewport that draws.
	// Work out which world this viewport is showing and stay out of the other ones.
	UWorld* ViewWorld = nullptr;
	if (Canvas->SceneView && Canvas->SceneView->Family && Canvas->SceneView->Family->Scene)
	{
		ViewWorld = Canvas->SceneView->Family->Scene->GetWorld();
	}
	if (!ViewWorld && PlayerController)
	{
		ViewWorld = PlayerController->GetWorld();
	}

	if (ViewWorld && ViewWorld != GetWorld())
	{
		return;
	}

	DrawPass(Canvas);
}
#endif
