# Original Feasibility Analysis

<!-- markdownlint-disable -->

清楚。BCGControlBar Pro 本质上是一个以 MFC 为基础的商业界面组件框架，包含工具栏、停靠窗口、Ribbon、属性窗口、Tab、状态栏、Visual Manager、主题等功能。

如果目标是“独立的、纯 Win32、基于资源文件的静态库”，建议不要直接封装 BCGControlBar Pro，而是参考其功能和交互方式，重新设计一套不依赖 MFC 和 BCG 的 Win32 UI 框架。

## 一、可行性结论

方案总体可行，但需要明确两点：

1. **不能简单把 BCGControlBar Pro 封装进一个独立 Win32 静态库**
   - BCGControlBar Pro 依赖 MFC。
   - 涉及商业授权。
   - 其控件、资源、主题实现不能直接复制或重新发布。
   - 最终库仍然会依赖 BCG 和 MFC，不是真正的独立库。

2. **可以重新实现一套类似能力的 Win32 资源界面库**
   - 使用纯 Win32 API、COMCTL32、GDI/GDI+ 或 Direct2D。
   - 使用 `.rc`、Dialog、Menu、Toolbar、String、Bitmap、Icon 等资源。
   - 以静态库 `.lib` 形式交付。
   - 不依赖 MFC 和 BCG。
   - 通过统一 API 提供类似 BCG 的界面组织能力。

建议项目定位为：

> 一个基于 Win32 和 Windows Resource 的企业级桌面界面框架，提供对话框、工具栏、菜单、状态栏、Tab、停靠窗口、主题、DPI 和布局管理能力。

可以暂定名称为：

```text
WinResourceUI
Win32ResourceUI
NativeWinUI
```

## 二、建议的总体架构

虽然最终可以合并成一个静态库，但内部应分层设计。

```text
WinResourceUI.lib
|
+-- Core              基础窗口和消息机制
+-- Resource          资源加载和资源管理
+-- Command           命令路由和更新机制
+-- Controls          常用控件封装
+-- Layout            布局和停靠管理
+-- Theme             主题、皮肤和视觉管理
+-- Persistence       窗口状态和布局持久化
+-- Accessibility     键盘和辅助功能支持
+-- Integration       Win32/MFC/应用集成适配
```

### 1. Core 基础层

负责所有 Win32 窗口的生命周期和消息处理：

- `HWND` 生命周期管理
- 窗口类注册
- 消息分发
- 子窗口和父窗口管理
- 窗口属性存储
- DPI 感知
- 键盘焦点
- 鼠标捕获
- Timer 管理
- GDI、HICON、HBRUSH 等资源管理
- C++ RAII 封装

建议使用 Unicode：

```cpp
#define UNICODE
#define _UNICODE
```

公共接口应尽量围绕 `HWND` 和资源 ID 设计，避免暴露过多内部实现。

### 2. Resource 资源层

主要负责从应用程序或静态库中加载资源：

- Dialog
- Menu
- Toolbar
- Accelerator
- String
- Icon
- Bitmap
- Cursor
- Version
- Manifest
- 自定义 `RCDATA`

典型接口可以类似：

```cpp
UiDialog dialog;
dialog.Create(IDD_MAIN_DIALOG, parentHwnd);

UiMenu menu;
menu.Load(IDR_MAIN_MENU);

UiToolbar toolbar;
toolbar.Create(parentHwnd, IDR_MAIN_TOOLBAR);
```

资源加载时需要显式管理资源模块句柄：

```cpp
UiInitialize({
    .resourceInstance = hInstance,
    .dpiMode = UiDpiMode::PerMonitorV2
});
```

不要假设资源一定来自当前 EXE，因为将来可能还要支持资源 DLL 或语言包。

### 3. Command 命令层

BCG 的一个重要能力是命令统一管理。建议设计自己的命令系统：

```cpp
struct UiCommand {
    uint32_t id;
    std::function<void()> execute;
    std::function<bool()> canExecute;
    std::function<bool()> isChecked;
};
```

工具栏、菜单、快捷键和上下文菜单都通过同一个命令路由器处理。

需要支持：

- 菜单命令
- 工具栏命令
- 快捷键
- 菜单状态更新
- Enable/Disable
- Checked/Unchecked
- Radio 状态
- 上下文命令
- 父子窗口命令冒泡

这样可以避免每个控件单独处理命令，便于实现类似 MFC Command Routing 的效果。

### 4. Controls 控件层

第一阶段建议只实现高价值控件：

- Dialog
- Button
- Edit
- Static
- ListBox
- ComboBox
- ListView
- TreeView
- Tab
- Toolbar
- StatusBar
- Rebar
- Splitter
- ProgressBar
- UpDown
- RichEdit

第二阶段再实现：

- Property Grid
- Advanced List
- Search Box
- Breadcrumb
- Navigation Pane
- Outlook Bar
- Ribbon
- 自定义标题栏
- 自定义 Dock Pane

控件实现可以采用两种方式：

1. 尽量使用原生 Win32/Common Controls。
2. 只对需要统一视觉效果的部分使用 Owner Draw 或 Custom Draw。

不建议一开始就全面自绘所有控件，否则工作量和兼容性风险会迅速增加。

## 三、资源文件和静态库的关键设计

这是整个项目中比较容易踩坑的地方。

### 方案 A：应用程序包含资源，静态库只提供代码

```text
Application.exe
+-- Main.rc
+-- Dialogs.rc
+-- Menus.rc
+-- WinResourceUI.lib
```

优点：

- 资源 ID 由应用统一管理。
- 本地化更简单。
- 静态库链接问题少。

缺点：

- 库本身不完全自包含。
- 一些内部控件资源需要由应用工程额外合并。

### 方案 B：静态库包含自己的资源

```text
WinResourceUI.lib
+-- C++ code
+-- Resource object
+-- Internal dialogs
+-- Internal icons
+-- Internal strings
```

需要特别处理：

- 静态库中的资源对象可能被链接器裁剪。
- 资源对象未被普通代码引用时，可能不会进入最终 EXE。
- 需要资源锚点、强制链接或 `.props` 集成。
- x86、x64、Debug、Release 的链接行为都要验证。
- `/OPT:REF`、链接时优化和增量链接可能影响资源是否保留。

推荐的交付形式是：

```text
WinResourceUI.lib
WinResourceUIResources.lib
WinResourceUI.props
WinResourceUI.rc
include/
```

并提供一个明确的资源初始化接口，例如：

```cpp
UiLinkResources();
```

应用启动时调用它，确保资源对象被链接进最终程序。

也可以提供 Visual Studio `.props` 文件，自动完成：

- 引入库目录
- 引入头文件目录
- 链接依赖
- 添加资源文件
- 添加预处理宏
- 设置 Common Controls Manifest

### 资源 ID 规划

库内部资源必须使用独立区间，例如：

```text
0x6000 - 0x6FFF    WinResourceUI 内部资源
0x7000 - 0x7FFF    应用预留
```

或者由库独立生成资源头文件：

```cpp
#define WRUI_IDD_DOCK_FLOATING       0x6001
#define WRUI_IDR_DEFAULT_TOOLBAR     0x6002
```

不要直接使用应用常见的 `IDOK`、`IDCANCEL`、`ID_FILE_EXIT` 等名称作为内部资源 ID，以避免冲突。

## 四、推荐的功能范围

### 第一阶段 MVP

建议先做一个可用的基础版本，不要一开始就追求 BCG 的全部功能。

包含：

- 资源对话框创建
- 基础控件封装
- Menu、Toolbar、StatusBar
- Tab 和 Splitter
- 命令路由
- 基础主题
- 高 DPI 支持
- 图标和字符串资源加载
- 窗口状态保存
- Debug/Release 静态库
- x86/x64 编译
- 一个纯 Win32 示例程序

MVP 应能实现类似下面的应用：

```text
主窗口
+-- 菜单
+-- 工具栏
+-- 左侧导航窗口
+-- 中央 Tab 页面
+-- 右侧属性窗口
+-- 底部状态栏
```

### 第二阶段

实现企业桌面软件最常用的高级能力：

- Docking Manager
- 可浮动窗口
- 自动隐藏窗口
- 窗口吸附
- Splitter 布局
- 布局保存和恢复
- Property Grid
- 高级 Tab
- 多种工具栏布局
- Light/Dark Theme
- 图标主题
- 键盘导航
- 多显示器支持

### 第三阶段

再考虑大型功能：

- Ribbon
- Visual Studio 风格 Docking
- Outlook 风格导航栏
- 完整 Visual Manager
- 自定义渲染引擎
- UI 自动化支持
- 多语言资源包
- 主题编辑器
- 设计器或代码生成器

Ribbon 和完整 Docking 是工作量最大的模块，不建议放入第一期。

## 五、模块接口建议

可以设计成类似以下风格：

```cpp
namespace wrui {

bool Initialize(const UiInitOptions& options);
void Shutdown();

class Dialog {
public:
    bool Create(uint32_t resourceId, HWND parent);
    INT_PTR DoModal(uint32_t resourceId, HWND parent);
    HWND GetHandle() const;
};

class Toolbar {
public:
    bool Create(HWND parent, uint32_t resourceId);
    void SetCommandRouter(CommandRouter* router);
};

class DockManager {
public:
    bool Attach(HWND mainWindow);
    bool Dock(HWND child, DockPosition position);
    bool Float(HWND child);
    bool SaveLayout(const wchar_t* fileName);
    bool LoadLayout(const wchar_t* fileName);
};

class ThemeManager {
public:
    void SetTheme(UiTheme theme);
    COLORREF GetColor(UiColor color) const;
};

}
```

接口设计原则：

- 纯 Win32，不依赖 MFC 类型。
- 句柄型资源和对象型资源分离。
- 公开 API 尽量稳定。
- 内部实现通过 PImpl 或内部类隐藏。
- 避免在公共头文件中暴露复杂 STL 类型。
- 不跨 DLL 边界传递 C++ 对象；虽然当前是静态库，也建议保持这个习惯。
- 提供 C++ API，同时可考虑少量 C API 作为稳定接口。

## 六、技术选型建议

### 编译环境

推荐：

```text
Visual Studio 2019/2022
C++17
Unicode
x86 / x64 / ARM64
CMake + Visual Studio 工程
```

### 系统依赖

主要依赖：

```text
user32.lib
gdi32.lib
comctl32.lib
uxtheme.lib
dwmapi.lib
imm32.lib
ole32.lib
```

根据功能可选：

```text
d2d1.lib
dwrite.lib
windowscodecs.lib
```

### 绘制技术

建议分阶段：

- 基础控件：原生 Win32/Common Controls
- 主题和控件外观：GDI/Buffered Paint
- 高级渲染：Direct2D/DirectWrite 可选
- 不建议第一版完全依赖 Direct2D
- 不建议第一版全面自绘，维护成本太高

### DPI

必须从第一版就支持：

- Per-Monitor DPI Awareness V2
- 100%、125%、150%、200% 缩放
- 多显示器不同 DPI
- 字体缩放
- 图标和位图缩放
- Dock 布局缩放
- 窗口恢复时的 DPI 适配

否则后续补 DPI 的成本很高。

## 七、项目目录建议

```text
WinResourceUI/
|
+-- include/
|   +-- wrui/
|       +-- wrui.h
|       +-- dialog.h
|       +-- toolbar.h
|       +-- docking.h
|       +-- command.h
|       +-- theme.h
|
+-- src/
|   +-- core/
|   +-- resource/
|   +-- command/
|   +-- controls/
|   +-- layout/
|   +-- theme/
|   +-- persistence/
|
+-- resources/
|   +-- wrui.rc
|   +-- resource.h
|   +-- icons/
|
+-- samples/
|   +-- BasicWin32/
|   +-- DockingDemo/
|   +-- ResourceDialogDemo/
|
+-- tests/
|   +-- ResourceLinkTest/
|   +-- DpiTest/
|   +-- LayoutTest/
|
+-- cmake/
+-- docs/
```

最终可以输出：

```text
lib/
+-- WinResourceUI.lib
+-- WinResourceUIResources.lib

include/
+-- wrui/*.h

build/
+-- WinResourceUI.props

samples/
+-- BasicWin32.exe
```

## 八、建议的开发阶段和时间

以 2 名 Win32/C++ 开发人员、1 名测试人员兼职为例：

| 阶段 | 时间 | 主要内容 |
|---|---:|---|
| 可行性验证 | 1～2 周 | 静态库资源链接、Dialog、Toolbar、DPI 验证 |
| 基础框架 | 3～5 周 | Core、Resource、Command、基础控件 |
| MVP 版本 | 4～6 周 | 菜单、工具栏、状态栏、Tab、主题、示例 |
| Docking 和布局 | 5～8 周 | Splitter、浮动、自动隐藏、布局保存 |
| Property Grid 和高级控件 | 4～6 周 | 属性窗口、导航栏、高级 Tab |
| 稳定性和发布 | 3～5 周 | 测试、文档、安装包、兼容性验证 |

基础可用版本大约需要：

```text
3～4 个月
```

接近 BCGControlBar Pro 常用能力的版本大约需要：

```text
6～12 个月
```

如果目标是完整覆盖 Ribbon、Docking、Visual Manager、Property Grid、主题、设计器等能力，则可能需要：

```text
12～24 个月
```

## 九、第一阶段可行性验证内容

建议先做一个两周以内的技术 Spike，验证以下内容：

1. 创建一个不依赖 MFC 的 Win32 EXE。
2. 只链接 `WinResourceUI.lib`。
3. 从资源中创建 Dialog、Menu、Toolbar。
4. 静态库内部资源可以被最终 EXE 正确找到。
5. 在 `/OPT:REF`、x86、x64 下都正常。
6. 支持 Per-Monitor DPI V2。
7. 支持命令路由和菜单状态更新。
8. 关闭窗口后没有 HWND、GDI、USER 对象泄漏。
9. 使用 `.props` 文件可以被其他工程快速集成。
10. 应用不需要修改库的源代码或复制库内部实现。

如果这一阶段无法稳定解决“静态库资源被链接器裁剪”和“资源模块句柄管理”问题，后续整个资源化方案都会受到影响，因此应该优先验证。

## 十、主要风险

### 1. 把资源文件误认为完整 UI 框架

`.rc` 只能描述初始界面结构，无法解决：

- 动态布局
- Docking
- 自动隐藏
- 命令路由
- 主题切换
- DPI 变化
- 自定义交互

所以应该采用：

> 资源负责初始声明，代码负责布局、行为和动态状态。

### 2. Docking 功能工作量较大

Docking 不是简单的 `SetParent`，通常需要处理：

- Dock 树
- Splitter
- 浮动窗口
- 自动隐藏
- 拖拽预览
- Tab 合并
- 窗口恢复
- 多显示器
- DPI
- 最小尺寸
- 最大化和全屏状态

建议不要把 Docking 放到第一版核心里，而是单独作为 `Layout/Docking` 模块。

### 3. MFC 工程集成

如果现有应用是 MFC 工程，需要注意：

- MFC 消息映射与 Win32 消息分发的冲突。
- MFC 窗口类和纯 Win32 窗口类的生命周期差异。
- `HINSTANCE` 和资源模块管理。
- MFC 的 `CWnd` 与原生 `HWND` 的绑定关系。

建议提供一个可选的：

```text
WinResourceUI.MfcAdapter.lib
```

但核心库本身仍然不依赖 MFC。

### 4. ABI 和运行库问题

静态库不是完全跨编译器的二进制组件，需要明确：

- MSVC 工具集版本
- `/MD` 或 `/MT`
- Debug/Release
- x86/x64/ARM64
- C++ 异常配置
- RTTI 配置
- 编译器结构体对齐方式

建议第一版发布：

```text
MSVC + C++17 + x64 + Unicode
```

确认稳定后再扩展平台。

### 5. 授权风险

可以参考 BCGControlBar Pro 的功能形态和交互设计，但不能：

- 复制 BCG 源代码。
- 复制其资源、图标和主题文件。
- 直接修改后重新包装 BCG。
- 使用容易造成误解的 BCG 品牌和类名。
- 在没有授权的情况下分发 BCG 运行库。

如果目标是直接使用 BCG，则应走另一个方案：

```text
MFC Application
+-- BCGControlBar Pro
+-- BCG Adapter Static Library
```

这个方案开发快，但不满足“独立、纯 Win32、无第三方依赖”的要求。

## 十一、建议的最终方案

推荐采用下面的路线：

### 第一版定位

```text
纯 Win32
不依赖 MFC
不依赖 BCG
静态库交付
基于 Windows Resource
支持 x64/x86
支持高 DPI
提供基础主题
提供命令路由
提供基础 Docking
```

### 第一版不做

```text
完整 Ribbon
完整可视化设计器
复杂脚本系统
全面自绘控件
跨平台支持
完全复制 BCG 的 API
```

### 交付物

```text
WinResourceUI.lib
WinResourceUIResources.lib
头文件
Visual Studio .props
CMake 配置
资源模板
示例工程
API 文档
主题说明
资源 ID 规范
版本兼容说明
```

## 十二、最终判断

这个项目技术上是可行的，但应把它看作：

> 重新开发一个轻量版 Win32 UI Framework，而不是简单对 BCGControlBar Pro 做一层封装。

如果仅需要：

- 资源 Dialog
- 菜单
- 工具栏
- 状态栏
- Tab
- 基础主题
- DPI
- 常用控件

那么 3～4 个月可以做出较稳定的第一版。

如果需要接近 BCGControlBar Pro 的完整能力，尤其是：

- Ribbon
- 高级 Docking
- Visual Manager
- Property Grid
- 自动隐藏
- 布局持久化
- 高级主题系统

则应按 6～12 个月以上的框架项目规划，而不是按普通控件封装项目规划。