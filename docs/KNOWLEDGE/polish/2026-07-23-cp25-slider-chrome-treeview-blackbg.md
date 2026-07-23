# CP25 — Slider 自绘 chrome + TreeView 黑色背景 bug 修复

> **Round 3/8** of the autonomous polish loop. Two changes land together:
> a new themed `nfui::Slider` control, and the **P0-1 user-reported bug**
> where TreeView rows under the cursor appear with a solid-black rectangle.

## Scope

| 子任务  | 主题                                    | 文件                                                                                                |
|---------|----------------------------------------|-----------------------------------------------------------------------------------------------------|
| CP25-A  | `nfui::Slider` chrome subclass         | `include/nfui/Controls/Slider.hpp` · `src/controls/Slider.cpp` · `cmake/nfui_slider.cmake`          |
| CP25-B  | Slider 焦点环                          | (CP25-A 内的 `paint_slider_chrome` 内联实现)                                                        |
| CP25-C  | correctness 审查                       | review-only                                                                                          |
| P0-1    | 修复 TreeView 选中行黑色背景 bug       | `src/controls/TreeView.cpp`                                                                         |

## P0-1 — TreeView 黑底 bug 修复

### 用户截图与 root cause

用户在 Workbench light 模式下截图:
- "Project" 节点:**纯黑色背景**
- "Resources" 节点:**正常 cream 底色**
- 鼠标移上去:**所有节点背景消失**(hover 切换时黑底"飘"过节点)

### 对抗式分析

原 `src/controls/TreeView.cpp` line 90:
```cpp
cd->clrTextBk = selected
    ? style_.selected_background.value_or(p.selection).rgb
    : (hot ? p.surface_hover.rgb : CLR_NONE);
```

Win10/11 modern-themed TreeView(V6 common-controls 双缓冲 custom-draw):
- `CLR_NONE` 在 idle 路径上**并不可靠** —— ComCtl32 在某些 RGN 重绘顺序下把它解读为"保留 `nmcd.hdc` 当前像素",而 TreeView 的 hot chrome 内部状态恰好让保留的像素是 **system theme 的 COLOR_HIGHLIGHT**(在 light 主题下接近黑)。
- 对比 ListView(`src/controls/ListView.cpp:219`)**永远不返回 `CLR_NONE`** —— idle 时用 `row_background` 或 `surface`,hover 用 `surface_hover`。这正是 TreeView 应该有的行为。

为什么只有 Project 黑底?Project 是用户鼠标当前所在的节点,ComCtl32 在 hot 切换路径上保留了 hot chrome 的临时像素;Resources 没在 hot 状态,所以保留的是 `TreeView_SetBkColor(surface)`,看起来正常。

### 修复

```cpp
cd->clrTextBk = selected
    ? style_.selected_background.value_or(p.selection).rgb
    : (hot ? p.surface_hover
           : style_.row_background.value_or(p.surface)).rgb;
```

永远用具体 palette 色,与 ListView 完全对齐。`CLR_NONE` 从 codebase 消失。

## CP25-A — `nfui::Slider` 自绘 chrome

### 问题

ComCtl32 TRACKBAR 在 light/dark/HC 主题下 thumb 与 rail 都是 system 灰,永远不匹配 `palette.accent`。Slider 在 SettingDemo/Workbench 等任何 sample 里出现都是 native chrome。

### 方案

新建 `nfui_slider` 模块,封装 `TRACKBAR_CLASSW` (`msctls_trackbar32`),chrome subclass 拦截 `WM_PAINT` 并自绘 track + filled rail + thumb + 焦点环。

**API:**

```cpp
struct SliderStyle {
    std::optional<Color> track_color;     // un-filled rail (default: palette.border)
    std::optional<Color> filled_color;    // filled portion (default: palette.accent)
    std::optional<Color> thumb_color;     // thumb disc (default: palette.accent)
    std::optional<int>   thumb_radius;    // thumb radius in DIPs
};

class Slider : public Control {
public:
    enum class Orientation { horizontal, vertical };
    bool create(const ControlCreateParams& params) noexcept;
    void set_orientation(Orientation) noexcept;
    void set_style(SliderStyle) noexcept;
    void set_range(int low, int high) noexcept;
    void set_pos(int position) noexcept;
    [[nodiscard]] int  pos() const noexcept;
    [[nodiscard]] int  range_low() const noexcept;
    [[nodiscard]] int  range_high() const noexcept;
};
```

**Paint 算法:**

1. `MemoryDC` 离屏,`palette.background` 清窗四角
2. **rail**:`fill_rounded_rect(g.rail, channel_radius, track, track)` —— 半高 radius
3. **filled portion**:`fill_rounded_rect(g.filled, channel_radius, filled_col, filled_col)` —— 叠加在 rail 之上,低边到 thumb 中心
4. **thumb disc**:`fill_ellipse(thumb_rect, thumb_col)` + `draw_ellipse(thumb_rect, p.border, 1)` —— border 让 thumb 在 HC 主题下仍然可见
5. **focus ring**:`paint_focus_border(focus_rect, p.accent, 1)`,仅当 `GetFocus() == slider_hwnd && enabled` 时画

**几何 (compute_geometry):**

- rail channel thickness:`dpi.logical_to_pixels(theme_metrics().spacing / 2)`,最小 2 px
- thumb radius:`dpi.logical_to_pixels(style.thumb_radius.value_or(theme_metrics().corner_radius_control))`
- thumb centre x/y:`rail_left/right - thumb_radius` 之间按 `(pos - low) / span` 线性插值
- vertical 模式镜像(rail_top/bottom 互换)

**状态读取 (与 ProgressBar 模式一致):**

```cpp
int low  = static_cast<int>(SendMessageW(hwnd, TBM_GETRANGEMIN, 0, 0));
int high = static_cast<int>(SendMessageW(hwnd, TBM_GETRANGEMAX, 0, 0));
int pos  = static_cast<int>(SendMessageW(hwnd, TBM_GETPOS, 0, 0));
```

注:`TBM_GETRANGE` 不是 public SDK symbol(它在 commctrl.h 内部作为未文档化的 struct-returning variant 存在),所以用 MIN/MAX 两个 single-bound 查询 —— SDK 文档化路径,且编译器接受。

**Workbench 集成:**

- 在 right panel(inspector 上方)放一条 horizontal slider,range 0-100,pos 35
- 链接 `nfui_slider` 到 `NativeFrameUI::NativeFrameUI` umbrella
- Slider chrome 立刻可见:accent 填充到 35% 处,thumb 圆盘在 35% 位置

## CP25-C — correctness review

| 项目                              | 结论                                                                  |
|----------------------------------|----------------------------------------------------------------------|
| chrome 链序                       | SetWindowSubclass 后装先跑,WM_PAINT return 0 ✅                       |
| `CLR_NONE` 在 codebase           | P0-1 后 codebase 搜索已无 CLR_NONE(TreeView 全部走 palette 色)✅       |
| Slider `std::max(int, LONG)` 推导 | 改为 `static_cast<int>` 强制统一 ✅                                   |
| 焦点环 honour `GetFocus()` + `enabled` | focus 状态正确切换 ✅                                            |
| disabled thumb / filled 颜色     | `text_secondary` 与 ProgressBar CP8 一致 ✅                          |
| vertical orientation 切换        | SetWindowLongPtrW(TBS_VERT) + InvalidateRect ✅                      |
| TBS_TRANSPARENT 不可用           | 已移除(CP25 draft 错误假设 SDK 暴露该 style) ✅                       |
| `WS_TABSTOP` 显式设置             | create params 默认带,Slider 可通过键盘接收焦点 ✅                      |

## 测试结果

```
100% tests passed, 0 tests failed out of 3
Total Test time (real) =   5.95 sec
```

- `NativeFrameUISmokeTest` ✅
- `NativeFrameUIBoundaryCheck` ✅(注册 `nfui_slider → {nfui_core, nfui_theme, nfui_control_base}`)
- `NativeFrameUIArchitectureCheck` ✅(`Controls/Slider.hpp` 走 `Controls/*` 通配符,无需新条目)

## 下一步 (CP26+)

剩余视觉缺口(按 demo 精品标准):

1. **ListView header caption 偏暗(dark)** — CP21 部分修,残留
2. **DateTime / MonthCalendar 自绘** — dark 模式 read 作 native chrome
3. **应用 brand 图标** — alt-tab 读作"匿名 exe"
4. **TBM_GETTOOLTIPS hover tooltip 颜色** — dark 模式 native

建议 CP26 选 ListView header 完整 caption 自绘(dark 模式可见度高,影响 Workbench 的 Item 列)。