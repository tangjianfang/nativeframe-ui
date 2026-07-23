# CP27 — Visual Audit Report (像素级真实评审)

> **Round 5/8** of the autonomous polish loop.
> 用户反馈 (Round 3): "相差甚远. 很多窗口背景还是灰色的,都没有去实现demo,没有精心打造"。
> 本轮作者**不再依赖代码自述**而**真实运行 13 个 sample exe + PrintWindow 抓像素**,
> 逐张审看,把"代码说有"和"实际渲染"区分开。

## 0. 工具链

- 截图 exe: `tools/visual_audit/screenshot.cpp` (Standalone Win32, PrintWindow + PW_RENDERFULLCONTENT)
- 13 张 BMP 抓自 `out/build/x64-debug/Debug/<sample>.exe`
- 经 PowerShell `System.Drawing` 批量转 PNG (PowerShell 接受透明 alpha, BMP 拒绝)
- 所有截图位于 `docs/VISUAL_AUDIT/*.png` (与本报告同目录)

## 1. 总览:打分分布

> 评分公式: `chrome_polish × 0.4 + layout_density × 0.3 + status_bar × 0.1 + iconography × 0.1 + palette_consistency × 0.1`
> 满分 100。每一项 **像素级严格**,发现 1 个严重缺陷扣 8-15 分。

| Sample                       | chrome | layout | status | icons | palette | **Total** | 等价 React 等级 |
|------------------------------|--------|--------|--------|-------|---------|-----------|-----------------|
| NativeFrameUIIconGallery     | 95     | 90     | 100    | 100   | 95      | **94**    | ✅ 精品      |
| NativeFrameUIDarkStudio      | 95     | 80     | 95     | 80    | 95      | **89**    | ✅ 精品      |
| NativeFrameUIControlsPlayground | 88  | 85     | 70     | 70    | 85      | **83**    | ⚠️ 接近精品 |
| NativeFrameUIShowcase        | 85     | 70     | 0      | 85    | 90      | **72**    | ⚠️ 接近精品 |
| NativeFrameUIWorkbench       | 75     | 65     | 80     | 75    | 80      | **73**    | ⚠️ 接近精品 |
| NativeFrameUIResourceGallery | 80     | 80     | 0      | 70    | 75      | **67**    | ❌ 仍灰      |
| NativeFrameUICharts          | 80     | 75     | 0      | 65    | 75      | **66**    | ❌ 仍灰 (Spline 渲染失败) |
| NativeFrameUISettingsDemo    | 78     | 80     | 0      | 60    | 75      | **64**    | ❌ 仍灰      |
| NativeFrameUIDialogTour      | 78     | 65     | n/a    | 55    | 75      | **65**    | ⚠️ 接近精品 (小窗口,够干净) |
| NativeFrameUIThemeDemo       | 75     | 70     | 0      | 70    | 85      | **68**    | ❌ 仍灰 (title 截断) |
| NativeFrameUIComponentGallery | 60    | 35     | 0      | 30    | 70      | **44**    | ❌ wireframe (ListView header 平, 滚动截断)|
| NativeFrameUIControls        | 65     | 60     | 0      | 35    | 70      | **56**    | ❌ 仍灰 (窗口标题栏 native)|
| NativeFrameUIMinimal         | 30     | 40     | n/a    | 10    | 30      | **28**    | ❌ broken (窗口看起来空) |

**未加权平均: 67/100**。

### 与之前自我打分的对照

| 轮次 | 自评 | 像素真评 | 差距原因 |
|------|------|----------|----------|
| CP22 | 99   | (未跑像素)| 仅基于代码路径自述 |
| CP23 | 99   | (未跑像素)| 同上 |
| CP24 | 99   | (未跑像素)| 同上 |
| CP25 | 92   | (未跑像素)| 同上 |
| CP26 | 99   | (未跑像素)| 同上 |
| **CP27** | (未定)| **67** | 真实像素评审 |

> **结论**: 过去 5 轮 92-99 的自评都基于"代码 写对了"。这一轮首次基于"像素
> 看起来对"打分,**真实评分 67/100,远低于之前宣称的 99%。** 用户反馈"相
> 差甚远"完全成立——自我评分的"精品"在大批 sample 上只是"勉强够用"。

## 2. 逐张像素发现 (严重缺陷置顶)

### 2.1 NativeFrameUIWorkbench  -- 73/100 (基线)

[截图](Workbench_light.png)

**好消息**:
- P0-1 修复生效 (TreeView 没有黑底, 用户报告已修)
- CP24 ProgressBar 自绘生效 (底部橙色 fill)
- CP25 Slider 自绘生效 (右上角橙色 thumb)
- StatusBar 底部有色块 (橙色 fill)

**仍有问题**:
- **菜单栏** "File / Help": **完全是 native Win32 灰底**, 不是 `palette.surface`。
  CP24-A `nfui::Menu::apply_to_bar()` 我以为生效了, 但截图里仍然是 native chrome。
- **Search 编辑框**: native white box, 无 chrome
- **Header caption "Item"**: 看起来是默认 Tahoma-ish 字体, **CP26 Semibold 修复看不出明显效果** (视觉上跟 body rows 没区别)
- **Splitter (中间灰竖条)**: native chrome
- **布局密度低**: 大量空白, 整体像 wireframe 而非 React UI

### 2.2 NativeFrameUIShowcase -- 72/100

[截图](NativeFrameUIShowcase.png)

**好消息**:
- Sidebar / Workspace / Inspector 三栏布局稳定
- Drop shadow 在 card 上可见 (但很subtle)
- Badge "Stable / Release Ready / Explicit" 颜色合理
- Status bar 缺失的发现: **截图里底部没有可见的 status bar!** 应该填那一段

**严重缺陷**:
- **Sidebar 标题 "NativeFrame UI" 右侧被截断**: 显示 "NativeFrame U",**最后两个字符没了**。
  sidebar 宽度 220 logical px, 字体过大。
- Inspector 右边栏的卡片 "Resource story" 内容被截断 ("nfui_add_resources" 末尾被剪)
- Workspace card 下方有大段空 (OK)

### 2.3 NativeFrameUIDarkStudio -- 89/100 (本轮最高)

[截图](NativeFrameUIDarkStudio.png)

**好消息**:
- dark 模式看起来 real,有 shell 感
- 4 个 navigation 选中条 (Surface) 橙色
- Header card / Preview canvas 大块橙色 accent 都到位
- 底部 metric cards 都可见
- StatusBar 底部有 (右下角细条可见)

**需要改善**:
- Preview canvas 内部那条 "Live preview" 提示框框着 #333 黑底, 文字对比偏弱
- 整体密度比 SampleBalancing 类的 React dark app 还差一截 (主要是 metric cards 之间留白太多)

### 2.4 NativeFrameUIComponentGallery -- 44/100 (最低)

[截图](NativeFrameUIComponentGallery.png)

**严重缺陷** (按视觉冲击排序):
- **ListView header band "Column A / Column B / Column C" 视觉上跟 body rows
  几乎没区别**: 顶部一条线, 三个 caption 看着像是一行 row label, 而非 header。
  CP26 Semibold fix 在这里**没看出差别**。
- **TreeView "Child A / B / C" 行高不一致 / 后面被截**: 项目子树看起来挤挤的
- **ComboBox 是个 plain white box**: 下拉箭头在右, 边框细, 没有 themed chrome
- **IconView 是 32px 的纯深蓝色小方块**, 完全没有 Icon 应该有的样子 (ResourceGallery 同样问题)
- **StatusBar 行完全没有可见的 chrome**: 应该是 CP23-A 自绘了, 但视觉上感觉不到
- **页面 layout 严重溢出**: 880×1320 但 bottom controls (Panel + Splitter) 之后
  "Every control class is exclusive" 文字只露出顶部 1px, 整页滚不下也不可见
- TabControl "Tab 1 / 2 / 3" 还行, active tab 有底部边框

> 这个 sample 是 **演示每种控件** 的核心门面, 分数 44/100 说明 chrome 自绘
> 在 CP26 之后**仍有系统性的视觉扁平问题**。

### 2.5 NativeFrameUIThemeDemo -- 68/100

[截图](NativeFrameUIThemeDemo.png)

**好消息**:
- "Light / Dark / High Contrast" 切换按钮漂亮 (橙色 filled)
- "OK / Cancel" 按钮 OK
- "Active mode: Light" 文字提示右上

**严重缺陷**:
- **窗口标题 "NativeFrame UI ThemeDemo" 被截断**: 截图里只看到顶部一点字形,
  像被卷上去滚出了画面顶部 (或者 title 字体大到占两行, 但窗口不够高)
- **StatusBar 行不见**: "ThemeDemo: ready" 应该在那, 截图里看不到
- TreeView 行只显示 "Project" + "Source" 一半, "Resources" / "Tests" 都被截

### 2.6 NativeFrameUIControlsPlayground -- 83/100

[截图](NativeFrameUIControlsPlayground.png)

**好消息**:
- 3 列布局密度最好的一帧
- Light/Dark/HC 切换按钮有色
- Keyboard navigation tiles (Home/Search/Build/Locked/Run) 显示正确
- State reference tiles (Normal/Hover/Pressed/Focused/Disabled) 都可见
- "Disable sample row" 橙色 filled 按钮 OK
- ListView 小但能看出 "Control/Module" 两列
- TreeView "Playground" root + 子项 OK
- TabControl Design/Preview/Log OK
- ProgressBar 橙色 fill

**待改善**:
- Section 1 "Dynamic create/destroy" 下方有一个**巨大的空矩形** (Layout 留白但看起来 dead)
- Section 3 "Every control wrapper" 列表只有 Button/ListView/TreeView 三行 (实际框架控件 11+ 种) — 应该有更多 wrapper

### 2.7 NativeFrameUISettingsDemo -- 64/100

[截图](NativeFrameUISettingsDemo.png)

**好消息**:
- 标题 "SettingsDemo" 副文本可见
- "Save snapshot" 按钮右上
- Left ListBox (General/Workspace/Release) General 选中 OK
- Form 表单 Profile name / Workspace root / Theme preference 都显示
- 2 个 Checkbox (Enable telemetry / Restore workspace) 选中状态正确
- Theme combo 显示 "Follow system"

**严重缺陷**:
- **Edit 框两个都是 plain white box**, 边框细, 完全没 chrome (不像 themed control)
- **顶部那条红 hairline 太刺眼**: "Pause-state pending changes" 上方一条红线穿过,
  跟整体 cream 调子不和谐
- Save button 是右侧唯一显眼 chrome element, 其他控件都太低调

### 2.8 NativeFrameUICharts -- 66/100

[截图](NativeFrameUICharts.png)

**好消息**:
- Bar chart 12 根橙色柱, 渐变上升
- HBar chart 3 系列 (FY2022 橙 / FY2023 绿 / FY2024 金黄) 5 类
- Line chart 2 series (Revenue 橙 / Costs 绿) 12 点
- Area chart 填充橙色
- 都带 caption

**严重缺陷**:
- **Spline chart 完全空白**: 只有 axis gridlines, **没有曲线**! 这是个严重 bug,
  用户读 chart 看到 4/5 正常会以为这是 demo, 但 Splene 本身就该画曲线但没画
- 坐标轴标签 "1 2 3 4 5 6 7 8 9" 之类角标文字被 axis 标注遮住

### 2.9 NativeFrameUIIconGallery -- 94/100 (最好)

[截图](NativeFrameUIIconGallery.png)

**优点 (CP18 矢量图标系统的代表作)**:
- 14 张 icon card 网格 (chevron × 4, check, close, plus, minus, search, gear, info, warning, dot, hamburger) 全部渲染
- 每张 card 是圆角 surface, 带 palette.gray 描边
- icon 用 GDI primitive (line / poly / arc) 画得不错
- "Icon Buttons (Button::set_icon)" 行底部 4 个图标按钮 (Search/Settings/Add/Close)
  橙色 filled + 白图标, **完全 React 等级**
- 顶部 3 个 theme toggle 按钮 OK

**可改善**:
- "Vector Icon Gallery" caption 字偏小
- Info icon 圈里是 "i", warning 是 ⚠️ 三角 + !

### 2.10 NativeFrameUIResourceGallery -- 67/100

[截图](NativeFrameUIResourceGallery.png)

**好消息**:
- 顶部 "Open resource dialog / Reload assets" 按钮 OK
- Asset checklist 表 (String/Menu/Dialog/Icon/Bitmap/Toolbar marker 全 loaded) OK
- Preview panel 显示 icon (32x32 深蓝方块) + Bitmap (大块深蓝 + 白色网格)

**严重缺陷**:
- **顶部 "File" 是 native Win32 menu bar**: CP24-A `apply_to_bar()` **没生效**,
  `MIM_BACKGROUND` brush 没把 native 灰色染成 palette.surface
- Bitmap 很大块但**实际是什么看不出来** (深蓝/白色的网格? 像是 placeholder
  而不是实际业务图标)

### 2.11 NativeFrameUIDialogTour -- 65/100

[截图](NativeFrameUIDialogTour.png)

**好消息**:
- 3 个 launch 按钮 (Show About modal / Show Preferences modeless / Close modeless) 橙色 filled
- 底部 status strip "about=unset prefs_open=no last=<none>" 可见
- 整体小窗口 (360×240) 干净

**待改善**:
- 窗口背景看起来**略带 green-tinted** 而不是纯 cream, 跟整体调子不太一致
- 没有图标按钮 (Search 等)

### 2.12 NativeFrameUIControls -- 56/100

[截图](NativeFrameUIControls.png)

**好消息**:
- Title 显示, "Switch to dark" 按钮 OK
- ListBox with Apple/Banana/Cherry/Dates 可见, "Banana" 选中
- ListView Item column + Row 0/1/2 可见
- OK / Cancel 按钮 OK
- IconView (深蓝 32x32) 可见

**严重缺陷**:
- 窗口标题栏 **是 native Win32 chrome** (默认灰色 + 白色关/最小化按钮), 没 themed
- 窗口**没有可见的 status bar**
- 整体 layout 太窄 (480×360), 控件排得局促

### 2.13 NativeFrameUIMinimal -- 28/100 (最差)

[截图](NativeFrameUIMinimal.png)

**严重缺陷**:
- 截图里看到的是**一个小窗口, 中间一块淡橙色**, **完全没有文字, 完全没有 "Click me" 按钮**
- 应该是 button + static text, 但**文字不可见** (文字颜色 vs surface 颜色大概一致)
- window 320×120 太小, 像是个 broken shell

> 这是 minimal link 的 demo, 但**视觉上完全 broken**, 应该立刻修。

## 3. 跨 sample 共性严重缺陷 (top 10 必须修)

按视觉冲击从大到小:

1. **Spline chart 完全空白** -- NativeFrameUICharts 主缺陷, 一个 chart 不画用户立刻知道 demo 不全
2. **NativeFrameUIMinimal 文字完全不可见** -- 截图显示 window 几乎空白, broken shell 体验
3. **native menu bar 残留** (ResourceGallery "File" / Workbench "File Help") -- CP24-A `apply_to_bar()` 实际上没把 menu chrome 主题化
4. **很多 sample 缺可见 status bar** (Workbench/DarkStudio/ComponentGallery/ThemeDemo/Charts/SettingsDemo/ResourceGallery/Controls)
   - CP23-A 自绘了 chrome, 但**视觉上没有 status bar 元素出现** (theme/状态文本)
5. **ThemeDemo 标题被截断 / ComponentGallery 滚动溢出** -- 窗口尺寸 vs 内容尺寸偏差, 顶部标题/底部控件被剪
6. **ListView header band 视觉上跟 body rows 一致** -- CP26 Semibold fix **肉眼几乎看不出差别**,
   header 应该是 quantized background + slightly raised visual weight
7. **NativeFrameUIControls / NativeFrameUIMinimal 标题栏 native 灰底** -- 没替换为 themed chrome
8. **Edit 控件 (SettingsDemo / Controls) 是 plain white box** -- Edit 自绘没像 Button 那样有 rounded chrome
9. **ComboBox 是 plain white box** -- 跟 Edit 同一类问题, ComboBox 自绘不彻底
10. **IconView 渲染 32x32 深蓝色方块, 而非应用图标** -- 截图无数次看到一个深蓝小方块,
    应该是 IDI_NFUI_APP 资源加载失败或者 IDA 给的是 placeholder

> 拿 React 的 Material UI / shadcn 对照, 一个**接近精品**的桌面 UI demo 应该至少有:
> - **整页 background 是按层级变化的** (Workspace, Panel, Field 三层)
> - **每个控件圆角 + 微阴影 + 触摸的 hover/press 反馈**
> - **菜单栏、滚动条、标题栏都是 themed chrome, 没有任何 native 灰底突兀**
> - **视觉密度高 (90% 的区域有信息), 不留大片空白**
> 
> **当前 NativeFrameUI: 普遍 60-70, 最好的 IconGallery 90+, 最差的 Minimal 28**。
> 想达 95+ 必须连修上面 10 条, 同时 systemically 提升 chrome polish 和 visual density。

## 4. CP28+ 推进路线建议

### CP28 (修复 top 10)
- Spline chart 真正画曲线
- Menu bar 应用 MENUINFO 让 chrome 真正染 surface
- Minimal sample 调整 button 文字颜色 / surface 对比
- ThemeDemo 标题 clipping (裁剪窗口尺寸 vs 内容 calc)
- ComponentGallery 移除过大空白, 改用更紧的 layout
- ListView header 在 caption 上加大 visual weight (加 divider height + 加 row count?)
- StatusBar 在所有 sample 里确保调用 + 显示文字
- Edit/ComboBox 自绘圆角 chrome
- IconView 用 IDI_NFUI_APP 真正加载 (或者用一个更具表达力的 glyph 替代)

### CP29 (visual density 系统提升)
- 每个 sample 默认 panel 背景 + 卡片背景 + 字段背景三层
- shadow 系统在各 sample 一致应用
- 触控 hover 反馈 (鼠标移上去按钮变色 + 圆角放大 1px)

### CP30 (跨 sample 视觉合同)
- 同样的按钮大小、边距、字段宽度在所有 sample 一致
- 同一个应用的 title bar / icon 在所有 sample 一致 (无需 unit-app-by-app)

### 后续 (CP31-32)
- HC (high-contrast) 主题视觉验证 — 当前还没拿到 theme 切换后的截图
- 真实运行 Workbench × 3 themes × 11 其他 samples, 拼成 33 张对比图

## 5. 测试结果

(无需运行 CTest, 此 round 是 visual review round)
手动检查: 全部 13 张截图存在, 4.2 MB BMP 合计。

## 6. 下一步 (CP28)

按上面 TOP-10 顺序, 第一波建议先修 #1-#5 (硬功能 bug):
1. Spline chart 渲染
2. Menu bar 真正 chrome 主题化
3. Minimal sample 视觉修复
4. StatusBar cross-sample 确认
5. ThemeDemo / ComponentGallery layout 溢出

预计需要 1.5-2 倍 CP26 工作量, 因为 Menu 和 StatusBar 都需要重新确认是否真的 self-paint。

