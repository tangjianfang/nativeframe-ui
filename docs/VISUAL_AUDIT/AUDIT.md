# Visual Audit Report

审计口径：以下结论来自 `PrintWindow` 对真实 Win32 `HWND` 的 WIC PNG 截图，不使用浏览器或合成 mock。工具把主题请求作为 `--theme` 传给 sample，并对标准窗口 chrome 请求对应主题；但除 ThemeDemo 外，这批 sample 没有可由启动参数切换到三种主题的实现。因此，多数组的内容区在三张图中完全相同，dark 最多只改变非客户区标题栏。这不是审计工具伪造失败，而是 sample 缺少运行时主题入口的直接证据。

## NativeFrameUIWorkbench — 评分: 38/100

截图:
- docs/VISUAL_AUDIT/Workbench_light.png
- docs/VISUAL_AUDIT/Workbench_dark.png
- docs/VISUAL_AUDIT/Workbench_hc.png

| 截图 | native gray chrome 残留 |
|---|---|
| light | 左侧 Search Edit 的硬黑边；TreeView 的系统展开线；中间 TabControl、ListView 表头和大片灰白行底；顶部 File/Help 菜单。 |
| dark | 只有标题栏变暗，客户区仍是 light；Edit、TreeView、Tab、ListView 全部保持灰色系统观感，形成最明显的混合主题。 |
| hc | 未进入真正高对比内容主题；同样的灰色 Edit/Tree/Tab/ListView 和低对比浅灰分隔线仍在。 |

**精品控件**：橙色 Slider 和底部 ProgressBar 有统一 accent；三栏 splitter 比裸系统分隔条克制。

**仍然 native gray**：几乎所有主要交互区。左上 Edit 像默认 Win32 输入框，中部 Tab 与 ListView 表头有 1995 年属性页风格，TreeView 线条和空白底没有现代状态层，菜单完全是系统默认。右侧空白 inspector 让整个窗口像未完成的 IDE skeleton。

**与 React UI 库的差距**:
- Material UI/shadcn 会给搜索、导航、tab、table 明确的容器层级、hover/selected state 和 8px 间距体系；这里是控件直接铺在灰底上。
- 信息密度失衡：中间和右侧约八成面积为空，但边框与 splitter 很多，视觉重量都落在系统线框而非内容。
- dark/high contrast 请求只换标题栏或根本无变化，远达不到 React 主题 token 一次切换全树的完整性。

**修复建议**：1. 先彻底 owner-draw Edit/Tree/Tab/ListView，包括表头、行状态和 focus ring；2. 建立真实的 light/dark/HC 运行时切换并统一非客户区；3. 用有内容的 inspector、行高和 section spacing 重做三栏密度。

## NativeFrameUIShowcase — 评分: 68/100

截图:
- docs/VISUAL_AUDIT/Showcase_light.png
- docs/VISUAL_AUDIT/Showcase_dark.png
- docs/VISUAL_AUDIT/Showcase_hc.png

| 截图 | native gray chrome 残留 |
|---|---|
| light | `Switch to dark` 像浅灰系统按钮；卡片虽有圆角，但 Adoption 卡仍带突兀的系统式阴影/立体边。 |
| dark | 只有标题栏变暗，按钮仍写着 `Switch to dark`，说明客户区没有进入 dark；所有浅灰卡片原样保留。 |
| hc | 内容仍是 light，不是高对比；浅灰描边、浅橙选中背景和灰色说明文字对高对比用户不可信。 |

**精品控件**：左侧选中导航、KPI 卡片、三色 badge、右侧 inspector card 已经形成较完整的产品化 chrome；圆角、暖色 accent 和卡片间距比其它 sample 明显成熟。

**仍然 native gray**：主题切换按钮缺少品牌化状态；第一张 KPI 卡的阴影像旧式 raised panel。左上品牌标题被截成 `NativeFrame U`，副标题也发生截断；右侧三张说明卡文字被底边裁切。这些裁切比“控件是不是现代”更致命。

**与 React UI 库的差距**:
- shadcn/Radix 的响应式布局不会允许品牌和卡片正文被容器硬裁切；这里固定尺寸痕迹明显。
- 右侧栏和主画布下半部过空，纵向 rhythm 断裂；React dashboard 通常会用 grid 自动平衡卡片高度与留白。
- 声称 light/dark showcase，但自动 dark/high-contrast 启动均未切换内容，主题 API 可验证性不足。

**修复建议**：1. 消灭品牌、描述和 inspector 的全部文本裁切；2. 为主题切换提供可自动化的 HWND/命令入口并实现 HC；3. 去掉 raised 阴影，统一 card elevation、按钮状态与响应式 grid。

## NativeFrameUIDarkStudio — 评分: 72/100

截图:
- docs/VISUAL_AUDIT/DarkStudio_light.png
- docs/VISUAL_AUDIT/DarkStudio_dark.png
- docs/VISUAL_AUDIT/DarkStudio_hc.png

| 截图 | native gray chrome 残留 |
|---|---|
| light | 客户区固定 dark，但标题栏是 light，形成系统 chrome 与应用 chrome 的断层；底部 StatusBar/resize grip 仍有原生条带感。 |
| dark | 标题栏和客户区一致，是三张中唯一完整观感；底部状态条仍像独立 Win32 带状控件。 |
| hc | 未进入高对比，仍是固定暗色；低对比灰字和橙底灰字继续存在。 |

**精品控件**：左侧 rail 的选中态、圆角 section/card、深色层级、KPI 卡和 accent outline 是本批最接近成品的一组。dark 标题栏下整体视觉是连贯的。

**仍然 native gray**：主要客户区几乎没有裸灰控件，但底部状态条和 resize grip 仍暴露桌面工具感；light 截图的白标题栏尤其像把现代网页面板塞进老窗口。橙色 Preview canvas 占比过大，正文却是低对比灰橙组合。

**与 React UI 库的差距**:
- 颜色层级接近现代 dark dashboard，但大面积高饱和橙色抢走全部焦点，缺少 Material/shadcn 常见的中性 surface 与节制 accent。
- 字号偏小、说明文字对比不足；现代 React 组件通常会通过 semantic foreground token 保证对比。
- 只有固定 dark，light/HC 请求无内容变化，主题系统不完整。

**修复建议**：1. 把橙色大画布降为中性 surface，仅保留局部 accent；2. 提升正文对比和字号；3. 补齐真正 light/HC 主题并统一 status bar/non-client chrome。

## NativeFrameUISettingsDemo — 评分: 54/100

截图:
- docs/VISUAL_AUDIT/SettingsDemo_light.png
- docs/VISUAL_AUDIT/SettingsDemo_dark.png
- docs/VISUAL_AUDIT/SettingsDemo_hc.png

| 截图 | native gray chrome 残留 |
|---|---|
| light | Profile/Workspace 两个 Edit 是硬边灰底；Theme ComboBox 是系统箭头；两行 CheckBox 是白色小方框叠灰色行底。 |
| dark | 仅标题栏深色，表单客户区仍白；Edit/Combo/CheckBox 继续 native gray，混合主题非常明显。 |
| hc | 未进入高对比；低对比浅橙分类选中态、灰说明字和灰输入框照旧。 |

**精品控件**：`Save snapshot` 按钮、左侧 category 选中胶囊、顶部细 accent divider 有一致的暖色 chrome。

**仍然 native gray**：核心正好是设置页最重要的表单控件——Edit、ComboBox、CheckBox——全部像系统默认控件。右侧表单纵向只使用窗口上半部，下面大片空白；复选框的灰条背景像老式 property page。

**与 React UI 库的差距**:
- 与 shadcn Form 相比，label/input/help text 的层级和纵向间距不统一，输入框高度太薄，focus/validation 状态不可见。
- 主题选择器只是数据项，不会改变当前 shell；现代 React 设置页通常立即预览主题并持久化 token 状态。
- 侧栏与表单宽度缺少 responsive max-width，超宽窗口显得内容漂浮。

**修复建议**：1. 优先重画 Edit/Combo/CheckBox 的边框、hover、focus、disabled；2. 让主题偏好即时切换整个窗口并支持 HC；3. 收紧内容 max-width，重建表单 8/12/16px spacing rhythm。

## NativeFrameUIDialogTour — 评分: 42/100

截图:
- docs/VISUAL_AUDIT/DialogTour_light.png
- docs/VISUAL_AUDIT/DialogTour_dark.png
- docs/VISUAL_AUDIT/DialogTour_hc.png

| 截图 | native gray chrome 残留 |
|---|---|
| light | 窗口本身是小型系统 dialog 比例；底部状态文本无容器、无层级，整体仍像传统 Win32 对话框。 |
| dark | 只有标题栏变暗，白色客户区和三枚橙色按钮不变；典型混合主题。 |
| hc | 客户区未进入 HC；橙底白字按钮与细小灰状态文本仍保持普通 light 配色。 |

**精品控件**：三个按钮的填充、圆角和统一尺寸明显使用 framework chrome，至少没有默认灰按钮白边。

**仍然 native gray**：窗口布局和信息架构仍是“按钮垂直堆叠 + 一行 debug 状态”的 1995 套壳。关闭按钮在没有 modeless dialog 时仍占主操作位，状态文本 `about=unset prefs_open=no last=<none>` 是开发诊断串，不是产品 UI。

**与 React UI 库的差距**:
- Radix Dialog 示例会有标题、描述、主次操作、关闭 affordance 和明确的内容层级；这里只是三个同权按钮。
- 320×200 固定布局、极小正文和裸状态串没有现代 spacing、typography 或 empty state。
- dark/HC 请求不作用于客户区，无法证明 dialog 主题覆盖。

**修复建议**：1. 把启动页做成带标题/说明/状态 card 的真实 tour；2. 建立主次按钮层级，移除裸 debug 串；3. 让主窗和实际 modal/modeless dialog 都可被自动切换并截图审计。

## NativeFrameUIResourceGallery — 评分: 61/100

截图:
- docs/VISUAL_AUDIT/ResourceGallery_light.png
- docs/VISUAL_AUDIT/ResourceGallery_dark.png
- docs/VISUAL_AUDIT/ResourceGallery_hc.png

| 截图 | native gray chrome 残留 |
|---|---|
| light | 顶部 File 菜单是系统默认；底部 StatusBar/resize grip 是原生带状 chrome；预览 bitmap 是生硬的蓝白矩形。 |
| dark | 只暗化标题栏，菜单、卡片和整个客户区仍 light；系统菜单与 dark non-client 直接冲突。 |
| hc | 未进入 HC，浅米色 card 描边和细小灰字仍在；菜单也没有 sample 级高对比处理。 |

**精品控件**：两个顶栏按钮、标题 hero card、左右内容 card 统一了圆角与暖色；整体结构比 Workbench 清楚。

**仍然 native gray**：File 菜单和底部状态条最明显。Asset checklist 只是多行裸文本，Preview 的资源图是无说明的色块，像测试夹具而非 gallery。两个大 card 下半部巨大空白。

**与 React UI 库的差距**:
- Material gallery 会把每类资源做成带缩略图、名称、状态、操作的可扫描 item；这里是 debug checklist 文本。
- card 使用了现代外框，但内容密度和 empty-state 设计没有跟上，视觉仍像测试 harness。
- 缺少 dark/HC token 路径，三主题审计实际上只得到一个 light 内容主题。

**修复建议**：1. 把资源清单和 preview 改为真实 item grid/list；2. 主题化 menu/status bar 并补全 dark/HC；3. 消除大面积空 card，增加缩略图说明、状态 badge 和可见交互反馈。

## NativeFrameUIComponentGallery — 评分: 45/100

截图:
- docs/VISUAL_AUDIT/ComponentGallery_light.png
- docs/VISUAL_AUDIT/ComponentGallery_dark.png
- docs/VISUAL_AUDIT/ComponentGallery_hc.png

| 截图 | native gray chrome 残留 |
|---|---|
| light | CheckBox/RadioButton 白方块、Edit 硬边、ComboBox 系统箭头、ListView 灰表头、TreeView 展开框、TabControl 凸边全部残留。 |
| dark | 只有标题栏深色；整页控件仍 light/native gray，所有残留区域与 light 相同。 |
| hc | 未进入真正 HC；灰表头、灰 Tab、浅灰勾选行和细边输入框原样不变。 |

**精品控件**：Default/Hover 按钮、ListBox 选中行、橙色 ProgressBar 有统一 palette；左侧控件类别标签便于对照。

**仍然 native gray**：这是裸控件残留最集中的截图。CheckBox、RadioButton、Edit、ComboBox、ListView header、TreeView、TabControl 每一类都暴露系统灰 chrome。窗口底部 Panel + Splitter 内容被截断，右侧滚动条也像传统 Win32；`StatusBar` 标签和实际 TabControl 位置关系甚至让语义看起来错位。

**与 React UI 库的差距**:
- 与 Material UI component gallery 相比，没有统一 control height、radius、focus ring、disabled opacity 和交互状态矩阵。
- 纵向表单像控件测试清单，而不是有 section/card/anchor navigation 的文档页面；大量不规则横向宽度破坏对齐。
- dark/HC 完全未覆盖客户区，视觉 QA 无法验证组件级主题一致性。

**修复建议**：1. 将上述七类 native gray 控件逐个 owner-draw/theme 化；2. 重排为 card section + 统一列宽，并修复底部裁切；3. 为每类控件展示 hover/focus/disabled/error 以及三主题状态。

## NativeFrameUIThemeDemo — 评分: 48/100

截图:
- docs/VISUAL_AUDIT/ThemeDemo_light.png
- docs/VISUAL_AUDIT/ThemeDemo_dark.png
- docs/VISUAL_AUDIT/ThemeDemo_hc.png

| 截图 | native gray chrome 残留 |
|---|---|
| light | CheckBox/RadioButton、Edit、ComboBox、ListView header、TreeView、TabControl 都保留灰白系统 chrome。 |
| dark | 主题确实切到 Dark，但 CheckBox/RadioButton 仍是刺眼白块，ListView header 和 TabControl 整条发白，Tree/ListBox 右端出现白色块。 |
| hc | 主题确实切到 High Contrast，但 CheckBox/RadioButton、ListView header、TabControl 仍是白色系统岛；控件并未全部服从统一 HC token。 |

**精品控件**：Light/Dark/High Contrast 三个按钮可真实切换；Button、ProgressBar、背景、label 和 non-client 在 dark/HC 下能明显响应。高对比的黑/白/黄方向是可辨识的。

**仍然 native gray**：页面顶部大标题被主题按钮覆盖/裁切；ListView 行内容和 TreeView 子项被控件高度裁掉。dark 模式尤其暴露“黑色 custom shell 中嵌白色 native islands”，看起来像 1995 控件直接贴进暗色网页。TabControl、表头、勾选框是最高冲击缺陷。

**与 React UI 库的差距**:
- React 设计系统的 dark token 会覆盖 trigger、content、border、icon、focus ring；这里主题只覆盖部分 wrapper，内部 native parts 仍泄漏白色。
- gallery 缺少 section card 与稳定 vertical rhythm，控件宽度任意，标题还被首行按钮遮挡。
- HC 虽能切换，但不能只靠黄按钮和黑背景；所有边界、选择、focus、disabled 状态都需系统色语义验证。

**修复建议**：1. 最高优先消灭 dark/HC 下所有白色 native island；2. 修复标题、ListView、TreeView 的裁切；3. 建立逐控件主题一致性矩阵并让自动审计激活每个模式。

## 总览

| Sample | 评分 | 关键缺陷 |
|---|---:|---|
| Workbench | 38 | 核心 Edit/Tree/Tab/ListView 全是灰色系统 chrome，内容极空，主题只换标题栏。 |
| Showcase | 68 | 品牌与 card 文本裁切，主题启动请求未切换客户区。 |
| DarkStudio | 72 | 固定 dark；橙色画布过重、正文对比低、light/HC 不存在。 |
| SettingsDemo | 54 | 表单核心控件 native gray，布局空，主题偏好不改变 shell。 |
| DialogTour | 42 | 三按钮加 debug 串，仍是传统小对话框信息架构。 |
| ResourceGallery | 61 | menu/status 原生，资源展示像测试夹具，三主题内容相同。 |
| ComponentGallery | 45 | 七类控件泄漏系统灰 chrome，底部裁切，dark/HC 未覆盖。 |
| ThemeDemo | 48 | 虽能真切主题，但 dark/HC 出现大片白色 native island，并有多处裁切。 |
| **真实平均分** | **53.5** | 明显低于 92.9；当前整体是“若干 polished shell + 大量未统一的 Win32 内核控件”。 |

## CP28+ 必须修复的 Top-10 缺陷优先级

1. **消灭 dark/HC 的白色 native islands**：先处理 CheckBox、RadioButton、ListView header、TabControl、TreeView/ListBox scrollbar。
2. **统一 Edit/ComboBox chrome**：输入框与下拉框必须有一致 border、radius、hover、focus、disabled 和主题 token。
3. **让三主题真正贯穿 sample 全树**：禁止只切标题栏；提供可自动化命令入口，并同步 non-client、menu、status bar 与所有 child HWND。
4. **修复所有首屏裁切**：Showcase 品牌/inspector、ThemeDemo 标题/ListView/TreeView、ComponentGallery 底部内容必须零裁切。
5. **重做 Workbench 核心 IDE 控件**：Tree/Tab/ListView/header/search 是最大面积的 1995 灰色 chrome，视觉冲击高于任何小按钮。
6. **建立统一 8px spacing 与 control-height 体系**：修正 gallery 中任意宽度、薄输入框、拥挤标签和大面积无意义空白。
7. **降低 DarkStudio 橙色画布视觉重量**：用中性 surface 承载内容，把 accent 限制在选择、进度和关键操作。
8. **将 DialogTour 从测试启动器升级为产品化 tour**：加入标题、说明、主次操作和状态 card，移除裸 debug 串。
9. **把 ResourceGallery 的 debug checklist 改为可扫描的资源卡/列表**：增加缩略图、名称、状态 badge、说明和操作反馈。
10. **建立自动视觉基线与失败门槛**：每个组件至少覆盖 light/dark/HC 的 default/hover/focus/disabled；任何混合主题、白岛或裁切都应阻止 CP 验收。
