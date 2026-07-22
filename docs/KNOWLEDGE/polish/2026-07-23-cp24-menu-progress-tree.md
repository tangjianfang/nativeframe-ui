# CP24 — nfui::Menu + ProgressBar 自绘 + TreeView 焦点环

> **Round 5/8** of the autonomous polish loop. Three changes land together
> because each one clears a visual-contract leak observed in CP19–CP23.

## Scope

| 子任务 | 主题                            | 文件                                                                                                          |
|--------|---------------------------------|---------------------------------------------------------------------------------------------------------------|
| CP24-A | `nfui::Menu` 框架化             | `include/nfui/Menu.hpp` · `src/menu/Menu.cpp` · `cmake/nfui_menu.cmake`                                       |
| CP24-B | `ProgressBar` 自绘              | `include/nfui/Controls/ProgressBar.hpp` · `src/controls/ProgressBar.cpp`                                      |
| CP24-C | `TreeView` 焦点环                | `src/controls/TreeView.cpp` (CDDS_ITEMPOSTPAINT)                                                              |
| CP24-D | correctness 审查                | (none — review-only)                                                                                          |

## CP24-A — `nfui::Menu` 框架化

**问题.** `Workbench` 之前用 raw `CreateMenu / AppendMenuW` 直接搭菜单栏,菜单表面是
默认 `COLOR_MENU`,与 `palette.surface` 不同步;Win10/11 的 native 菜单 chrome 与
应用主色调割裂,视觉上像 1995 套壳。

**方案.** 引入新模块 `nfui_menu`,把 HMENU + MENUINFO + palette 的搭配封装在
`nfui::Menu` / `OwnedMenu` / `MenuBuilder` 三件套里,Workbench 改用 fluent builder。
依赖白名单里允许 `nfui_menu → {nfui_core, nfui_theme}` —— 不许它碰 controls /
command / layout / persistence,符合"业务模块不互相侵入"的层次约定。

**API.**

```cpp
class OwnedMenu {
    HMENU handle_{};
    HBRUSH brush_{};   // owned; DestroyMenu does NOT free it
    friend class Menu;
    friend class MenuBuilder;
};

class MenuBuilder {
public:
    MenuBuilder& item(label, command_id, enabled = true) noexcept;
    MenuBuilder& separator() noexcept;
    MenuBuilder& check_item(label, command_id, checked) noexcept;
    MenuBuilder  popup(label) noexcept;  // returns builder targeting the child HMENU
};

class Menu {
public:
    explicit Menu(ThemePalette palette) noexcept;
    OwnedMenu&  bar() noexcept;
    OwnedMenu   make_popup() noexcept;
    MenuBuilder builder(OwnedMenu& menu) noexcept;
    void        apply_palette(OwnedMenu& menu) noexcept;
};
```

**MENUINFO 字段.**
- `dwStyle = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS` —— 让 popup 子菜单也继承 surface
- `hbrBack = palette.surface` —— 我们用 `OwnedMenu::brush_` 跟踪这个刷子,在 `reset()`
  时 `DeleteObject`。`DestroyMenu` 不会释放 brush,所以手动追踪是必须的。
- `brBorder` 不填 —— Win10/11 不读它,留 zero 让系统画边框即可。

**Workbench 迁移.**

```cpp
nfui::Menu menu_(palette_);          // member
menu_.apply_to_bar();
(void)menu_.builder(menu_.bar()).popup(L"&File")
    .item(L"E&xit", IDM_NFUI_EXIT);
(void)menu_.builder(menu_.bar()).popup(L"&Help")
    .item(L"&About", command_about);
SetMenu(hwnd(), menu_.bar().get());
```

Context menu 同样走 fluent API,Workbench 不再直接持有 `HMENU` 字段。

## CP24-B — `ProgressBar` 自绘 chrome

**问题.** ComCtl32 在 modern 主题下对 `PBS_SMOOTH` 的渲染在 light/dark/HC 三种
调色板下都偏灰,与 `accent` 按钮色阶不一致;`PBM_SETBARCOLOR` 也不能彻底关掉
ComCtl32 的 chrome。结果就是 workbench 的进度条在 light 模式下读作"灰条"。

**方案.** 沿用 CP23-A `StatusBar` chrome subclass 的模式:

1. `create_native` 之后 `SetWindowSubclass(...)` 安装 chrome proc
2. chrome proc 先于 base 跑 (last-installed-runs-first),拦截 `WM_PAINT` 后 return 0
3. `paint_progress_chrome()` 在离屏 `MemoryDC` 里画:
   - `palette.background` 清窗四角
   - `fill_rounded_rect(track, p.border)` 用 `theme_metrics().corner_radius_control`
   - `fill_rect(bar, accent)` 计算 `pos - low / high - low` 填充
   - disabled 时 fallback 到 `text_secondary`(WCAG AA cleared)

`PBM_GETRANGE` / `PBM_GETPOS` 在 paint 阶段读出 — 这样 `set_pos` 之类的业务 API
无需回调,直接读控件当前状态。

**CP19 焦点环.** ProgressBar 不接收键盘焦点,跳过 `CDIS_FOCUS` / `paint_focus_border`,
与既有 chrome 控件(Button / StatusBar)保持一致:可聚焦 → 有焦点环;不可聚焦 → 没有。

## CP24-C — `TreeView` 焦点环

**问题.** TreeView 自绘后(在 CP20),选中行 chrome 是按 `CDIS_SELECTED` 渲染的,
但键盘焦点(箭头键目标行)与选中行解耦后没有视觉提示,无障碍用户看不到当前激活行。

**方案.** 在 `CDDS_PREPAINT` 请求 `CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT`,
然后在 `CDDS_ITEMPOSTPAINT` 用 `paint_focus_border(cd->nmcd.hdc, cd->nmcd.rc,
p.accent, 1)` 画 1px stroke-only 焦点环。

**stroke-only 的原因.** CP20 review 里发现的:TreeView 在 POSTPAINT 用 `fill_rect`
会擦除系统刚画的 +/- glyph、indent guide 线、行文本。`paint_focus_border` 是
1px stacked `FrameRect`,画在 selection 背景之上,但 selection 填充 + 文本 + indent
线仍然可见 —— 满足"焦点提示不破坏 chrome"的契约。

**CDIS_FOCUS vs CDIS_SELECTED.** `CDIS_FOCUS` 是 TreeView 的"键盘焦点行",即使
控件本身不是前台窗口(用户切走之后)仍然存在。`CDIS_SELECTED` 是当前选中行。
CP24-C 只对 `CDIS_FOCUS` 加环,确保环只在键盘激活时出现,鼠标点击后立即消失。

## CP24-D — correctness review

| 项目                        | 结论                                                                  |
|----------------------------|----------------------------------------------------------------------|
| `OwnedMenu` brush 生命周期 | `reset()` 同时 DestroyMenu + DeleteObject; `release()` 保留 brush 由 SetMenu 接管 ✅ |
| `Menu::apply_palette` 幂等  | 先 DeleteObject 旧 brush,再创建并 SetMenuInfo,再回填 owned brush ✅          |
| `Menu::builder()` brush     | builder 内持 brush 整段链式调用,链结束立即析构,无泄漏 ✅                      |
| `ProgressBar` chrome 链序    | SetWindowSubclass 后装先跑,WM_PAINT return 0 短路 PBS_SMOOTH ✅        |
| `ProgressBar` disabled 颜色  | `text_secondary` 满足 WCAG AA 对比 surface track ✅                       |
| `TreeView` POSTPAINT 不擦   | stroke-only `paint_focus_border`,不覆盖系统渲染的 chrome ✅               |
| `nfui_menu` 依赖白名单       | `nfui_menu → {nfui_core, nfui_theme}` 满足 architecture / boundary 检查 ✅ |

**测试结果.**

```
100% tests passed, 0 tests failed out of 3
Total Test time (real) =   5.72 sec
```

- `NativeFrameUISmokeTest` ✅
- `NativeFrameUIBoundaryCheck` ✅(注册 `nfui_menu` 到 `tools/verify_boundaries.ps1`
  allow-list + `docs/ARCHITECTURE.md`)
- `NativeFrameUIArchitectureCheck` ✅(注册 `nfui_menu` 到 `tools/verify_architecture.ps1`
  module map)

## 下一步 (CP25+)

候选方向:

1. **Slider 自绘** — CP19 焦点环已就绪,但 Slider 的 thumb / track 仍是 native
   gray;沿 CP24-B 模式加 chrome subclass。
2. **Header 自绘** — `ListView` header 的 caption 字体在 dark 模式下偏暗(CP21 部分修);
   让 header 的文本色由 palette.text 驱动而非 system color。
3. **DateTime / MonthCalendar 自绘** — V1 不要求,但 dark mode 下用户能感受到。
4. **应用图标** — `wWinMain` 当前没有 brand icon,在 alt-tab 里读作"匿名 exe"。

建议 CP25 选 1(Slider),延续 CP24-B 的 chrome subclass 模式。
