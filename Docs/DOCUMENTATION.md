# HitTicker — Documentation

**Floating damage numbers for games that produce a lot of them.** No widget per hit, no allocation while
the game runs, one canvas pass per frame, and a hard budget you set.

---

## Contents

1. [At a glance](#1-at-a-glance)
2. [Installation](#2-installation)
3. [Quick start — five minutes](#3-quick-start--five-minutes)
4. [Class and API overview](#4-class-and-api-overview)
5. [Code examples](#5-code-examples)
6. [Styles](#6-styles)
7. [Project settings](#7-project-settings)
8. [Merging — what keeps it readable](#8-merging--what-keeps-it-readable)
9. [Budget, culling and overlap avoidance](#9-budget-culling-and-overlap-avoidance)
10. [Editor preview (no PIE)](#10-editor-preview-no-pie)
11. [Fonts](#11-fonts)
12. [Console commands](#12-console-commands)
13. [Behaviour under load](#13-behaviour-under-load)
14. [Troubleshooting](#14-troubleshooting)
15. [Limits — what HitTicker is not](#15-limits--what-hitticker-is-not)
16. [The demo map](#16-the-demo-map)
17. [Support](#17-support)

---

## 1. At a glance

| | |
|---|---|
| **Engine version** | Unreal Engine **5.8** (`"EngineVersion": "5.8.0"`) |
| **Type** | C++ code plugin with a complete Blueprint API |
| **Modules** | One runtime module (`HitTicker`, `LoadingPhase: PreDefault`). No editor module. |
| **Supported platforms** | **Win64** (`PlatformAllowList` in the `.uplugin`) |
| **Build configurations** | Debug, Development, **Shipping** — the draw path is `AHUD::DrawHUD`, which survives cooking |
| **Module dependencies** | `Core`, `CoreUObject`, `Engine`, `DeveloperSettings`, `SlateCore` (public), `RenderCore` (private) |
| **Third-party code** | None |
| **UMG dependency** | None — drawing is `UCanvas`, not Slate widgets |
| **Network replication** | None — display only, per client |
| **Included content** | Demo map, demo game mode, combat director, drifting target, click-driven control panel, six style assets, arena materials |

> **Other platforms.** The module has no platform-specific code; if you need Android, iOS or a console,
> add the platform to `PlatformAllowList` in `HitTicker.uplugin` and rebuild. Only the three above are
> shipped as verified.

---

## 2. Installation

### 2.1 As a project plugin (recommended)

1. Close the editor.
2. Copy the `HitTicker` folder into `<YourProject>/Plugins/` so you end up with
   `<YourProject>/Plugins/HitTicker/HitTicker.uplugin`.
3. Right-click your `.uproject` ▸ **Generate Visual Studio project files** (Windows) or
   **Generate Xcode project** (macOS). Skip this step if your project is Blueprint-only — the editor
   will offer to build the module for you on the next start.
4. Start the editor and confirm the rebuild prompt, or build the project from your IDE.
5. **Edit ▸ Plugins ▸ UI ▸ HitTicker** — make sure it is enabled. Restart if asked.

### 2.2 As an engine plugin

Copy the folder to `<UE_5.8>/Engine/Plugins/Marketplace/HitTicker/` instead. Everything else is the same.
The plugin is then available to every project on that engine install.

### 2.3 Blueprint-only projects

HitTicker works in a Blueprint-only project. The editor will ask to compile the plugin module once on
first start; after that everything is reachable from Blueprint — the function library, the HUD class, the
HUD component, the settings and the style data assets. You never have to write C++ to use it.

---

## 3. Quick start — five minutes

### Step 1 — Give the numbers a canvas

The numbers are drawn onto `UCanvas` from the HUD. Pick **one** of the two ways in. Both end in the same
subsystem call; neither is a fallback for the other.

**A — You do not have your own HUD class yet**

Open your Game Mode and set **HUD Class** to **HitTicker HUD** (`AHitTickerHUD`). That is the whole
hook-up. Nothing else to do.

**B — You already have your own HUD class (the usual case)**

Do **not** replace it. Add the **HitTicker HUD Component** to your HUD actor and call `DrawNumbers` from
your own draw:

```cpp
// AMyHUD.h
#include "HitTickerHUDComponent.h"

UPROPERTY(VisibleAnywhere, Category = "HUD")
TObjectPtr<UHitTickerHUDComponent> HitTickerComponent;

// AMyHUD.cpp
AMyHUD::AMyHUD()
{
    HitTickerComponent = CreateDefaultSubobject<UHitTickerHUDComponent>(TEXT("HitTicker"));
}

void AMyHUD::DrawHUD()
{
    Super::DrawHUD();
    HitTickerComponent->DrawNumbers(Canvas);   // one line — that is the whole integration
}
```

In a **Blueprint HUD**: Add Component ▸ *HitTicker HUD Component*, then on **Event Receive Draw HUD**
call **Draw Numbers** on it and plug the HUD's own **Canvas** into the Canvas pin.

You can also skip the component entirely and call `UHitTickerSubsystem::DrawNumbers(Canvas)` yourself —
the component only exists so that there is an obvious, discoverable place to hang the call.

### Step 2 — Show a number

From anywhere your game already applies damage. Blueprint:

> **Add Damage Number** — *World Location*, *Damage*, *Target* (optional, under Advanced)

C++:

```cpp
#include "HitTickerStatics.h"

UHitTickerStatics::AddDamageNumber(this, Hit.ImpactPoint, DamageAmount, DamagedActor);
```

### Step 3 — Pass the target actor

**Always pass the target actor when you have one.** It is what lets repeat hits merge into one
counting-up number, and what makes a number ride along with a moving enemy. Everything still works
without it — you just get one separate number per hit, anchored to a fixed world point.

That is the installation. Everything below is tuning.

---

## 4. Class and API overview

| Class | Base | What it is for |
|---|---|---|
| `UHitTickerSubsystem` | `UTickableWorldSubsystem` | The ticker itself. One per world. Owns the ring buffer, ages entries, merges, projects, budgets and draws. Everything else in the plugin is a way of reaching it. |
| `UHitTickerStatics` | `UBlueprintFunctionLibrary` | Blueprint/C++ shortcuts so an ability graph never spells out the subsystem lookup. Every call is a null-safe no-op when the world has no ticker (a dedicated server, for instance). |
| `UHitTickerStyle` | `UPrimaryDataAsset` | How one kind of number looks and moves: font, colour over life, pop curve, arc, outline, prefix/suffix, lifetime, merging, importance. |
| `UHitTickerSettings` | `UDeveloperSettings` | Project-wide defaults. **Project Settings ▸ Plugins ▸ HitTicker**, stored in `DefaultGame.ini`. |
| `AHitTickerHUD` | `AHUD` | Ready-made HUD class. Set it as your Game Mode's HUD Class and numbers draw. |
| `UHitTickerHUDComponent` | `UActorComponent` | The way in for projects with their own HUD class. One public function: `DrawNumbers(Canvas)`. No tick, no state. |
| `FHitTickerStats` | `USTRUCT`, `BlueprintType` | What the ticker did: live, drawn, merged, dropped, culled, draw milliseconds, buffer high water. |
| `FHitTickerEntry` | plain struct (not exposed) | One floating number. No `UObject`, no widget, no shared pointer. |

### 4.1 `UHitTickerStatics` — the Blueprint API

| Function | Signature | Notes |
|---|---|---|
| **Get HitTicker** | `UHitTickerSubsystem* GetHitTicker(WorldContext)` | The subsystem for that world, or null. |
| **Add Damage Number** | `(WorldContext, FVector Location, float Damage, AActor* Target = nullptr)` | Style `Damage`. |
| **Add Heal Number** | `(WorldContext, FVector Location, float Healing, AActor* Target = nullptr)` | Style `Heal`. |
| **Add Crit Number** | `(WorldContext, FVector Location, float Damage, AActor* Target = nullptr)` | Style `Crit`. Crits outrank normal hits when the budget bites. |
| **Add Damage Over Time Number** | `(WorldContext, FVector Location, float Damage, AActor* Target = nullptr)` | Style `DamageOverTime`. |
| **Add Combat Text** | `(WorldContext, FVector Location, FName Text, FName StyleName, AActor* Target = nullptr)` | Words instead of a number: MISS, BLOCK, IMMUNE. |
| **Add Styled Number** | `(WorldContext, FVector Location, float Value, FName StyleName, AActor* Target = nullptr)` | Any style name of your own. |
| **Get HitTicker Stats** | `FHitTickerStats GetHitTickerStats(WorldContext)` | All zero if the world has no ticker. |
| **Clear All** | `(WorldContext)` | Drop every number on screen. |
| **Clear For Target** | `(WorldContext, AActor* Target)` | Drop every number belonging to one actor, e.g. when it despawns. |
| **Set Budget** | `(WorldContext, int32 MaxDrawnPerFrame)` | |
| **Set Merge Enabled** | `(WorldContext, bool bMergeEnabled)` | |
| **Set Show Stats** | `(WorldContext, bool bShowStats)` | Same as `HitTicker.Stats`. |
| **Set Stress Rate** | `(WorldContext, float NumbersPerSecond)` | Same as `HitTicker.Stress`. `0` stops it. |

### 4.2 `UHitTickerSubsystem` — the full surface

**Spawning**

```cpp
int32 AddNumber(FVector WorldLocation, float Value, FName StyleName, AActor* Target = nullptr);
int32 AddDamage(FVector WorldLocation, float Value, AActor* Target = nullptr);
int32 AddHeal(FVector WorldLocation, float Value, AActor* Target = nullptr);
int32 AddCrit(FVector WorldLocation, float Value, AActor* Target = nullptr);
int32 AddDamageOverTime(FVector WorldLocation, float Value, AActor* Target = nullptr);
int32 AddText(FVector WorldLocation, FName Text, FName StyleName, AActor* Target = nullptr);
void  ClearAll();
int32 ClearForTarget(AActor* Target);
```

Every `Add…` returns the ring slot it landed in, or `INDEX_NONE` when there was no style to draw it with.
The sign of `Value` is ignored — use the style's `Prefix` for `-` or `+`.

**Drawing**

```cpp
void DrawNumbers(UCanvas* Canvas);   // the whole renderer, one call
```

**Knobs** (all `BlueprintCallable` / `BlueprintPure`)

```cpp
void  SetBudget(int32 InMaxDrawnPerFrame);        int32 GetBudget() const;
void  SetMaxDrawDistance(float InMaxDrawDistance); float GetMaxDrawDistance() const;
void  SetMergeEnabled(bool bInMergeEnabled);       bool  IsMergeEnabled() const;
void  SetMergeWindow(float InMergeWindow);         float GetMergeWindow() const;
void  SetShowStats(bool bInShowStats);             bool  IsShowingStats() const;
void  SetStressRate(float NumbersPerSecond);       float GetStressRate() const;

const FHitTickerStats& GetStats() const;
int32 GetLiveCount() const;      // == GetStats().Live, without the copy
int32 GetCapacity() const;       // ring size for this world
UHitTickerStyle* GetStyleByName(FName StyleName) const;
TArray<FName>    GetStyleNames() const;
```

### 4.3 `FHitTickerStats`

| Field | Kind | Meaning |
|---|---|---|
| `Live` | level | Numbers alive in the ring. Can never exceed `MaxLiveNumbers` — that is the point. |
| `DrawnThisFrame` | per pass | Numbers actually drawn. Can never exceed `MaxDrawnPerFrame`. |
| `Merged` | running total | Hits folded into an existing number, since the last `ClearAll`. |
| `DroppedByBudget` | per pass | Visible numbers the budget refused, least important first. |
| `CulledByDistance` | per pass | Skipped: too far, behind the camera, off screen. Never projected, never sorted. |
| `DrawMs` | per pass | Wall-clock milliseconds inside the draw pass, projection and sorting included. |
| `BufferHighWater` | level | Highest slot count the ring ever reached. Equal to `MaxLiveNumbers` means the ring is doing its job. |

---

## 5. Code examples

### 5.1 The common case — inside `TakeDamage`

```cpp
#include "HitTickerStatics.h"

float AMyEnemy::TakeDamage(float Damage, const FDamageEvent& Event,
                           AController* Instigator, AActor* Causer)
{
    const float Applied = Super::TakeDamage(Damage, Event, Instigator, Causer);
    if (Applied <= 0.f)
    {
        return Applied;
    }

    const FVector HitPoint = GetActorLocation() + FVector(0.f, 0.f, 90.f);
    UHitTickerStatics::AddDamageNumber(this, HitPoint, Applied, this);
    return Applied;
}
```

### 5.2 Crits, heals, DoT and words

```cpp
// A critical hit — outranks normal hits when the per-frame budget bites.
UHitTickerStatics::AddCritNumber(this, Hit.ImpactPoint, Damage, DamagedActor);

// Healing.
UHitTickerStatics::AddHealNumber(this, Location, HealAmount, HealedActor);

// A poison tick. Usually a quieter style with a longer lifetime.
UHitTickerStatics::AddDamageOverTimeNumber(this, Location, TickDamage, PoisonedActor);

// Words instead of a number.
UHitTickerStatics::AddCombatText(this, Location, TEXT("MISS"),  TEXT("Miss"),  DodgingActor);
UHitTickerStatics::AddCombatText(this, Location, TEXT("BLOCK"), TEXT("Block"), BlockingActor);
```

### 5.3 A style of your own

```cpp
// "Shock" is just the StyleName on one of your UHitTickerStyle assets.
UHitTickerStatics::AddStyledNumber(this, Location, LightningDamage, TEXT("Shock"), Target);
```

### 5.4 Talking to the subsystem directly

```cpp
#include "HitTickerSubsystem.h"

if (UHitTickerSubsystem* Ticker = GetWorld()->GetSubsystem<UHitTickerSubsystem>())
{
    Ticker->SetBudget(64);              // tighter budget on a weaker machine
    Ticker->SetMergeWindow(0.4f);       // hold numbers open a little longer
    Ticker->SetShowStats(true);         // the statistics box

    const FHitTickerStats& S = Ticker->GetStats();
    UE_LOG(LogTemp, Log, TEXT("live %d  drawn %d  dropped %d  %.2f ms"),
        S.Live, S.DrawnThisFrame, S.DroppedByBudget, S.DrawMs);
}
```

### 5.5 Cleaning up when an enemy despawns

Not required — a deleted target is safe, the numbers simply finish their life at the last world point
they knew about. Do this only when you want the numbers gone *immediately*:

```cpp
UHitTickerStatics::ClearForTarget(this, DyingEnemy);
```

### 5.6 A quality setting that ties into the budget

```cpp
void UMySettingsMenu::ApplyDamageNumberDensity(int32 Density)   // 0 low, 1 medium, 2 high
{
    static constexpr int32 Budgets[] = { 24, 64, 192 };
    UHitTickerStatics::SetBudget(this, Budgets[FMath::Clamp(Density, 0, 2)]);
}
```

### 5.7 Drawing without the component

```cpp
void AMyHUD::DrawHUD()
{
    Super::DrawHUD();
    if (UHitTickerSubsystem* Ticker = GetWorld()->GetSubsystem<UHitTickerSubsystem>())
    {
        Ticker->DrawNumbers(Canvas);
    }
}
```

---

## 6. Styles

A style is a data asset (`UHitTickerStyle`), not code. Create one with **Content Browser ▸ right-click ▸
Miscellaneous ▸ Data Asset ▸ HitTickerStyle**. Change the look of every crit in your game by editing one
asset — no recompile, no Blueprint.

| Field | What it does |
|---|---|
| `StyleName` | The name gameplay code calls it by: `Damage`, `Crit`, `Heal`, `Miss`, or your own. Names, not asset references, so gameplay code never hard-references a UI asset. |
| `Font`, `FontSize` | Leave `Font` empty to fall back to the engine default Slate font. **No font ships with the plugin** — see §11, and read it, there is a trap. |
| `Prefix`, `Suffix` | `"-"`, `"+"`, `"%"`, `" dmg"`. Written around the number. |
| `DecimalPlaces` | 0 → `1234`, 1 → `1234.5`. Damage numbers are almost always 0. |
| `bAbbreviateLargeValues` | `12500` draws as `12.5k`. Keeps late-game numbers off the whole screen. |
| `BaseColor` | Base tint, multiplied by `ColorOverLife` when that curve has keys. |
| `ColorOverLife` | Optional colour curve across the entry's life. Its alpha multiplies the fade. |
| `FadeStart` | Fraction of the lifetime the number stays fully opaque before fading. Default `0.65`. |
| `OutlineSize`, `OutlineColor` | Outline in pixels. `0` disables it. An outline buys more readability on a bright wall than a bigger font does. |
| `bDrawShadow`, `ShadowOffset`, `ShadowColor` | Drop shadow, as well as or instead of the outline. |
| `Lifetime` | Seconds on screen. Default `1.1`. |
| `RiseDistance`, `DriftDistance` | Pixels travelled up and sideways over the whole life. Drift direction comes from the entry seed. |
| `RiseOverLife`, `DriftOverLife`, `ScaleOverLife` | Optional curves shaping that travel. Empty falls back to ease-out rise, linear drift, and a short pop-in. |
| `SpawnScatter` | Random pixels of scatter at spawn, so two simultaneous hits on one actor do not start on the same pixel. |
| `bMergeSameTarget` | Fold repeat hits on the same target into one counting-up number (§8). |
| `bScaleWithDamage`, `ScaleReferenceValue`, `MaxDamageScale` | Bigger hits draw bigger, capped, so one lucky `99999` does not fill the screen. |
| `bAvoidOverlap` | Take part in screen-space dodging (§9). Off means this style always draws exactly where the hit was. |
| `ImportanceBias` | Added to the score the budget sorts by. Push boss damage up so it survives the cut. |
| `Icon`, `IconSize`, `IconColor` | Optional damage-type icon left of the number. No icon ships with the plugin. |

### Registering styles

**Project Settings ▸ Plugins ▸ HitTicker ▸ Styles** — add your style assets to the list.

Any of the six standard names (`Damage`, `Crit`, `Heal`, `DamageOverTime`, `Miss`, `Block`) that you do
*not* register gets a **built-in fallback style at runtime**, so the plugin draws something sensible on a
fresh project before a single asset has been authored. Calling an unknown style name falls back to
`DefaultStyleName`.

Styles are limited to **255 per project** — the entry's style index is a `uint8`.

---

## 7. Project settings

**Project Settings ▸ Plugins ▸ HitTicker**, written to `Config/DefaultGame.ini` under
`[/Script/HitTicker.HitTickerSettings]`.

| Setting | Default | Notes |
|---|---|---|
| `MaxLiveNumbers` | 512 | Ring buffer size. Read **once**, when a world comes up; changing it takes effect on the next world load. |
| `MaxDrawnPerFrame` | 128 | Hard per-frame draw budget. Changeable at runtime. |
| `MaxDrawDistance` | 6000 | Numbers further from the camera are not even projected. |
| `MergeWindow` | 0.25 s | How long a number stays open for merging. |
| `bMergeEnabled` | true | Master switch; styles can still opt out individually. |
| `bAvoidOverlap` | true | Screen-space dodging on/off. |
| `AvoidanceCellSize` | 76 × 30 px | Occupancy grid cell — roughly one number wide and tall. |
| `AvoidanceSearchRadius` | 3 | How many neighbouring cells a number may be pushed into before it gives up. |
| `Styles` | — | Your style assets. |
| `DefaultStyleName` | `Damage` | Used when a call names a style that does not exist. |
| `bEnableEditorPreview` | true | Numbers fly in an editor viewport without PIE (§10). |
| `bShowStatsByDefault` | false | Statistics box on from the start. Same as `HitTicker.Stats 1`. |
| `StressRadius` | 900 | Radius around the camera focus that `HitTicker.Stress` scatters into. |
| `StressDistance` | 1400 | Distance in front of the camera it centres on. |

An example block, as shipped with the demo:

```ini
[/Script/HitTicker.HitTickerSettings]
+Styles=/HitTicker/HitTicker/Styles/HTS_Damage.HTS_Damage
+Styles=/HitTicker/HitTicker/Styles/HTS_Crit.HTS_Crit
+Styles=/HitTicker/HitTicker/Styles/HTS_Heal.HTS_Heal
+Styles=/HitTicker/HitTicker/Styles/HTS_DamageOverTime.HTS_DamageOverTime
+Styles=/HitTicker/HitTicker/Styles/HTS_Miss.HTS_Miss
+Styles=/HitTicker/HitTicker/Styles/HTS_Block.HTS_Block
DefaultStyleName=Damage
MaxLiveNumbers=512
MaxDrawnPerFrame=128
MaxDrawDistance=6000.000000
MergeWindow=0.250000
bMergeEnabled=True
bAvoidOverlap=True
bEnableEditorPreview=True
```

---

## 8. Merging — what keeps it readable

Thirty hits a second on one enemy, drawn honestly, is thirty numbers on top of each other and nothing a
player can read. With merging on, a hit on the same target in the same style within `MergeWindow` of the
previous one is **added to the live number** instead of taking a new slot: the value counts up, the pop
replays, and the number holds near the target until the hits stop — then it flies off and fades.

- The window is a **sliding** one. Hits arriving every 33 ms keep one number alive indefinitely.
- Merging needs a `Target`. A call with no target never merges — there is nothing to merge *on*.
- The lookup is a hash from (target, style) to ring slot, so folding a repeat hit stays O(1) at any rate.
- Per style: `bMergeSameTarget`. Globally: `HitTicker.Merge 0|1`, `SetMergeEnabled`, or the setting.
- `Miss` and `Block` do not merge by default — "MISS" counting up to 3 is not useful.

---

## 9. Budget, culling and overlap avoidance

Every frame, the draw pass:

1. walks the ring and drops anything **behind the camera**, **beyond `MaxDrawDistance`** or **off screen**
   (counted as `CulledByDistance`) — these are never projected and never sorted;
2. scores what is left: **near before far, crit before normal, young before old**, plus the style's
   `ImportanceBias`;
3. sorts by that score and **cuts everything past `MaxDrawnPerFrame`** (counted as `DroppedByBudget`);
4. resolves overlaps on a coarse occupancy grid over the viewport, most important number first. A number
   whose cell is taken is pushed into a free neighbouring cell — upwards first. The dodge is resolved
   **once**, on the first frame the entry draws, and then travels with it; recomputing it every frame
   would make the number vibrate. It is a grid, not an n² comparison, so twice the numbers is not four
   times the cost;
5. draws least important first, so the most important number ends up on top.

Lowering the budget therefore does not "cut the display in half" — it keeps the numbers a player would
have looked at anyway and drops the ones in the distance.

---

## 10. Editor preview (no PIE)

With `bEnableEditorPreview` on, numbers also draw in an editor viewport with nothing playing, and the
ticker ages them there. That is what makes a screenshot, a camera fly-through or a store video possible
without pressing Play.

This second path uses `UDebugDrawService`, which is **compiled out of a Shipping build** — which is
exactly why it is the second path and `AHUD::DrawHUD` is the first. Nothing in your shipped game depends
on it. If a real HUD has already drawn in the same frame, the preview stands aside so the numbers are
never drawn twice.

---

## 11. Fonts

**No font ships with HitTicker**, for licensing reasons. A style with an empty `Font` falls back to the
engine's default Slate font at the style's size. To use your own: import a font asset and set it as
`Font` on the style. Everything else — size, outline, colour, scale — keeps working unchanged.

> **Set a `Font` on every style you ship.** The empty-`Font` fallback is a safety net, not a finished
> look. On a game viewport canvas it can resolve to a font the canvas will not draw, and the number then
> takes a ring slot, survives the budget and counts in the statistics box while nothing appears on
> screen. **If numbers read as "live" and "drawn" but you see nothing, an unset `Font` is the first thing
> to check.** The demo styles under `/HitTicker/HitTicker/Styles/` all point at
> `/Engine/EngineFonts/Roboto`, which every project already has.

---

## 12. Console commands

| Command | Effect |
|---|---|
| `HitTicker.Stats 0\|1` | On-screen statistics box: live, drawn, dropped, culled, merged, high water, draw ms. |
| `HitTicker.Budget <n>` | Set the hard per-frame draw budget. With no argument, prints the current one. |
| `HitTicker.Clear` | Drop every number currently on screen. |
| `HitTicker.Stress <n>` | Fire `n` numbers per second around the camera. `0` stops it. |
| `HitTicker.Merge 0\|1` | Master switch for folding repeat hits on one target into one number. |

The commands apply to every world that has a ticker, so they behave the same during PIE and in an editor
viewport with nothing playing.

---

## 13. Behaviour under load

- `Live` can never exceed `MaxLiveNumbers`. Past that the **oldest** number is overwritten — never the
  newest dropped, because the newest hit is the one the player is looking for.
- `DrawnThisFrame` can never exceed `MaxDrawnPerFrame`.
- `BufferHighWater` never climbs past `MaxLiveNumbers`. Sitting exactly there means the ring is doing its
  job, not that something is wrong.
- **Nothing is allocated while the game runs.** The ring, the merge lookup, the draw-candidate list and
  the avoidance grid are all reserved up front; text formatting goes into a stack buffer, so even the
  string for `-1234` allocates nothing.
- Motion is a pure function of `(age, style, seed)`. A number that was not drawn for four frames lands
  exactly where it would have been if it had been drawn every frame — so a frame spike elsewhere does not
  smear the numbers.
- Deleting a target while its numbers are in flight is safe (`TWeakObjectPtr`): the numbers finish their
  life at the last world point they knew about.

Measured on the shipped demo at `HitTicker.Stress 600` with the defaults: `Live` sits at 512/512,
`DrawnThisFrame` at 128/128, `DroppedByBudget` around 384, draw pass ≈ 1.2 ms. Those numbers are what the
statistics box is for — run it on your own content rather than taking them on faith.

---

## 14. Troubleshooting

| Symptom | Cause and fix |
|---|---|
| **Nothing on screen at all** | No canvas. Either your Game Mode's HUD Class is not `AHitTickerHUD`, or your own HUD never calls `DrawNumbers(Canvas)`. Check `HitTicker.Stats 1` — if `Live` climbs but `DrawnThisFrame` stays 0, it is the draw hook-up. |
| **`Live` and `Drawn` both climb, still nothing visible** | An empty `Font` on the style. See §11. |
| **Numbers appear but do not follow the enemy** | You are not passing `Target`. Without it, the number is anchored to a fixed world point. |
| **Repeat hits never merge** | Merging needs a `Target`; check `bMergeSameTarget` on the style, `bMergeEnabled` globally, and that both hits use the *same* style. |
| **Numbers vanish at range** | `MaxDrawDistance`, default 6000 units. Raise it, or accept it — projecting numbers you cannot read is the cost the setting exists to avoid. |
| **Numbers flicker in and out** | The budget is tight and entries are swapping across the cut. Raise `MaxDrawnPerFrame`, or lengthen `MergeWindow` so fewer entries compete. |
| **Numbers pile up on one spot** | `bAvoidOverlap` is off, on the style or globally; or `AvoidanceSearchRadius` is too small for the density. |
| **Style changes do nothing** | The asset is not in **Project Settings ▸ Plugins ▸ HitTicker ▸ Styles**, so the built-in fallback of the same name is being used instead. |
| **Changing `MaxLiveNumbers` has no effect** | It is read once, when the world comes up. Reload the level. |
| **Nothing draws in the editor viewport** | `bEnableEditorPreview` is off — or you are looking at a cooked/Shipping build, where that path does not exist by design (§10). |
| **Dedicated server logs nothing, draws nothing** | Correct. There is no canvas on a dedicated server; every call is a null-safe no-op. |

---

## 15. Limits — what HitTicker is not

- **No combat system.** No damage calculation, no hit detection, no health bars. HitTicker *shows* what
  your game has already worked out.
- **No UMG widgets.** Drawing is on `UCanvas`. If you want damage numbers built from animated widget
  Blueprints, this is the wrong plugin.
- **No 3D world-space text.** No `TextRenderComponent`, no number you can walk around. Numbers live in
  screen space above a world point.
- **No network replication.** Each client shows what its own game code tells it to show.
- **No font included** (§11).
- `MaxLiveNumbers` is read once per world; changing it takes effect on the next world load.
- Styles are limited to 255 per project (the entry's style index is a `uint8`).
- Verified platforms are Win64. The code is platform-neutral, but nothing else is
  shipped as verified.

---

## 16. The demo map

Everything below ships in the plugin, under one pack folder: `/HitTicker/HitTicker/`.

Open **`/HitTicker/HitTicker/Maps/L_HitTickerDemo`** and press Play. Nine targets drift around an arena
while a scripted fight runs against them; a control panel on the right drives the plugin directly.

| Asset | What it is |
|---|---|
| `Maps/L_HitTickerDemo` | The arena. Its World Settings already point at the demo Game Mode. |
| `Blueprints/BP_HitTickerDemoGameMode` | Game Mode whose **HUD Class** is `HitTicker HUD`. That one setting is the whole draw hook-up. |
| `Blueprints/BP_HitTickerDemoDirector` | The fight. A looping `CombatStep` rolls miss / block / heal / DoT / crit / hit per target and calls exactly one HitTicker node per branch. Every knob is exposed in the Details panel under **Demo Combat** and **Hit Ticker Runtime**. |
| `Blueprints/BP_HitTickerTarget` | One target: a mesh that wanders on Perlin noise, so you can watch numbers ride a moving actor. `Hit Height` is where its numbers spawn. |
| `UI/WBP_HitTickerDemoPanel` | The control panel: volley, crit storm, heal wave, miss/block, stress test, merge on/off, clear — plus a style legend. |
| `Styles/HTS_*` | The six standard styles as data assets: Damage, Crit, Heal, DamageOverTime, Miss, Block. |
| `Materials/*` | Arena materials. Nothing HitTicker needs — just so the numbers have something to sit on. |

The styles are registered in the host project's `Config/DefaultGame.ini` (§7). Copy that block into your
own project, or point **Project Settings ▸ Plugins ▸ HitTicker ▸ Styles** at them by hand. Without it the
plugin falls back to its built-in styles and the demo still runs — it just uses the fallback look.

Two things in the demo are worth stealing outright:

- **`BP_HitTickerDemoDirector`'s `CombatStep`** is the shape a real integration takes: your damage code
  already knows the world point, the value and the actor, so the HitTicker call is one node at the end of
  a branch you already have.
- **Merge off vs merge on.** Press `MERGE OFF`, then `VOLLEY × 40`: forty separate numbers, and the
  statistics box reads `Merged 0`. Press `MERGE ON` and volley again: one counting-up number per target.
  That is the difference between a readable bullet-heaven and a wall of digits.

---

## 17. Support

- Documentation: this file, shipped in the plugin under `Docs/`.
- Support link and documentation link are declared in `HitTicker.uplugin` (`SupportURL`, `DocsURL`).
- Questions, bug reports and feature requests are welcome — please include your engine version, platform,
  the relevant style asset settings and, where a performance question is involved, a screenshot of the
  statistics box (`HitTicker.Stats 1`). It answers most of the questions in one image.
