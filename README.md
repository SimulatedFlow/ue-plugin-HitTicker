# HitTicker

**Floating damage numbers, pooled and budgeted.** Thousands per second on one canvas pass — no
`UUserWidget` per hit, no allocation while the game runs, no frame spike.

Unreal Engine 5.8 · one runtime module · Win64 / Mac / Linux · no third-party code.

---

## Why

The usual damage number is a widget per hit: an object to construct, a Slate tree to build, a layout
pass to run, a font atlas to touch — thirty times a second in a bullet-heaven. HitTicker replaces that
with a fixed-size ring buffer allocated once, one draw pass per frame, and a hard budget.

## Install

1. Copy this folder into your project's `Plugins/` directory and enable **HitTicker**.
2. Either set **HitTicker HUD** as your Game Mode's HUD class, **or** add the **HitTicker HUD Component**
   to your own HUD and call `DrawNumbers(Canvas)` from `DrawHUD` — one line.
3. Call **Add Damage Number** from wherever your game applies damage.

```cpp
UHitTickerStatics::AddDamageNumber(this, Hit.ImpactPoint, Damage, DamagedActor);
```

Pass the target actor whenever you have one: that is what merges repeat hits into a single counting-up
number and what makes numbers follow a moving enemy.

## What you get

- Fixed-size ring buffer, allocated once; full overwrites the **oldest**, never drops the newest
- Hard per-frame draw budget, cut by importance (near, crit, young first)
- Same-target merging — thirty hits a second read as one growing total
- Screen-space overlap avoidance on a grid, not an n² comparison
- Styles as data assets: font, colour over life, pop, arc, outline, prefix/suffix, importance
- Draws in the editor viewport without PIE, for screenshots and camera moves
- On-screen statistics and a `HitTicker.Stress <n>` command, so the claims above are measurable

## See it running

Open `/HitTicker/HitTicker/Maps/L_HitTickerDemo` and press Play. Nine targets drift around an arena
under a scripted fight; the panel on the right fires volleys, crit storms, heals, misses and a 600/s
stress test, and toggles merging on and off while the statistics box shows what that costs. Every demo
asset lives under the single pack folder `/HitTicker/HitTicker/` — map, Game Mode, director, target,
control panel, the six style assets. Details in [`Docs/DOCUMENTATION.md`](Docs/DOCUMENTATION.md) §12.

## Not included

No combat system, no hit detection, no health bars, no UMG widget variant, no 3D world-space text, no
replication, and no font (licensing — the styles use the engine default).

Full documentation: [`Docs/DOCUMENTATION.md`](Docs/DOCUMENTATION.md).
