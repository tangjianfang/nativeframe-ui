# CP19 — Chrome consistency + 全控件焦点环

## 问题陈述

CP18 之前的 chrome 语言不一致：

- Edit/ComboBox 的焦点环与 hover 态的边框颜色相同（都是
  `accent_hover`），用户难以分辨"被聚焦"与"鼠标悬停"。
- ListBox 是 native LISTBOX with WS_BORDER，从来没有自定义 chrome；
  系统默认灰边框与调色板完全无关。
- CheckBox/RadioButton 完全保留原生虚线焦点矩形，与 nfui 的 palette
  风格脱节。
- Tooltip 是平面的矩形气泡，没有 TTS_BALLOON 选项。

CP19 的目标是把这些差异填平，所有可聚焦控件共用一个清晰的、
调色板感知的焦点语言。

## 设计

### 共享 `paint_focus_border` 辅助函数

`nfui::paint_focus_border(HDC, RECT, Color, int width)` 位于 `Paint.hpp`，
编译进 `nfui_core`（与 CP16 的阴影/渐变一致）。它堆叠 `width` 个 1px
`FrameRect`，从外到内每帧 inset 1px——避免单支宽笔的中心位于矩形边缘
导致一半像素被裁掉的问题。这样 2px、3px 的边框都是像素级锐利的。

### 焦点边框语言

| 控件 | 未聚焦 | 聚焦 | 禁用 |
|------|--------|------|------|
| Edit | 1px `palette.border` | 2px `palette.accent` | 1px `border ⊕ background 55%` |
| ComboBox | 1px `palette.border` | 2px `palette.accent` | 1px blend |
| ListBox | 1px `palette.border` | 2px `palette.accent` | 1px blend |
| CheckBox | 系统虚线（保留） | 2px `palette.accent` 客户端外框 | 系统虚线 |
| RadioButton | 系统虚线（保留） | 2px `palette.accent` 客户端外框 | 系统虚线 |

未聚焦=1px 灰边；聚焦=2px accent——粗细+颜色双重区分，
彻底解决"hover 与 focus 难以分辨"。

### CheckBox / RadioButton 的原生虚线焦点矩形

`BS_AUTOCHECKBOX` / `BS_AUTORADIOBUTTON` 默认绘制系统虚线焦点指示，
与 palette 不一致。CP19 通过 `WM_UPDATEUISTATE` + `UIS_SET|UISF_HIDEFOCUS`
对该控件单独抑制（不传播到兄弟），然后在 `WM_PAINT` 的后绘制阶段
用 `GetDC` 在客户区周围覆盖 2px accent 边框。

> 选择 `WM_UPDATEUISTATE`（直接发送）而不是 `WM_CHANGEUISTATE`（传播）：
> 前者只影响该窗口本身及其子窗口，按钮没有子窗口，所以 0 副作用；
> 后者会冒泡到父窗口并向下广播，影响所有兄弟。

### Tooltip 气球选项

`FrameStyle::balloon`（`std::optional<bool>`）控制 `TTS_BALLOON` 窗口样式。
TTS_BALLOON 是窗口创建时样式，必须在 `create_native` 之前决定——
因此 `Tooltip::create` 读取 `style_.balloon` 并把 `TTS_BALLOON` 拼到
`extra_style`。颜色叠加（`TTM_SETTIPTEXTCOLOR/BKCOLOR`）照旧工作。

### WM_NCPAINT chrome 子类过程

每个有自定义 chrome 的控件（Edit、ComboBox、CP19 新增的 ListBox）
都有一个独立于 `Control::subclass_proc` 的 `visual_subclass_proc`。
它们组成子类链（后进先出）：

```
WindowProc → visual_subclass_proc → (DefSubclassProc) → Control::subclass_proc
          → (DefSubclassProc) → 原始窗口过程
```

`DefSubclassProc` 把消息委托给链中下一个子类，最后才到达原生控件。
这样：

- `Control::subclass_proc` 仍然拥有 `OCM_DRAWITEM` 反射、行 hover、
  鼠标状态、`WM_SETTEXT`、动画 timer 等基础机制。
- `visual_subclass_proc` 拥有 NC chrome、focus state、theme 重绘。
- 两者通过 `WM_NCDESTROY` 各自 `RemoveWindowSubclass`，互不影响。

## 关键代码

- `include/nfui/Paint.hpp` — 新增 `paint_focus_border` 声明。
- `src/paint/Paint.cpp` — `paint_focus_border` 实现（堆叠 FrameRect）。
- `include/nfui/Controls/FrameTypes.hpp` — 新增 `balloon` 字段。
- `src/controls/Tooltip.cpp` — `Tooltip::create` 读 `style_.balloon`
  并把 `TTS_BALLOON` 拼到 `extra_style`。
- `src/controls/Edit.cpp` — `paint_border` 改用 `paint_focus_border`，
  focused 时 2px accent。
- `src/controls/ComboBox.cpp` — `paint_chrome` 同上。
- `include/nfui/Controls/ListBox.hpp` — 新增 `visual_subclass_proc` 和
  `paint_border` 声明；override `on_palette_changed` 重绘 NC 边框。
- `src/controls/ListBox.cpp` — 实现以上三者。
- `include/nfui/Controls/CheckBox.hpp` / `RadioButton.hpp` — 新增
  `visual_subclass_proc` 声明。
- `src/controls/CheckBox.cpp` / `RadioButton.cpp` — chrome 子类：
  `WM_PAINT` 后绘制 2px accent、`WM_UPDATEUISTATE` 抑制虚线焦点。
- `tests/nativeframeui_smoke.cpp` — 新增 `paint_focus_border` 像素测试，
  覆盖 1/2/3 px 宽度，验证四边着色、中心未覆盖、退化矩形安全。

## 测试

```
cmake --build --preset x64-debug          # 通过
ctest --preset x64-debug                  # 3/3 通过
  ✓ NativeFrameUISmokeTest                # 含 paint_focus_border 像素测试
  ✓ NativeFrameUIBoundaryCheck            # 无 MFC/ATL/WTL/BCG
  ✓ NativeFrameUIArchitectureCheck        # 依赖方向合规
```

## 影响范围

- **API 表面新增**：`paint_focus_border`、`FrameStyle::balloon`。
- **行为变化**：Edit/ComboBox/ListBox 焦点边框从 1px `accent_hover`
  升级为 2px `accent`；CheckBox/RadioButton 新增 2px `accent` 焦点环；
  Tooltip 可选 TTS_BALLOON。
- **视觉一致性**：所有可聚焦控件共享"1px 灰边 / 2px accent 焦点"语言。
- **回归风险**：低——`paint_focus_border` 在 CP16 shadow/gradient 风格
  之外新增，不影响渐变/阴影路径；ListBox 新增 `on_palette_changed`
  override 与 Edit/ComboBox 模式一致。
