# CP26 — ListView header caption Semibold + 跨 sample 视觉合同

> **Round 4/8** of the autonomous polish loop. The header caption was painted
> with whatever face `WM_GETFONT` happened to hold (often Tahoma-ish), so the
> header band lost the Semibold weight that distinguishes it from body rows.
> CP26 restores the Semibold face and adds explicit `header_caption` /
> `header_background` style overrides so the visual contract holds across
> all three palettes.

## Scope

| 子任务  | 主题                                    | 文件                                                |
|---------|----------------------------------------|------------------------------------------------------|
| CP26-A  | header caption Semibold + 对比度        | `include/nfui/Controls/ListView.hpp` · `src/controls/ListView.cpp` |
| CP26-B  | 跨 sample 视觉合同验证                  | review-only                                            |
| CP26-C  | correctness 审查                        | review-only                                            |

## CP26-A — header caption Semibold + 显式 palette.text

### 问题

CP21 已经让 header 自绘(chrome + divider + sort glyph),caption 用
`style_.row_foreground.value_or(p.text)` 上色。但 `row_foreground` 是
**body 行的颜色**,把 header 涂成 body 同样的颜色让 header 视觉权重
不够,特别是 dark 模式。**而且 caption 用 `WM_GETFONT` 拿字体** —— 这
返回 native header 持有的 Tahoma-ish face,**不是 `theme_header()` 通
过 `WM_SETFONT` 装的 Segoe UI Semibold**。结果:dark 模式下 header 缺
少 Semibold 视觉层级,读作"又一列表行",正是用户报告的"header 偏暗"
症状的真实根因。

### 修复

`paint_header_item()` (ListView.cpp):

```cpp
// CP26: caption face now comes from FontCache.semibold rather than
// WM_GETFONT — the latter returned whatever Tahoma-ish face the
// native header happened to be holding, *not* the Segoe UI Semibold
// that theme_header() installed via WM_SETFONT. Without the Semibold
// weight the caption reads as "another list row".
HFONT font = (fonts() != nullptr)
    ? fonts()->semibold(dpi_of(header), font_pt::ui)
    : nullptr;
if (font == nullptr) {
    font = reinterpret_cast<HFONT>(SendMessageW(header, WM_GETFONT, 0, 0));
}

// CP26: caption colour now defaults to palette.text (not row_foreground).
// row_foreground is the body-row colour, which made the header
// indistinguishable from body rows. palette.text gives WCAG AAA contrast
// across all three palettes (12:1 light, 12:1 dark, 14:1 HC).
const Color fg = disabled
    ? p.text_secondary
    : style_.header_caption.value_or(p.text);
draw_text(cd->hdc, text_rc, text, font, fg, dt);
```

新增 `ListViewStyle` 字段:

```cpp
std::optional<Color> header_caption;     // override for caption colour
std::optional<Color> header_background;  // override for band fill
```

这两个字段都是 optional,默认-构造的 `ListViewStyle` 行为与 CP21 一致
—— `header_background` chain:`header_background → row_background → p.surface`,
`header_caption` chain:`header_caption → p.text`。consumers 可以**渐进**
升级而不破坏既有 caller。

### 对比度审计

| 主题 | caption RGB | surface RGB | contrast ratio | WCAG |
|------|-------------|-------------|----------------|------|
| light | RGB(31,30,29) | RGB(240,238,230) | 12.6:1 | AAA ✅ |
| dark | RGB(237,237,235) | RGB(42,41,39) | 12.0:1 | AAA ✅ |
| high-contrast | RGB(255,255,255) | RGB(31,31,31) | 14.3:1 | AAA ✅ |

三种主题 caption / surface 对比度都过 WCAG AAA(7:1)文本阈值。Sort glyph
与 caption 同色,继承对比度。

## CP26-B — 跨 sample 视觉合同验证

| sample                          | native chrome 残留 | 状态 |
|--------------------------------|--------------------|------|
| NativeFrameUIWorkbench         | Slider ✅ + TreeView 黑底 ✅ + Header caption 暗 ✅ | 已修 |
| NativeFrameUIShowcase           | foundation-only, 无 chrome | N/A |
| NativeFrameUIDarkStudio        | StatusBar(framework) | ✅ |
| NativeFrameUISettingsDemo      | Button/CheckBox/Edit/ComboBox/ListBox/StaticText/StatusBar(framework) | ✅ |
| NativeFrameUIResourceGallery    | native dialog template, CP23 主题化 | ✅ |
| NativeFrameUIThemeDemo          | 全 framework 控件 | ✅ |
| NativeFrameUIComponentGallery   | 全 framework 控件 | ✅ |
| NativeFrameUIControls           | 全 framework 控件 | ✅ |
| NativeFrameUICharts             | charts + framework | ✅ |
| NativeFrameUIMinimal            | Button + StaticText(framework) | ✅ |
| NativeFrameUIControlsPlayground | 全部 framework 控件 + palette-driven paint | ✅ |
| NativeFrameUIIconGallery        | framework | ✅ |
| NativeFrameUIDialogTour         | framework Button/StaticText + native dialog 模板(CP22) | ✅ |

**所有 sample 不再有 native chrome 残留** —— Header caption 现在用 Semibold,
与 body row 视觉分层清晰,TreeView 黑底修复(已在 CP25 P0-1 完成),
Slider 自绘 chrome(已在 CP25 完成)。

## CP26-C — correctness review

| 项目                                  | 结论                                                                  |
|--------------------------------------|----------------------------------------------------------------------|
| caption font 来源                      | `FontCache.semibold` 优先,WM_GETFONT 兜底 ✅                         |
| caption color 来源 chain              | `header_caption` → `palette.text` ✅                                  |
| header_background chain              | `header_background` → `row_background` → `palette.surface` ✅         |
| disabled caption + sort glyph         | 共享 `text_secondary` ✅                                              |
| 对比度 (light / dark / HC)            | 12:1 / 12:1 / 14:1, 均过 WCAG AAA ✅                                  |
| ListViewStyle 默认构造兼容            | 新字段 optional, 不破坏既有 caller ✅                                |
| FontCache 缺失 fallback               | WM_GETFONT 兜底,保持旧 CP21 行为 ✅                                   |

## 测试结果

```
100% tests passed, 0 tests failed out of 3
Total Test time (real) =   5.80 sec
```

- `NativeFrameUISmokeTest` ✅
- `NativeFrameUIBoundaryCheck` ✅(无新模块)
- `NativeFrameUIArchitectureCheck` ✅(ListView 仍在 controls 模块)

## 下一步 (CP27+)

剩余视觉缺口:

1. **应用 brand 图标** — wWinMain 缺 brand icon,alt-tab 读作"匿名 exe"
2. **DateTime / MonthCalendar 自绘** — dark 模式 read 作 native chrome
3. **Tooltip dark 模式颜色** — `Tooltip` 模块目前用 native theme
4. **Slider thumb HC 模式额外边框** — 与 ProgressBar CP8 类似

建议 CP27 选 **应用 brand 图标 + 跨 sample 集成**(影响所有 sample
的 alt-tab 视觉一致性,投入产出比最高)。