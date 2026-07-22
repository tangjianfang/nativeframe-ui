export const meta = {
  name: 'cp18-review',
  description: 'Adversarial review of CP18 vector icon system',
  phases: [
    { title: 'Review', detail: 'correctness, architecture, visual quality' },
    { title: 'Verify', detail: 'adversarially verify each finding' },
  ],
}

const FINDINGS_SCHEMA = {
  type: 'object',
  properties: {
    findings: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          title: { type: 'string' },
          file: { type: 'string' },
          line: { type: 'integer' },
          severity: { type: 'string', enum: ['critical', 'important', 'minor', 'nit'] },
          why: { type: 'string' },
        },
        required: ['title', 'file', 'severity', 'why'],
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
    key: 'correctness',
    prompt: [
      'You are reviewing CP18 of NativeFrameUI, a pure Win32 C++20 static UI library (nfui namespace).',
      'CP18 adds a vector icon system. Review ONLY for CORRECTNESS and EDGE CASES.',
      'Files to review (read them in full):',
      '  include/nfui/VectorIcon.hpp',
      '  src/paint/VectorIcon.cpp',
      '  include/nfui/Paint.hpp (the new fill_ellipse / draw_ellipse declarations)',
      '  src/paint/Paint.cpp (the new fill_ellipse / draw_ellipse implementations)',
      '  src/controls/ComboBox.cpp (chevron refactor in paint_chrome)',
      '  include/nfui/Controls/Button.hpp and src/controls/Button.cpp (leading icon in on_paint)',
      '  include/nfui/Controls/IconView.hpp and src/controls/IconView.cpp (set_glyph + glyph paint path)',
      'Focus on: GDI resource leaks (brush/pen not deleted, GDI object not restored); integer truncation',
      'or rounding in the 16x16 grid -> device-pixel mapping that could clip or misplace glyphs at small',
      'sizes or odd DPI; degenerate-rect handling; the aspect-preserving square computation; whether a',
      'glyph could paint ZERO pixels (the smoke test asserts each glyph paints >=1 foreground pixel —',
      'would any glyph fail that at a 48px cell?); re-entrancy or state mutation inside paint; off-by-one',
      'in the Button text_bounds narrowing (could text_bounds.left exceed right and is it clamped?);',
      'IconView disabled-glyph alpha_blend using the right background; whether set_glyph vs set_icon',
      'priority is correct. Do NOT flag style/naming. Only real correctness defects. Each finding: file,',
      'approx line, severity, and a concrete failure scenario.',
    ].join(' '),
  },
  {
    key: 'architecture',
    prompt: [
      'You are reviewing CP18 of NativeFrameUI (pure Win32 C++20, nfui namespace, layered architecture).',
      'Review ONLY for ARCHITECTURE / LAYERING / CONVENTION violations.',
      'Read: include/nfui/VectorIcon.hpp, src/paint/VectorIcon.cpp, include/nfui/Paint.hpp,',
      'src/paint/Paint.cpp, include/nfui/NativeFrameUI.hpp, tools/verify_architecture.ps1,',
      'cmake/nfui_core.cmake, CMakeLists.txt (the NativeFrameUIIconGallery target), src/controls/ComboBox.cpp,',
      'include/nfui/Controls/Button.hpp, src/controls/Button.cpp, include/nfui/Controls/IconView.hpp,',
      'src/controls/IconView.cpp, samples/NativeFrameUIIconGallery/NativeFrameUIIconGallery.cpp.',
      'Rules (from CLAUDE.md / docs/ARCHITECTURE.md): core must not depend on controls/command/layout/theme;',
      'paint may depend on core+theme only; icon may depend on core only; no global mutable singletons;',
      'never let C++ exceptions cross WindowProc/DialogProc/callback boundaries; only UI thread',
      'creates/destroys/repaints windows; keep logical units vs device pixels distinct; use LONG_PTR for',
      'pointer data; prefer composition over inheritance. Check: is VectorIcon in the right module (paint)?',
      'Are the new headers mapped in verify_architecture.ps1? Does the new .cpp compile into nfui_core?',
      'Is the logical-vs-device distinction kept (glyphs authored on a 16x16 LOGICAL grid, scaled via the',
      'device-pixel bounds — never persisting physical pixels)? Are any paint helpers noexcept-violating',
      '(allocations that could throw crossing a callback)? Is the IconGallery sample wired correctly',
      '(nfui_add_resources, link NativeFrameUI::NativeFrameUI, nfui_apply_compiler_options)? Only real',
      'violations, not style.',
    ].join(' '),
  },
  {
    key: 'visual',
    prompt: [
      'You are reviewing CP18 of NativeFrameUI. The user\'s quality standard (verbatim): "I want every demo',
      'to be a 精品 / top-tier showcase, not a raw grey styleless demo." Review the VISUAL / UX quality of',
      'the vector icon system and its showcase sample.',
      'Read: src/paint/VectorIcon.cpp (glyph geometry — do the glyphs READ as their intended shape at a',
      '16x16 grid? e.g. is the gear recognizable, the warning triangle + exclamation proportional, the',
      'search magnifier ring+handle sensible, the check mark aspect correct?), samples/NativeFrameUIIconGallery/',
      'NativeFrameUIIconGallery.cpp (card grid layout, label placement, theme switching, icon-button row),',
      'src/controls/Button.cpp (icon + caption balance — is the text centering with a leading icon going to',
      'look off-center or acceptable?), src/controls/ComboBox.cpp (chevron size/position vs the old hand-rolled',
      'triangle — is the new cell size dpi/8 reasonable, could it be too big/small?).',
      'Flag concrete visual defects: a glyph that will look wrong/clipped/unbalanced, a layout that will',
      'overlap or misplace at the default 1040x560 window, a disabled/HC state that will be invisible, a',
      'theme switch that leaves part of the UI un-repaletted. Be specific and visual. Severity by user',
      'impact. Do not flag subjective taste unless it clearly violates the 精品 standard.',
    ].join(' '),
  },
]

phase('Review')
const results = await pipeline(
  DIMENSIONS,
  (d) => agent(d.prompt, { label: `review:${d.key}`, phase: 'Review', schema: FINDINGS_SCHEMA }),
  (review, d) => {
    if (!review || !review.findings || review.findings.length === 0) return []
    return parallel(review.findings.map((f) => () =>
      agent(
        [
          'You are an adversarial verifier for NativeFrameUI CP18. A reviewer claims the following defect.',
          'Try to REFUTE it. Read the actual file(s) and confirm the code really does what the claim says.',
          'Default to is_real=false if you cannot confirm from the code. Only is_real=true if the defect is',
          'genuinely present and would cause a real problem (crash, leak, wrong render, architecture',
          'violation, or a visible 精品-standard break). Provide concrete reasoning citing the code.',
          '',
          `Dimension: ${d.key}`,
          `Title: ${f.title}`,
          `File: ${f.file}  (line ~${f.line})`,
          `Severity claimed: ${f.severity}`,
          `Claim: ${f.why}`,
        ].join('\n'),
        { label: `verify:${d.key}:${f.file}`, phase: 'Verify', schema: VERDICT_SCHEMA }
      ).then((v) => ({ ...f, dimension: d.key, verdict: v }))
    ))
  }
)

const confirmed = results.flat().filter(Boolean).filter((f) => f.verdict && f.verdict.is_real)
log(`Confirmed ${confirmed.length} real defect(s) after adversarial verification.`)
return { confirmed }