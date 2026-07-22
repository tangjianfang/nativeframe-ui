# CP21 — ListView header caption 自绘 + TabControl active border

> **状态**：✅ 已提交 — 2026-07-23
> **前置**：CP20 ListView/TreeView/TabControl 自绘
> **测试**：`NativeFrameUISmokeTest` 0.95 s（环境变量 `NONFUI_SKIP_DIALOG=1`）

## 目标

CP20 把 ListView/TreeView/TabControl 的行背景、选中/悬停态接入调色板，但遗留两个可见缺陷：

1. **ListView header column title 文字颜色仍是系统默认**——深色调色板下读不清。
2. **TabControl 选中 Tab 没有视觉强调**——和未选中 Tab 在视觉权重上区分不出来。

CP21 把这两块接入 chrome 自绘体系。

## ListView header caption 自绘

### 实现

- `header_subclass_proc`（header HWND 的子类）拦截 `ocm_base + WM_NOTIFY` + `NM_CUSTOMDRAW`。
- `handle_header_custom_draw`：
  - `CDDS_PREPAINT` → `CDRF_NOTIFYITEMDRAW` 通知系统按列通知。
  - `CDDS_ITEMPREPAINT` → `CDRF_SKIPDEFAULT | CDRF_NOTIFYPOSTPAINT`：跳过系统默认画，并在 post-paint 阶段自画。
  - `CDDS_ITEMPOSTPAINT` → `paint_header_item` 自绘背景 + 1px 分割线 + 文字 + 排序箭头。

### paint_header_item 关键点

- **背景**：`palette.surface`（默认）/ `palette.surface_hover`（`CDIS_HOT`）；hot 状态必须自己画——`CDRF_SKIPDEFAULT` 把 native hot 高亮一起跳过了。
- **分割线**：列矩形右侧 1px `palette.border`，不延伸到相邻列。
- **文字**：
  - 字号：`WM_GETFONT` 读 header 当前的 semibold font（与 `theme_header` 写入的一致）。
  - 颜色：`style_.row_foreground.value_or(p.text)`，disabled 时降级为 `p.text_secondary`。
  - 对齐：`HDF_JUSTIFYMASK` 映射到 `DT_LEFT`/`DT_CENTER`/`DT_RIGHT`；`HDF_RTLREADING` 映射到 `DT_RTLREADING`。
  - padding：`DpiScale::logical_to_pixels(6)`，按 DPI 缩放。
- **排序箭头**：
  - `HDF_SORTUP` / `HDF_SORTDOWN` 替换：原本 native 自带的箭头被 `CDRF_SKIPDEFAULT` 一并抑制，必须自己画。
  - 位置：列右侧的固定 box（`[rc.right - sort_gap - sort_size, rc.right - sort_gap]`），尺寸按 DPI 缩放。
  - 形状：`fill_polygon` 三角形，2:1 宽高比（与 native Win32 排序箭头风格一致）。
  - 颜色：caption 同色。

### 评审发现并修复的 bug

- **排序箭头越界**：原版把三角形中心 `cx` 算成 `rc.right - sort_gap + sort_size/2`，导致三角形右顶点在列矩形外，覆到右侧分割线和下一列。修复：`cx = rc.right - sort_gap - sort_size/2`，三角形完整落在预留 box 内。

## TabControl active border

### 实现

- `paint_tab` 在画完 caption 之后，如果是 selected Tab，沿底部画一条 2 device px（按 DPI 缩放）的 `palette.accent` 横条。
- **inset by radius**：accent 横条左右两边各缩进 `corner_radius_control`，避免圆角处 accent 突出到圆外。半径为 0（未选中 Tab）时也安全，inset 等于 0。

### Tab padding DPI 化

- 原 `const int pad = 6;`（设备像素）改成 `const DpiScale dpi{dpi_of(hwnd())}; const int pad = dpi.logical_to_pixels(6);`，125/150/200% DPI 下边距与设计语言保持一致。

### 评审发现并修复的 bug

- **accent 突破圆角**：原版 accent 是 `cd->rc` 整宽的矩形，selected Tab 的圆角底部被 accent 涂出两小块 accent 色"小角"。修复：accent 左右各 inset `radius`，与圆角半径对齐。
- **重复 `dpi_of(hwnd())`**：构造 `DpiScale` 后立即在字体查询处又调一次。修复：复用 `dpi.dpi()`。

## 已知限制

- **HDF_BITMAP / HDF_IMAGE 格式未自绘**——遇到带位图/图标的列会回退到 native 绘制（caption 会回到系统默认色）。当前 samples 没有用这两个格式，留给后续 CP 跟进。
- **HDF_OWNERDRAW 未特殊处理**——应用的 owner-draw 头会被我们的自绘覆盖。如果将来库需要支持自定义列头样式，需要在 `CDDS_ITEMPREPAINT` 检查 format 并对 owner-draw 项返回 `CDRF_DODEFAULT`。
- **列宽 dpi 缩放**：HDITEM 不需要我们处理，系统自带 DPI 缩放。

## 测试

- `NativeFrameUISmokeTest` 0.95 s；3/3 通过。
- 视觉验证：`NativeFrameUIThemeDemo`（含 3 列 ListView）+ `NativeFrameUIControlsPlayground`（含 Tab 切换 + ListView）。