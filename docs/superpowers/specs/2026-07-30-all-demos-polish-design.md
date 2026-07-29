# All-Demos Polish — Design

**Date**: 2026-07-30
**Author**: Claude (with user direction)
**Status**: Draft → committed for review

---

## 1. Context

`docs/VISUAL_AUDIT/AUDIT.md` 已经把所有 sample 量化了 — 13 个 demo 平均分 53.5 / 100，最低的 Workbench 只有 38。用户 2026-07-22 明确表态"每一个 demo 必须是精品"，并在 2026-07-30 二次表态对所有 demo 的现状都不满意（界面粗糙、布局混乱）。本次设计覆盖"全 13 demo 精品化"的完整方案。

### 已诊断的根问题（与用户吐槽对齐）

1. **控件层**: dark / HC 下 Edit / ComboBox / CheckBox / RadioButton / ListView header / TreeView / TabControl / StatusBar / Menu / Scrollbar 出现大面积 native gray chrome 残留（白色 islands）。
2. **主题系统**: 除 ThemeDemo 外，13 个 sample 都不能在客户区切换 light/dark/HC；只有标题栏变深，整个内容区保持 light。
3. **布局**: 多 demo 首屏文本被裁切（Showcase 品牌 / ThemeDemo 标题 / ComponentGallery 底部）；空 inspector / debug 串 / 不对齐列宽普遍存在。
4. **设计 token**: 没有统一 spacing / control-height / radius / typography 常量，每个 demo 各自乱写。

### 不在本次范围

- 新增 demo / 新增功能
- 框架 API 重命名 / 大改
- 不动 V1 非目标（Ribbon / Property Grid / Data Grid / plugin / ARM64 / Direct2D）

---

## 2. Goals & Quality Bar

### 量化门槛

| 维度 | 现状 | 目标 |
|---|---:|---:|
| 13 demo 平均分（AUDIT.md 评级） | 53.5 | ≥ 90 |
| 单 demo 最低分 | 38 | ≥ 85 |
| dark 客户区被覆盖的 demo | 1/13 | 13/13 |
| HC 客户区被覆盖的 demo | 1/13 | 13/13 |
| 首屏文本裁切 | 多处 | 0 |
| 裸 debug 串入界面 | 多处 | 0 |

### 段 A → 段 B 衔接门槛

- 段 A（framework）全部 4 个 CP 完成后：**全 demo 平均 ≥ 70** 才能启动段 B。
- 段 B 每个 demo CP 完成后：**单 demo ≥ 85** 才进下一个。

---

## 3. Scope

### 13 个 demo 全覆盖

`Workbench`, `Showcase`, `DarkStudio`, `SettingsDemo`, `ResourceGallery`, `ComponentGallery`, `ControlsPlayground`, `ThemeDemo`, `DialogTour`, `Charts`, `ChartsInteractive`, `IconGallery`, `Minimal`。

### 交付物（每个 demo 三件套）

- **样貌**: 自绘 chrome + 设计 token + 三主题真实贯穿客户区
- **密度**: 修首屏裁切 / 空 inspector / debug 串 / 不对齐列宽
- **识别度**: 区分"产品 demo"和"能力展示 demo"两类视觉身份

---

## 4. Architecture

### 4.1 两段式执行

```
段 A — Framework Foundation（4 个 CP）
  A1: 设计 token + 主题广播基建
  A2: 核心输入控件自绘（Edit / ComboBox / CheckBox / RadioButton）
  A3: 容器控件自绘（ListView header+rows / TreeView / TabControl）
  A4: chrome 控件自绘（StatusBar / Menu / Scrollbar）+ 视觉审计门禁

段 B — Per-Demo Polish（13 个 CP，按分数从低到高）
  B1: Workbench (38)
  B2: DialogTour (42)
  B3: ComponentGallery (45)
  B4: ThemeDemo (48)
  B5: SettingsDemo (54)
  B6: ResourceGallery (61)
  B7: ControlsPlayground (未评分)
  B8: Charts (未评分)
  B9: Showcase (68)
  B10: DarkStudio (72)
  B11: ChartsInteractive (未评分)
  B12: IconGallery (未评分)
  B13: Minimal (未评分)
```

### 4.2 关键技术决策

| 决策 | 选择 | 理由 |
|---|---|---|
| 自绘实现方式 | `SUBCLASSWINDOW` + `WM_PAINT` owner-draw | 不动 demo 调用代码；HWND 仍原生；不引入 Direct2D |
| 主题广播机制 | `ThemeBroker` 登记表 + `WM_THEMECHANGED` 派发 | 不重建窗口树；UI 线程内同步 |
| 主题命令入口 | `ID_THEME_LIGHT/DARK/HC` → `WM_COMMAND` → `ThemeBroker::set_theme` | 自动化可驱动；demo 启动参数 `--theme X` 转发到该命令 |
| 高对比色源 | `GetSysColor(COLOR_WINDOW / COLOR_WINDOWTEXT / COLOR_HIGHLIGHT / ...)` | 系统 HC 真实语义，不自造色 |
| 设计 token 单值源 | `include/nfui/design_tokens.hpp` 常量 + `nfui::theme_tokens` 计算 | 避免散落魔数 |
| 视觉审计门禁 | `tools/visual_audit/gate.ps1` 进 CI | 任何回归立刻拦下 |
| 测试范围 | 现有 `NativeFrameUISmokeTest` 扩三主题 + 各 owner-draw 控件的创建/销毁/命令路由 | 保住 wrapper API 不变 |

### 4.3 模块依赖图

```
design_tokens ───► 所有控件绘制
              └──► 所有 demo 布局

theme ───► ThemeBroker ───► 所有 window（非客户区 + menu + status + 子控件）
       └──► theme_tokens ───► 所有控件 palette

controls (自绘版) ───► 所有 demo
```

依赖方向严格遵守 `docs/ARCHITECTURE.md`：core 不依赖 controls/command/layout/theme；controls 不依赖业务；layout 不依赖 controls；theme 不依赖 controls（仅被 controls 单向消费）。

---

## 5. Components & Data Flow

### 5.1 设计 token（CP-A1）

```cpp
namespace nfui {
  // 8px 间距体系
  inline constexpr int spacing_xs = 4;
  inline constexpr int spacing_sm = 8;
  inline constexpr int spacing_md = 16;
  inline constexpr int spacing_lg = 24;
  inline constexpr int spacing_xl = 32;

  // 控件高度
  inline constexpr int control_height_sm = 24;   // dense list
  inline constexpr int control_height_md = 32;   // 默认
  inline constexpr int control_height_lg = 40;   // 主操作

  // 圆角
  inline constexpr int radius_sm = 4;
  inline constexpr int radius_md = 8;
  inline constexpr int radius_lg = 12;

  // 字号
  inline constexpr int font_caption  = 12;
  inline constexpr int font_body     = 14;
  inline constexpr int font_subtitle = 16;
  inline constexpr int font_title    = 20;
  inline constexpr int font_hero     = 28;
}
```

DPI 缩放：所有 token 在 `nfui::theme_tokens::resolve(dpi)` 阶段乘 `dpi_scale / 96.0f`。

### 5.2 主题广播（CP-A1）

```cpp
class ThemeBroker {
public:
  static void set_theme(Theme t);     // 设全局 + 广播 WM_THEMECHANGED
  static Theme current();
  static void register_hwnd(HWND, std::function<void(Theme)> on_changed);
  static void unregister_hwnd(HWND);
private:
  static void broadcast(Theme);       // 遍历登记表 + SendMessage
};
```

**广播实现**:
- 登记表 `std::unordered_map<HWND, Handler>`，UI 线程访问
- `Window` 构造时 register（递归 `EnumChildWindows` 一次性把子树登记完）
- `Window` 析构时 unregister
- 主题切换时遍历表 + `SendMessageW(hwnd, WM_THEMECHANGED, 0, 0)`
- 接收方在 `WM_THEMECHANGED` 走默认 `theme_tokens::resolve` + `InvalidateRect(hwnd, nullptr, TRUE)`
- 非客户区：`SetWindowPos` 带 `SWP_FRAMECHANGED` 触发 NC 重画
- Menu / StatusBar：自绘版在 `WM_THEMECHANGED` 重新刷 palette

### 5.3 自绘控件状态矩阵（CP-A2 / A3）

每个自绘控件都覆盖 default / hover / focus / pressed / disabled / error 六态，每态在 light / dark / HC 三主题各有 palette。

绘制顺序（Edit 为例）:
1. `fill_rounded_rect(hdc, rect, bg, radius_sm)`
2. `draw_rounded_rect(hdc, rect, border, 1, radius_sm)`
3. focus 时叠加 2px 外扩 focus ring
4. `DrawTextW` 文字用 `palette.on_surface`
5. placeholder 用 `palette.on_surface_variant`

向后兼容：demo 代码 `nfui::Edit edit(parent, id)` 不变；自动拿到 owner-draw 版本；`hwnd()` 仍返回原生 HWND；`SetWindowText/GetWindowText` 正常。

### 5.4 ListView / TreeView 自绘细节（CP-A3）

**ListView**:
- `LVS_OWNERDRAWFIXED`，行高 32px，padding 8px
- header 24px 圆角背景 + sort indicator（▲/▼）
- 行状态：default / hover / selected / focused+selected / disabled
- selection bg = `palette.selection`，文字 = `palette.on_selection`
- 列分隔 1px `palette.divider`

**TreeView**:
- `NM_CUSTOMDRAW` 接管 `CDDS_ITEM` 阶段（cp42 已 fix 反射转发）
- 展开箭头 8x8 chevron 自绘
- indent 16px 步进
- selected state: 整行 pill 背景，不用系统 highlight

**TabControl**:
- tab 高 32px，padding 12px
- active tab: 顶部 2px accent bar + 底色 `palette.surface`
- inactive tab: 底色 `palette.surface_variant`
- hover: 底色 `palette.hover`

### 5.5 Chrome 自绘（CP-A4）

**StatusBar**: bg = `palette.surface_variant`；分隔器 1px `palette.divider`；size grip 4x4 三连点自绘。

**Menu**: popup 圆角 8px + 1px border + elevation 阴影；item 高 28px / padding 8px；icon 16x16；accelerator 右对齐灰色；selected: `palette.selection` bg + `palette.on_selection` 文字。

**Scrollbar**: thumb 圆角 4px；hover 加宽 2px；track 透明；arrow button 自绘或隐藏（推荐隐藏）。

### 5.6 视觉审计门禁（CP-A4）

```powershell
# tools/visual_audit/gate.ps1
$results = foreach ($demo in 13_demos) {
  foreach ($theme in light,dark,hc) {
    capture_png($demo, $theme)
    score_png($demo, $theme)
  }
}
$avg = average($results)
$min = min($results)
if ($min -lt 85 -or $avg -lt 90) { exit 1 }
```

集成进 `.github/workflows/ci.yml`：每次 PR 自动跑；分数不达标阻塞合并。

### 5.7 命令路由（保持不变）

- `ID_THEME_LIGHT` / `ID_THEME_DARK` / `ID_THEME_HC` 由 `ThemeBroker::set_theme` 处理
- demo 启动参数 `--theme X` 解析后 `PostMessage(WM_COMMAND, ID_THEME_X)`
- 现有 `WM_COMMAND` / `WM_NOTIFY` dispatch 路径不动

---

## 6. Per-Demo Polish Plan（段 B）

| CP | Demo | 关键改动 |
|---|---|---|
| B1 | Workbench | 自绘三栏（tree+search / tabs+list / inspector）；inspector 填内容；菜单自绘 |
| B2 | DialogTour | 改成产品 tour（标题 / 说明 / 主次按钮 / 状态 card）；去掉 debug 串 |
| B3 | ComponentGallery | 七类控件加 hover/focus/disabled/error 状态矩阵；修底部裁切 |
| B4 | ThemeDemo | 消灭 dark/HC 白色 island；修标题 / ListView / TreeView 裁切 |
| B5 | SettingsDemo | 表单 8/12/16 间距节奏；Edit / Combo / CheckBox 状态完整；主题偏好即时切换 shell |
| B6 | ResourceGallery | asset 列表改可扫描 item（缩略图 + 状态 badge + 操作）；menu/status 自绘 |
| B7 | ControlsPlayground | 补完三主题 + spacing 节奏 |
| B8 | Charts | 三主题 + 图例 polish（cp40/42 已做轴标题 / 鼠标读数 / 比较切换） |
| B9 | Showcase | 修品牌 / 描述 / inspector 文本裁切；按钮品牌化状态；补真 dark/HC |
| B10 | DarkStudio | 降橙色画布视觉重量；提升正文对比；补 light/HC |
| B11 | ChartsInteractive | 跟 B8 类似 + 交互控件 polish |
| B12 | IconGallery | 图标系统补齐 + grid 间距节奏 |
| B13 | Minimal | 最小 demo 留作 framework 自检（自动验证 CP-A 全部就位） |

---

## 7. Error Handling

- 自绘 owner-draw 任何绘制失败 → 回退到 `DefWindowProc` 默认绘制，不让 demo 黑屏
- `ThemeBroker` 广播时若某 HWND 已 destroy（极少见但 race）→ 用 `IsWindow()` 守卫，失败登记记日志继续
- HC palette 解析失败 → 回退到 dark palette，加 `Result` warning
- 视觉审计 PNG 生成失败 → CI 步骤失败，阻塞合并
- 自绘控件 hit-test 失败 → 走 `DefWindowProc` 默认处理，不阻断用户交互

---

## 8. Testing & Validation

### 自动化

- `NativeFrameUISmokeTest` 扩到三主题下 owner-draw 控件的 create/destroy/SetText/GetText/`WM_COMMAND` 路由断言
- 新增 `NativeFrameUIThemePropagationTest`：构造嵌套窗口树，切主题，断言所有后代收到 `WM_THEMECHANGED`
- `tools/visual_audit/gate.ps1` 跑全 13 demo × 3 主题，分数门槛进 CI

### 手动

- 每个 B 系列 CP 完成后本地三主题肉眼 walkthrough
- Doku / screenshot 更新：`docs/<DEMO>_WALKTHROUGH.md` 同步

### 验收门槛

- 全 13 demo 平均分 ≥ 90
- 单 demo 最低 ≥ 85
- dark/HC 客户区覆盖 13/13
- 0 首屏裁切
- 0 裸 debug 串

---

## 9. Risks & Mitigations

| 风险 | 概率 | 影响 | 对策 |
|---|---|---|---|
| 自绘改动量大回归 | 高 | 高 | 保留所有现有 `nfui::*` wrapper API；SmokeTest 扩三主题 |
| 主题广播漏子控件 | 中 | 高 | `ThemeBroker` register 时 `EnumChildWindows` 子树；加断言 |
| owner-draw hit-test 错 | 中 | 高 | 显式 `PtInRect` 命中 + `SendMessageW(parent, WM_COMMAND, ...)` |
| HC palette 语义错 | 中 | 中 | HC 底色用 `GetSysColor`，accent 仅在边框/选中 |
| 视觉审计字体抖动 | 中 | 中 | 字体回退到 Segoe UI；门槛渐进 80→85→90 |
| 17 个 CP 周期超预期 | 高 | 中 | 段 A 优先于段 B；段 A 平均 ≥ 70 才启动段 B |
| 13 demo 评分口径不一致 | 低 | 中 | `run_audit.ps1` 用同一 PNG capture + 同一评分规则 |

---

## 10. Milestones

- **M1**: CP-A1 完成（设计 token + 主题广播）
- **M2**: CP-A2 完成（核心输入自绘）
- **M3**: CP-A3 完成（容器控件自绘）
- **M4**: CP-A4 完成 + 视觉审计门禁进 CI → 段 A 平均 ≥ 70
- **M5**: CP-B1-B6 完成（最低 6 个 demo 拉精品）
- **M6**: CP-B7-B13 完成（全 13 demo 精品化）
- **M7**: CHANGELOG / RELEASE_NOTES_v1.0.0.md 写"全 demo 精品化"里程碑

---

## 11. References

- `docs/VISUAL_AUDIT/AUDIT.md` — 13 demo 现状评分 + Top-10 缺陷
- `docs/VISUAL_AUDIT/INDEX.md` — 截图清单
- `docs/ARCHITECTURE.md` — 模块依赖规则
- `docs/THEME_GUIDE.md` — 主题系统现状
- `docs/DEMO_MATRIX.md` — 13 demo 登记
- 已有 `cp40` / `cp42` 工作：Charts polish / SettingsDemo 自绘 checkbox
- 用户反馈 memory：`feedback-demo-polish-standard`, `feedback-self-painted-controls-on-dark`, `feedback-paint-parent-body-bg`

---

## 12. Open Questions

无。细节按推荐实现，不阻塞开工。