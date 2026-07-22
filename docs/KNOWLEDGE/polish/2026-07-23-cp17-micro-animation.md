---
title: CP17 micro-animation / transition system
date: 2026-07-23
tags: [polish, animation, timer, transition, easing]
status: in-progress
severity: polish
effort: medium-large
---

## Goal

Give the framework its first animation capability — the thing CP10A audited
and deferred to v2.0. The user's "every demo must be a polished showcase"
standard (see `feedback-demo-polish-standard` memory) makes stiff instant
state-flips a visible defect. CP17 adds smooth micro-transitions where they
read as life, not as sugar:

- Button hover / press face cross-fade (~120 ms, ease-out).
- Splitter drag pulse (active while dragging).
- Theme-switch palette cross-fade helper for samples (~200 ms).

## Design constraints (from the CP10A audit + this pass's territory map)

1. **No `ApplicationContext`, no global mutable singletons** (`AGENTS.md`).
   A shared `TimerScheduler` is therefore not available.
2. **`subclass_proc` is `noexcept`**; no exception may cross the timer arm.
3. **`WM_TIMER` runs on the UI thread inside the message loop**; a tick that
   repaints must use async `InvalidateRect` (not `UpdateWindow`) to avoid
   re-entering `on_paint` synchronously inside the tick.
4. **HWND lifetime**: a timer must be killed before the HWND is gone.
   `WM_NCDESTROY` is the existing detach point.
5. **Tests are synchronous** — the smoke test never runs a message loop, so
   `WM_TIMER` never fires under it. Animation *logic* must therefore be pure
   and drivable by an injected clock, independent of the OS timer.
6. **HC profile stays flat** (CP10/CP16 precedent): animation is suppressed
   in high-contrast so the chrome never softens an accessibility chrome.
7. **System UI-effects setting**: honor `SPI_GETCLIENTAREAANIMATION`; if the
   user disabled client-area animation, transitions resolve instantly.

## Key design decision: per-HWND timer owned by the existing subclass

The scheduler-ownership problem (singleton vs `ApplicationContext`) is
sidestepped entirely: each animating `Control` owns its own OS timer via
`SetTimer(hwnd, id, period, nullptr)`, dispatched by a new `WM_TIMER` arm in
`Control::subclass_proc`. The timer is killed in `detach_destroyed_hwnd`
(which already runs on `WM_NCDESTROY`). No shared scheduler, no singleton,
no new service container. This matches the framework's existing ownership
model (per-HWND subclass installed in `create_native`).

## Architecture (layered, one-way)

```
Easing.hpp          (core, pure)        ease_out_cubic / ease_in_out_cubic / ...
Clock.hpp           (core, pure-ish)    Clock interface + Win32Clock (GetTickCount64)
Animation.hpp       (paint, pure)       ColorAnimation state machine (begin/sample)
Theme.hpp/.cpp      (theme)             + lerp_color(Color,Color,float) + lerp_palette(...)
Control.hpp/.cpp     (control_base)      + Clock* clock_, + animation timer, + on_animation_tick
Controls.cpp         (control_base)      + WM_TIMER arm, + kill on NCDESTROY
Button.cpp           (button)           hover cross-fade via ColorAnimation
Splitter.cpp         (splitter)          drag pulse via phase accumulator
samples/...          (samples)          theme cross-fade via lerp_palette + window timer
```

`lerp_color` lives in the **theme** layer (with `Color`), not in paint. This
keeps the theme layer from depending upward on paint (`theme` allowed set is
`{core, dpi}`; `paint` depends on `theme`, so `theme→paint` would be a cycle).
`Animation.hpp` (mapped to `paint`) includes `Theme.hpp` + `Easing.hpp` — both
allowed for paint. The architecture checker (`tools/verify_architecture.ps1`)
maps `Clock.hpp`/`Easing.hpp`→core and `Animation.hpp`→paint.

`nfui_button` and `nfui_frame` already depend on `nfui_core`/`nfui_theme`/
`nfui_control_base`; the new headers flow down without new edges.
`nfui_control_base` gains the timer arm (it already owns `subclass_proc`).
No new module, no new CMake source list (all new code is header-only or
folded into existing .cpp files).

## Pure layer (fully unit-tested, no HWND)

### `include/nfui/Easing.hpp`
```cpp
namespace nfui {
float ease_linear(float t) noexcept;
float ease_out_cubic(float t) noexcept;     // 1 - (1-t)^3
float ease_in_out_cubic(float t) noexcept;   // smoothstep-ish
float ease_out_quint(float t) noexcept;      // 1 - (1-t)^5  (snappy finish)
}
```
All clamp `t` to `[0,1]`.

### `include/nfui/Clock.hpp`
```cpp
namespace nfui {
class Clock {
public:
    virtual ~Clock() = default;
    virtual unsigned long long now_ms() const noexcept = 0;
};
class Win32Clock : public Clock {
public:
    unsigned long long now_ms() const noexcept override; // GetTickCount64
};
}
```
A function-local static `Win32Clock` is the default when none injected —
stateless utility, not a mutable singleton (analogous to `theme_palette`).

### `include/nfui/Animation.hpp`
```cpp
namespace nfui {
struct ColorAnimation {
    Color from{};
    Color to{};
    unsigned long long start_ms{0};
    unsigned int duration_ms{0};
    bool active{false};

    void begin(Color from_, Color to_, unsigned long long now, unsigned int duration) noexcept;
    Color sample(unsigned long long now, float (&ease)(float)) noexcept;
    bool is_active() const noexcept { return active; }
    void cancel() noexcept { active = false; }
};
}
```
`sample` returns `from` at t=0, `to` at t≥1 (and clears `active`), lerped by
`ease` in between. Pure given `now`.

### `Paint.hpp` / `Theme.hpp`
- `Color lerp_color(Color a, Color b, float t) noexcept;` (straight channel
  lerp — distinct from `alpha_blend` which composites src over dst).
- `ThemePalette lerp_palette(const ThemePalette& a, const ThemePalette& b, float t) noexcept;`

## Driver layer (Control base)

`Control` gains (private/protected):
- `const Clock* clock_{nullptr};` + `void set_clock(const Clock*) noexcept;`
  + `unsigned long long clock_now_ms() const noexcept;` (returns injected
  clock's now, else a default `Win32Clock`).
- `UINT_PTR anim_timer_id_{0};` + `void start_anim_timer(unsigned int period_ms) noexcept;`
  + `void stop_anim_timer() noexcept;`.
- `virtual void on_animation_tick(unsigned long long now_ms) noexcept {}`
  — leaves override to advance their animation + `InvalidateRect`.
- A fixed `constexpr UINT_PTR anim_timer_event = 0xA017;` event id.

`Controls.cpp` `subclass_proc`:
- New `case WM_TIMER:` — if `wparam == anim_timer_event` and control != null,
  call `control->on_animation_tick(control->clock_now_ms())`. Do NOT
  `InvalidateRect` here (the leaf does it, async). Break to DefSubclassProc.
- `detach_destroyed_hwnd`: `KillTimer` before releasing (HWND still valid at
  NCDESTROY). Belt-and-suspenders: also kill in the WM_NCDESTROY path.

## Button hover cross-fade

`Button` gains `ColorAnimation hover_anim_` + `bool last_hover_{false}`.
Refactor face selection into `Color button_face(const PaintState&, const ThemePalette&, bool hc)` so both the transition detector and the paint path call it.

`on_paint`:
1. Compute target face for `state`.
2. If `state.hover != last_hover_` (and !hc and system animations enabled and
   enabled): `hover_anim_.begin(prev_face, target_face, now, 120)`; start
   anim timer (16 ms). `last_hover_ = state.hover`.
3. If `hover_anim_.active()`: face = `hover_anim_.sample(now, ease_out_cubic)`;
   else face = target face.
4. Paint with `face` (gradient path from CP16 still applies).

`on_animation_tick`: if `hover_anim_.active()`, `InvalidateRect(hwnd, nullptr, FALSE)`;
when `sample` cleared `active`, `stop_anim_timer()`.

Pressed/disabled stay instant (CP3: anchor states must read at-a-glance).

## Splitter drag pulse

`Splitter` gains `float pulse_phase_{0};` + uses the base anim timer.
`set_dragging(true)` → `start_anim_timer(33)` (~30 fps is enough for a pulse).
`set_dragging(false)` → `stop_anim_timer()`.
`on_animation_tick`: if dragging, `pulse_phase_ += 0.12f` (wrap 2π), `InvalidateRect`.
`on_paint` (dragging && !hc): hit-line alpha = `0.55f + 0.30f * sin(pulse_phase_)`,
drawn via a 32-bit DIB alpha-blend so the line breathes. Idle/hover unchanged.

> Implementation note: the pulse ended up as a **brightness lerp** between
> `accent_hover` and a 35% lift of it (driven by `0.5 + 0.5*sin(phase)`), not
> an alpha-blended DIB. A 2 px line does not justify a per-paint 32-bit DIB;
> the brightness lerp reads the same "breathing" way at a fraction of the
> cost and keeps `on_paint` a cheap `fill_rect`.

## Theme cross-fade (sample-level)

`lerp_palette` is the framework primitive. A sample that wants a smooth
theme switch keeps `ThemePalette from_, to_;` + a window-level `SetTimer`
(owned by the sample's top-level HWND, like the Playground's existing
progress timer) and on each tick builds `lerp_palette(from_, to_, t)` and
calls `set_palette` on its controls. Wired into
`NativeFrameUIControlsPlayground::switch_mode` (it already has the control
list + a timer pattern). 200 ms, ease-in-out-cubic.

## Test plan

Smoke test additions (synchronous, no message loop needed for the pure layer):
- `ease_out_cubic(0)==0`, `ease_out_cubic(1)==1`, monotonic.
- `lerp_color(black, white, 0.5)` ≈ RGB(127,127,127).
- `lerp_palette(dark, light, 0.5)` field-by-field midpoint.
- `ColorAnimation`: begin at t0; `sample(t0)==from`; `sample(t0+60)==mid`;
  `sample(t0+120)==to` and `!active`; cancel works.
- `Win32Clock::now_ms()` is non-decreasing across two calls.
- Button/Splitter still paint without crash (existing synchronous paint
  tests unchanged — no hover transition is triggered there).

The `WM_TIMER` arm itself is not exercised by the synchronous harness (it
cannot be); the logic it drives is covered, and the arm is a 5-line
dispatch verified by manual run of the samples.

## Non-goals (still deferred)

- Per-control `AnimationState` threaded through every `on_paint` (CP10A's
  broad refactor). Only Button + Splitter animate in CP17.
- TabControl cross-fade (ComCtl32 owns selection chrome).
- Direct2D render path.