export const meta = {
  name: 'cp17-review',
  description: 'Adversarial review of CP17 micro-animation system',
  phases: [{ title: 'Review' }, { title: 'Verify' }],
}

const ROOT = 'C:/tjf/github/NativeFrameUI'

const FINDINGS_SCHEMA = {
  type: 'object',
  properties: {
    findings: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          file: { type: 'string' },
          line: { type: 'integer' },
          summary: { type: 'string' },
          failure_scenario: { type: 'string' },
          severity: { type: 'string', enum: ['critical', 'important', 'minor'] },
        },
        required: ['file', 'summary', 'failure_scenario'],
      },
    },
  },
  required: ['findings'],
}

const VERDICT_SCHEMA = {
  type: 'object',
  properties: {
    is_real: { type: 'boolean' },
    reasoning: { type: 'string' },
    suggested_fix: { type: 'string' },
  },
  required: ['is_real', 'reasoning'],
}

const DIMENSIONS = [
  {
    key: 'timer-lifetime',
    prompt: [
      'You are reviewing the CP17 micro-animation system in a Win32 C++20 UI library at ' + ROOT + '. Focus ONLY on timer/HWND lifetime and re-entrancy correctness.',
      'Read these files fully: include/nfui/Controls/Control.hpp; src/controls/Controls.cpp (subclass_proc, detach_destroyed_hwnd, start_anim_timer, stop_anim_timer, clock_now_ms, system_animations_enabled, the new WM_TIMER arm); include/nfui/Animation.hpp; include/nfui/Clock.hpp; include/nfui/Easing.hpp.',
      'Hunt for bugs:',
      '1. Can the animation timer fire after the HWND is destroyed? (WM_NCDESTROY / detach_destroyed_hwnd ordering; SetTimer on a window being destroyed; KillTimer on a dead HWND.)',
      '2. Re-entrancy: WM_TIMER -> on_animation_tick -> InvalidateRect. Is anything calling UpdateWindow or RedrawWindow(UPDATENOW) inside a tick (synchronous re-paint inside the tick)? Could on_paint be re-entered while a tick is on the stack?',
      '3. Timer id collision: anim_timer_event = 0xA017. Could a leaf/sample SetTimer with the same id be hijacked? Does the WM_TIMER arm correctly ignore foreign timer ids (fall through to DefSubclassProc)?',
      '4. start_anim_timer re-arm: calling SetTimer with the same id while already running - does it reset the period correctly? Is anim_timer_ tracked correctly (SetTimer returns the id; is the return value used right)?',
      '5. Clock default: function-local static Win32Clock - any thread-safety/init issue? noexcept correctness across the boundary.',
      '6. Does stop_anim_timer get called on every path that should stop it (animation complete, control destroyed, drag end)?',
      'Report ONLY real defects with a concrete failure scenario (inputs/state -> wrong behavior or crash). If you find none, return an empty findings array. Do not report style nits.',
    ].join('\n'),
  },
  {
    key: 'button-logic',
    prompt: [
      'You are reviewing the CP17 Button hover cross-fade in a Win32 C++20 UI library at ' + ROOT + '. Focus ONLY on Button animation correctness.',
      'Read fully: include/nfui/Controls/Button.hpp; src/controls/Button.cpp (face_for, on_paint, on_animation_tick); include/nfui/Animation.hpp (ColorAnimation begin/sample).',
      'Hunt for bugs:',
      '1. Hover transition detection: on_paint compares state.hover != last_hover_. Is last_hover_ updated on every path? Could a transition be missed or double-fired across OCM_DRAWITEM vs WM_PAINT (Button is BS_OWNERDRAW so it paints via OCM_DRAWITEM - does last_hover_ stay consistent)?',
      '2. The cross-fade seeds the from-color from hover_anim_.sample() when active, else face_for(prev). Could sample() clearing active as a side-effect here cause a logic error before begin() re-activates? Trace it.',
      '3. can_anim = !hc && enabled && !pressed && system_animations_enabled(). When can_anim is false mid-fade (e.g. user presses button while hover fade running), does the face snap correctly to the pressed face? Is hover_anim_ cancelled?',
      '4. First-paint: last_hover_ defaults false. If a button is created already hovered (unlikely but possible), does the first paint animate from accent to accent_hover, or flash?',
      '5. Does the interpolated face flow correctly into BOTH the gradient path and the focus-ring fill (which re-fills interior with face)? Any path where a stale/discrete face is used instead of the animated one?',
      '6. on_animation_tick: it calls sample() (discarded via static_cast<void>) to clear active, then InvalidateRect, then stops timer if inactive. Is there a frame where the paint shows an uninterpolated face (undershoot) or overshoots past the target color?',
      '7. Does the timer ever run forever (never stops) if a hover transition is triggered but no subsequent paint happens?',
      'Report ONLY real defects with concrete failure scenarios. Empty array if none.',
    ].join('\n'),
  },
  {
    key: 'splitter-theme-arch',
    prompt: [
      'You are reviewing the CP17 Splitter drag pulse, the Playground theme cross-fade, and architecture in a Win32 C++20 UI library at ' + ROOT + '.',
      'Read fully: include/nfui/Controls/Splitter.hpp; src/controls/Splitter.cpp (set_dragging, on_animation_tick, on_paint pulse); samples/NativeFrameUIControlsPlayground/NativeFrameUIControlsPlayground.cpp (switch_mode, step_theme_fade, apply_palette_to_all, WM_TIMER arm, cleanup, new members); tools/verify_architecture.ps1 (header->module map for Clock.hpp/Easing.hpp/Animation.hpp and the allowed dependency sets); include/nfui/Theme.hpp + src/theme/Theme.cpp (lerp_palette); include/nfui/Paint.hpp + src/paint/Paint.cpp (lerp_color).',
      'Hunt for bugs:',
      '1. Splitter pulse: set_dragging(true) starts a 33ms timer; on_animation_tick bumps pulse_phase_ and InvalidateRect. Does the pulse stop on set_dragging(false)? Does on_animation_tick stop the timer if not dragging? Could the timer keep running after drag ends? Could pulse_phase_ drift (the 2pi wrap)?',
      '2. Splitter on_paint uses anim_timer_running() to decide pulse vs steady. Is that the right gate (vs dragging_)? What if dragging_ is true but the timer is not running (HC / system animations off - set_dragging skips start_anim_timer)? Does the line still render (steady) without crashing?',
      '3. Playground theme fade: switch_mode starts a fade; step_theme_fade advances it. Does it correctly kill the timer on completion? On rapid re-switch (user clicks theme twice during a fade), does it restart cleanly from the current interpolated palette_? Does palette_from_ get set to the CURRENT palette_ (which may be mid-fade) so the reversal is seamless?',
      '4. Does the fade snap correctly when either end is HC or system animations are off? Is is_high_contrast(palette_) the right check for the current shown palette?',
      '5. Architecture: Animation.hpp is mapped to paint and includes Paint.hpp (paint) + Easing.hpp (core). Is paint->core allowed? Is controls->paint allowed (Button includes Animation)? Any layering violation introduced by the new headers or the Theme.cpp->Paint.hpp include (theme->paint - is that allowed? theme allowed set is core,dpi; paint is NOT in it - is that a VIOLATION)?',
      '6. lerp_palette / lerp_color: any field missed in lerp_palette? Any clamping issue in lerp_color for t slightly outside [0,1] from an easing function?',
      'Report ONLY real defects with concrete failure scenarios. Pay special attention to #5 (theme->paint dependency) - verify against the allowed map. Empty array if none.',
    ].join('\n'),
  },
]

phase('Review')
const results = await pipeline(
  DIMENSIONS,
  (d) => agent(d.prompt, { label: 'review:' + d.key, phase: 'Review', schema: FINDINGS_SCHEMA }),
  (review, d) => {
    const fs = (review && review.findings) || []
    if (fs.length === 0) return []
    return parallel(
      fs.map((f) => () =>
        agent(
          'You are an adversarial verifier for the NativeFrameUI C++20 Win32 library at ' + ROOT + '. A reviewer claims this defect. Verify it by reading the actual code. Default to is_real=false if the code does not actually exhibit the bug or the scenario is impossible. Only confirm if you can point to concrete code that fails.\n\nClaim (file ' + f.file + ', line ' + f.line + '): ' + f.summary + '\nFailure scenario: ' + f.failure_scenario + '\n\nRead the relevant files and determine: is this a REAL defect? Respond with is_real, reasoning (cite file:line), and a suggested_fix if real.',
          { label: 'verify:' + d.key + ':' + f.line, phase: 'Verify', schema: VERDICT_SCHEMA }
        ).then((v) => ({ ...f, verdict: v, dimension: d.key }))
      )
    )
  }
)

const confirmed = results.flat().filter(Boolean).filter((f) => f.verdict && f.verdict.is_real)
return { confirmed, total_raw: results.flat().length }