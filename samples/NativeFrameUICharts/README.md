# NativeFrameUICharts

The chart-renderer showcase. Drives every chart kind shipped in V1.0
(`Bar / HBar / Line / Spline`) through a 2 × 2 grid against the shared
palette + font cache. The four views share a single registered
`NativeFrameUIChartView` window class — the first `create()` registers
it, the rest reuse it.

## What it demonstrates

- **Header band** — coral hairline at the bottom edge (matches the
  SettingsDemo + Showcase family) so the sample reads as a sibling of
  the rest of the product-growth demos.
- **Caption strip** below the header — short caps labels
  (`Vertical bar — monthly revenue`, `Horizontal bar — platform mix`)
  so the four charts identify themselves even before the user hovers
  them.
- **2 × 2 chart grid** (top-left → bottom-right):
  - **`nfui::BarChartView`** — single monthly revenue series, twelve
    bars (Jan→Dec), `axis_y = 0..100`.
  - **`nfui::HBarChartView`** — three series (`FY 2022 / FY 2023 /
    FY 2024`) across five categories (`Desktop / Mobile / Tablet /
    Console / Smart TV`). Coloured `accent / success / warning`.
  - **`nfui::LineChartView`** — revenue vs. costs, twelve monthly
    samples, markers on (`point_radius = 3`).
  - **`nfui::SplineChartView`** — 30-point sine wave, `tension = 0.5`,
    no markers (intentional — the smooth Catmull-Rom curve stays
    uncluttered).

Data sets are baked as `constexpr` `std::array<double, N>` literals so
the sample is reproducible bit-for-bit across runs.

## Key controls

- `nfui::BarChartView`, `nfui::HBarChartView`, `nfui::LineChartView`,
  `nfui::SplineChartView` — all subclasses of `nfui::ChartView`.
- `nfui::Window` — host window with `WM_SIZE` / `WM_DPICHANGED` /
  `WM_PAINT` handlers.
- `nfui::FontCache` — single shared cache injected into all four
  chart views before `create()`.
- `nfui::ThemePalette` — light cream + coral accent palette.
- `nfui::DpiScale` — every layout measurement is logical; the grid
  reflows on DPI changes via `set_font_cache` re-binding.
- `nfui::MemoryDC` — flicker-free offscreen buffer for the header +
  caption strip (the chart views paint their own surfaces).
- `nfui::ResourceContext` — loads `IDI_NFUI_APP` for the title bar.
- `nfui::initialize_chart_aa()` — one-time call to enable the AA
  hint that the chart renderer reads.

## Theme support

Single light palette. The demo's product story is "four chart kinds on
one canvas" — adding a theme toggle would compete with the chart
content. For Light / Dark / HC comparison, run
`NativeFrameUIThemeDemo` or paint against a different palette by
editing the `nfui::theme_palette(...)` call.

## Build & run

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target NativeFrameUICharts
./out/build/x64-debug/Debug/NativeFrameUICharts.exe
```

Same recipe works for `x64-release`.

## Known limitations

- No interactive crosshair / tooltip on hover — this is a static
  surface, not a charting widget.
- Series `name` fields are `std::wstring_view` borrowed from static-
  storage `constexpr` literals. Adding or renaming a series requires
  updating the literal AND keeping the storage alive for the chart
  view's lifetime.
- The four chart views are uniformly sized; very tall data ranges
  may compress axis labels.
- Chart axes use the documented `ChartAxisRange` `format` field with
  `{:.0f}` — values are rendered without decimal precision, which
  matches the demo's USDk / percent framing.
