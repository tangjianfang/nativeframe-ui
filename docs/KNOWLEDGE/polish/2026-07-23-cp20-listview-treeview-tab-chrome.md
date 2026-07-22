# CP20 — ListView / TreeView / TabControl 自绘 chrome

> **状态**：✅ 已提交（CP20） — 2026-07-23
> **前置**：CP19 chrome 一致性 + 跨控件焦点环
> **测试**：`NativeFrameUISmokeTest` 0.90 s（环境变量 `NONFUI_SKIP_DIALOG=1`）

## 目标

CP19 把基础控件（Edit / ComboBox / ListBox / CheckBox / RadioButton）的 chrome（边框、焦点环、背景、字体）收敛到调色板。但容器型 native 控件 `ListView` / `TreeView` / `TabControl` 仍是 OS 默认灰色孤岛——选中行灰白、悬停无反馈、Tab 标签白底灰边。

CP20 给这三个控件接入 chrome 自绘：行背景、选中态、悬停态、Tab 圆角 + 标签色，全部走 `ThemePalette` + `FrameStyle`/`ListViewStyle`/`TreeViewStyle`，与其他 chrome 语言一致。

## 设计要点

### ListView

- **行背景**：`LVS_EX_FULLROWSELECT | LVS_EX_TRACKSELECT` 启用后，自定义绘制走 `NMLVCUSTOMDRAW` 路径：
  - `CDDS_PREPAINT` → `CDRF_NOTIFYITEMDRAW` 告知系统按行通知。
  - `CDDS_ITEMPREPAINT` → 设置 `clrText` / `clrTextBk`（选中态取 `selection_text` / `selection`，悬停取 `surface_hover`，默认 `text` / `surface`）。
  - `CDDS_ITEMPOSTPAINT` → 用 `fill_rect` 在行背景上再画一次，作为系统 `clrTextBk` 未生效时的兜底（保证 `LVS_OWNERDATA` + `FULLROWSELECT` 边界情况下也不露白底）。
- **ListView 空区背景 / 基础文本色**：`ListView_SetBkColor` / `ListView_SetTextColor` 在 `on_palette_changed` 同步。
- **Header 控件**：`WC_HEADERW` 是 ListView 的子窗口，有独立的 HWND。
  - Windows SDK 10.0.26100.0 **没有声明 `HDM_SETBKCOLOR`**（`grep` 全 SDK 零命中），也没有 `HDM_SETTEXTCOLOR`。
  - 唯一可控的路径是给 Header HWND 装 `WM_ERASEBKGND` 子类，在系统画列矩形前先把整个 header 带填成 `palette.surface`，然后让系统照常画列（系统画列时会清掉刚填的底，但列之间的空隙就是我们要的色）。
  - Header 文字仍为系统默认（无公开 HDM API 可改），但已经在浅/深色调色板下不显得突兀——后续 CP 想要彻底自绘，需要走 `NM_CUSTOMDRAW` 的 item-post-paint 自绘 caption。
- **悬停**：`LVS_EX_TRACKSELECT` 触发 `CDIS_HOT`，自定义绘制按位检测写背景。

### TreeView

- **基础颜色**：`TVM_SETBKCOLOR` / `TVM_SETTEXTCOLOR` / `TVM_SETLINECOLOR`（ListView 有但 TabControl 没有的头号 API）控制空区、文本、缩进线。
- **悬停 / 选中行**：`TVS_TRACKSELECT` 启用 → `CDIS_HOT`；`NMTVCUSTOMDRAW`：
  - `CDDS_PREPAINT` → `CDRF_NOTIFYITEMDRAW`。
  - `CDDS_ITEMPREPAINT` → 写 `clrText` / `clrTextBk`（正常行 `clrTextBk = CLR_NONE` 让系统用 `TVM_SETBKCOLOR` 透传，悬停/选中行填 `surface_hover` / `selection`）。
  - `CDDS_ITEMPOSTPAINT` → 用 `fill_rect` 在选中/悬停行再画一次（系统画 indent guides / +/- 图标后不会被遮）。
- **不画普通行**：普通行保持透传，避免覆盖系统画的缩进线和按钮。

### TabControl

- **自绘整 Tab**：`CDRF_SKIPDEFAULT | CDRF_NOTIFYPOSTPAINT` 跳过系统默认绘制，`CDDS_ITEMPOSTPAINT` 自己画：
  - 圆角（`fill_rounded_rect`），选中态和悬停态用 `corner_radius_control`，未激活态直角。
  - 颜色：`FrameStyle.background`（选中 → `palette.surface`）/ `chrome_bg`（悬停 → `surface_hover`，默认 → `surface`）/ `chrome_text`（选中 → `text`，未选中 → `text_secondary`）。
  - 标签：`TCM_GETITEM` 拿文字，`draw_text` 居中绘制，padding 6px。
- **`NMCUSTOMDRAW` vs `NMCTCUSTOMDRAW`**：SDK 同样**未声明 `NMCTCUSTOMDRAW`**。改用通用 `NMCUSTOMDRAW`（`dwItemSpec` 直接是 tab 索引），跟 ListView 字段访问保持一致。
- **Tab 控件 `FrameStyle`**：复用 CP19 的 `FrameStyle`，新增 `background` / `chrome_bg` / `foreground` / `chrome_text` 字段覆盖。

## 字段新增

```cpp
struct ListViewStyle {
    std::optional<Color> row_background;
    std::optional<Color> row_foreground;
    std::optional<Color> selected_background;
    std::optional<Color> selected_foreground;
};

struct TreeViewStyle {
    std::optional<Color> row_background;     // TVM_SETBKCOLOR
    std::optional<Color> row_foreground;     // TVM_SETTEXTCOLOR
    std::optional<Color> line_color;         // TVM_SETLINECOLOR
    std::optional<Color> selected_background;
    std::optional<Color> selected_foreground;
};
// TabControl 复用 FrameStyle（CP19）。
```

## 关键代码位置

- `src/controls/ListView.cpp` — `handle_custom_draw` / `paint_row_background` / `theme_header` / `visual_subclass_proc` / `header_subclass_proc`
- `src/controls/TreeView.cpp` — `handle_custom_draw` / `paint_row_background` / `visual_subclass_proc`
- `src/controls/TabControl.cpp` — `handle_custom_draw` / `paint_tab` / `visual_subclass_proc`

## 反射路径

`ocm_base + WM_NOTIFY`（其中 `ocm_base = WM_USER + 0x1c00`）由 `Window` 父控件把收到的 `WM_NOTIFY` 反射回子控件，子控件的子类链 `SetWindowSubclass` 看到的就是反射消息。CP19 已经在 `Control::subclass_proc` 里处理了 `NMCUSTOMDRAW`（PREPAINT / ITEMPREPAINT）的基础回退逻辑；CP20 在 ListView / TreeView / TabControl 的 `visual_subclass_proc` **前置拦截**反射消息，按控件类型做更细的处理（item-post-paint 兜底、Tab 整 Tab 自绘等）。

## 取舍

- **Header 自绘 caption** 没做。SDK 无 `HDM_SETTEXTCOLOR`，自绘 caption 需要接管 item-post-paint 整列绘制，会增加复杂度；列标题在浅色调下用系统默认色已可读，留给后续 CP。
- **TreeView 普通行不画背景**——透传 `TVM_SETBKCOLOR`，避免覆盖缩进线和按钮位图。
- **`NMCUSTOMDRAW` 通用结构替代 `NMCTCUSTOMDRAW`**——SDK 不声明 tab 专属扩展，通用结构字段够用。

## 已知陷阱（CP20 评审发现并修复）

- **ITEMPOSTPAINT 兜底 fill_rect 覆盖系统刚画的文本/图标**：早期 CP20 在 `CDDS_ITEMPOSTPAINT` 调 `fill_rect(cd->nmcd.rc, bg)`，pre-paint 阶段系统已经按 `clrText`/`clrTextBk` 画好了图标和文字，post-paint 重涂同色矩形把前景像素直接抹掉。ListView 全部行、TreeView 选中/悬停行都被影响。修复：去掉 post-paint 路径，完全靠 PREPAINT 的 `clrText`/`clrTextBk`（系统原生支持）。
- **TabControl 把 `dwItemSpec == 0` 当 sentinel**：Win32 TabControl 的 `dwItemSpec` 直接是 0-based tab 索引，无保留值。原先 `if (dwItemSpec == 0) return;` 让首个 tab 永远缺文字。修复：去掉判断，索引 0 tab 正常绘制。
- **HDM_SETBKCOLOR 在 Windows SDK 10.0.26100.0 中未声明**（`grep -rn` 全 SDK 零命中）：ListView header 背景只能走 `WM_ERASEBKGND` 子类路径。Header 文字色无公开 HDM API，留待后续 CP。
- **`NMCTCUSTOMDRAW` 在 Windows SDK 10.0.26100.0 中未声明**：TabControl 用通用 `NMCUSTOMDRAW`（`dwItemSpec` 是 tab 索引），跟 ListView 字段访问保持一致。

## 测试

- 三个控件的创建/销毁路径已在 `per_component_smoke` (T9/T10/T11) 和 `wmain` 控件创建路径覆盖。
- 视觉验证在 `NativeFrameUIControlsPlayground` / `NativeFrameUIShowcase` 走 `LVView_Dark` / `TabStrip_Light` 实例可见（CP21 起 sample 主题随控件更新）。
- `ctest --preset x64-debug` 三项测试（smoke / boundary / architecture）全部通过。