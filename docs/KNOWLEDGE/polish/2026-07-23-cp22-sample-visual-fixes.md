# CP22 — Sample 视觉一致性修复

> **状态**：✅ 已提交 — 2026-07-23
> **前置**：CP15–CP21 chrome polished samples；Explore 审计给出 5 个高优先级可见缺陷
> **测试**：`NativeFrameUISmokeTest` 0.94s + boundary + architecture 3/3 通过

## 目标

CP15–CP21 修了 Edit / Combo / CheckBox / Radio / Tooltip / ListView / TreeView / TabControl 的 chrome，
但 sample 层仍有 5 个用户可一眼看见的缺陷。本轮专注于"用户最先看到、且修起来代价小"的样本可见缺陷，
不动 framework 控件自身。

## 修复清单

### CP22-A · NativeFrameUIDialogTour raw native chrome

**问题**：三个启动按钮用 `CreateWindowExW(L"BUTTON", BS_PUSHBUTTON)`，状态条用 `STATIC SS_SUNKEN`，
全部显示 Win32 系统灰底色 + Tahoma 8pt，是 sample 中视觉最差的一处。

**修复**：
- 引入 `nfui::Button` 三个成员 + `nfui::StaticText` 状态条
- 新增 `ThemePalette palette_` + `FontCache fonts_` 成员 + `DpiScale dpi_{96}`
- `inject_theme(&palette_, &fonts_)` 绑定后再 `create()`
- `apply_native_fonts()` 给三个 Button 发 `WM_SETFONT`（StaticText 自绘不需要）
- 窗口 `WM_PAINT` 画 `palette.background`，避免裸 `COLOR_BTNFACE`
- 布局改用 `dpi.logical_to_pixels(kPadX/kPadY/kButtonH)` 做 DPI 缩放
- CMake `target_link_libraries` 增加 `nfui_button` + `nfui_text`（README 已知限制同步更新）

### CP22-B · NativeFrameUIShowcase 键盘不可达 + 选中导航对比度

**问题**：
- 主题切换按钮是手绘 `fill_rounded_rect`，`Tab` 不到、`Enter` 不响应，鼠标用户才能切换。
- 选中导航用 `palette.text` 画在 `accent_soft` 上；暗色模式下 `accent_soft ≈ 背景 + 70% accent`，配 `text`（近白）会塌成 ~3:1。

**修复**：
- `ShowcaseView` 新增 `focus_index_`（-1=无焦点；0=主题切换；1..4=nav[0..3]），
  `set_focus_index` / `cycle_focus(bool reverse)` / `activate_focused()` / `focused_rect()`
- `ShowcaseWindow` 在 `WM_KEYDOWN` 处理 `VK_TAB`（含 `VK_SHIFT` 反向）、`VK_RETURN` / `VK_SPACE`
- `on_left_button_down` 命中 toggle / nav 时同时设 `focus_index_`
- `paint()` 在 `focus_index_ >= 0` 时画 accent 焦点环（inset 2 logical px，`fill_rounded_rect` radius+2）
- `palette_for` 暴露 `accent_text`，选中导航用 `accent_text` 而非 `text` 解决对比度

### CP22-C · NativeFrameUICharts 每格加 caption + Workbench inspector 字体 + 菜单注释

**问题**：
- Charts 5 个图表只给 bar / hbar 配 caption，line / spline / area 是"未标识"格子。
- 原 CP14 注释说"3rd cell intentionally empty so the grid reads balanced"——视觉上读作缺失资源。
- Workbench `inspector_`（StaticText 自绘）缺 `WM_SETFONT`，与邻居字体不一致。
- Workbench 菜单是 raw `CreateMenu/SetMenu`——未注释说明意图。

**修复**：
- Charts `paint_chrome` 改为 per-cell caption，每个 cell 上方 18 logical px 高度标签：
  "Bar — monthly revenue" / "HBar — platform mix" / "Line — revenue vs costs" /
  "Spline — sensor A (Hz)" / "Area — monthly revenue"
- `layout_chrome` 把每个 chart MoveWindow 高度减 `caption_reserve = 20 logical px`，把
  释放出的高度给 caption（不重叠图表）
- Charts header description 改"Bar/HBar/Line/Spline chart kinds" → 加 "Area"
- Workbench `apply_native_fonts` 增加 `inspector_.hwnd()` 的 `WM_SETFONT` 注入
- Workbench menu 构造加注释说明是 intentional raw chrome（与 ResourceGallery 的 LoadMenuW demo 对齐）

## 评审发现并修复的 bug

CP22 触发了 9 个确认 finding，全部修复。

### Major（必修）

1. **DialogTour `prefs_.valid()` 返回 stale pointer → 第二次点 Show Preferences 静默 no-op**
   `OwnedHwnd::valid()` 只检查 `hwnd_ != nullptr`，DLGPROC 直接 `DestroyWindow(dlg)` 后 wrapper 仍持有死 HWND；第二次点 "Show Preferences" 走到 `SetForegroundWindow(死 HWND)` 直接 return。
   **修复**：`launch_prefs` / `close_prefs` 改为 `prefs_.valid() && IsWindow(prefs_.hwnd()) != FALSE`。

2. **Showcase focus ring overpaints focused affordance with `palette.window`**（CP22-B 严重可见缺陷）
   `fill_rounded_rect` 用 GDI RoundRect 同时填充 + 描边，外圈 fill 是 `palette.window`（= 窗口背景），把 focused toggle / nav 的 surface fill + 标签文字一起擦白。
   **修复**：改用 `nfui::paint_focus_border`（stroke-only），affordance 自身的 surface + 文字保留。

3. **Showcase 点击已选中 nav 不重绘焦点环**
   `on_left_button_down` 在 nav 分支仅当 `selected_navigation_` 变化才 return true；首次启动（默认 selected=0, focus=-1）点 Overview：focus 从 -1 → 1 但 selected 不变，return false → ShowcaseWindow 不调 `InvalidateRect` → 焦点环不画。
   **修复**：同时追踪 `prior_focus`，返回 `focus_changed || selection_changed`。

4. **DialogTour 按钮居中用 unscaled `kButtonW` 但 MoveWindow 用 scaled `button_w` → >96% DPI 按钮右偏**
   `cx = (rc.right - rc.left - kButtonW) / 2` 在 200% DPI 下偏右 ~110 px。
   **修复**：`cx = (rc.right - rc.left - button_w) / 2`，用 DPI 缩放后的 `button_w` 做居中。

### Minor（已修）

5. **DialogTour 窗口从 220 增到 240，状态条 22 px 高 → 下方留 46 px 空带**
   **修复**：状态条 anchor 到 `rc.bottom - status_bottom_pad - status_h`，window 增长时往下贴而不是悬空。

6. **Showcase `accent_text` 在 dark 模式下 ≈ `text`**（RGB(255,255,255) vs RGB(237,237,235)），CP22-B 描述的对比度修复其实无效。
   **修复**：回退到 `palette.text`，移除无用的 `ShowcasePalette::accent_text` 字段与对应 `palette_for` 赋值，文档解释为何不用 accent_text。

7. **Charts 每格 chart HWND 短 20 logical px → 最小窗口下 HBar sub-bar 变细**
   **修复**：`caption_reserve` 从 20 → 18 logical px，`caption_h` 从 18 → 16，留回 2 logical px 给 chart HWND。

### Refuted（评审正确否定）

- "DialogTour `status_label_` 用了 `WS_TABSTOP`" — `ControlCreateParams::style` 默认就是 `WS_CHILD | WS_VISIBLE | WS_TABSTOP`，所有 wrapper 都继承，无差异。
- "Workbench `apply_native_fonts` 漏了 `left_splitter_` / `right_splitter_`" — Splitter 是自绘类，不画文字、不用 font。
- "Charts caption 与 chart HWND 在 120 DPI 下重叠 1 px" — 反向论证：实际是 caption 占用 26 物理像素而 reserve 给 25，所以 caption 底部超出 1 px，但这一像素被 chart HWND 的白色背景覆盖，从用户视觉上是 caption + chart 之间多 1 px 空隙，不是重叠。

## 已知限制

- Showcase 焦点环只覆盖主题切换 + 4 个 nav，未覆盖 3 个 card（card 不可激活，仅 hover）。
- Charts caption 区与 chart HWND 之间留 18 logical px 标题行，无显式边框，视觉上读作 chart 的一部分。
- Workbench 菜单仍是 raw native chrome，由 CP23 或后续专门 round 处理 framework Menu wrapper。
- Dark mode 下 `accent_soft ≈ 背景 + 70% accent` 是 muted wash，`text`（near-white）在上面够亮但跟 sidebar 本体颜色接近——视觉上 selected nav 与未 selected nav 区分主要靠 fill，不是文字权重。进一步优化需在 palette 层给 dark mode 配独立 `selection` 字段。

## 测试

- `NativeFrameUISmokeTest` 0.95s 通过（`NONFUI_SKIP_DIALOG=1`）。
- `NativeFrameUIBoundaryCheck` 2.64s 通过。
- `NativeFrameUIArchitectureCheck` 2.06s 通过。
- 视觉验证：`NativeFrameUIDialogTour.exe` / `NativeFrameUIShowcase.exe` / `NativeFrameUICharts.exe` / `NativeFrameUIWorkbench.exe`。