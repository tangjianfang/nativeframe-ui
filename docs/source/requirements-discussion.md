# Original Requirements Discussion

<!-- markdownlint-disable -->

我会先检查当前工作区是否已有工程、构建系统或约束，再确定项目名并把需求、设计、里程碑和首个可运行骨架落到仓库中。这一步先把项目边界和现有代码摸清，避免规划文档与实际工程脱节；如果目录是空的，我会按纯 Win32 静态库加示例程序的结构初始化。项目建议命名为 **NativeFrame UI**，工程标识使用 `NativeFrameUI`，C++ 命名空间使用 `nfui`。

名称含义明确：面向原生 Win32 桌面应用的界面框架。它不使用 BCG 的品牌或类名，也不暗示二进制兼容关系。

## 项目目标

开发一个独立的纯 Win32 C++ 静态界面库：

```text
NativeFrameUI.lib
```

核心目标：

- 不依赖 MFC、ATL/WTL 或 BCGControlBar Pro
- 基于 Win32 API 和 Common Controls
- 支持 `.rc`、Dialog、Menu、Icon、Bitmap、String 等 Windows 资源
- 支持 Unicode 和 Per-Monitor DPI V2
- 提供窗口封装、消息分发、命令路由、布局、主题及常用控件
- 支持 MSVC x64，后续扩展 x86/ARM64
- 允许现有 Win32 项目渐进式接入
- 参考 BCG 的功能组织和交互方式，但不复制其代码、资源、接口或品牌

## 开发流程

整个项目按七个阶段推进，每个阶段都产生可审查的正式成果。

```text
立项
  ↓
需求分析
  ↓
概要设计
  ↓
详细设计
  ↓
迭代开发
  ↓
系统测试
  ↓
验收发布
```

每个阶段采用“文档评审通过后进入下一阶段”的方式，但开发过程中允许需求和设计受控调整。

## 第一阶段：立项与需求基线

预计 3～5 个工作日。

### 主要工作

- 明确目标应用类型
- 明确目标 Windows 版本
- 明确编译器和运行库
- 确定首版控件范围
- 确定资源管理方式
- 确定现有项目接入方式
- 划定与 BCGControlBar Pro 的法律和技术边界
- 定义性能、稳定性和兼容性要求

### 输出文档

```text
docs/
├── 01-project-charter.md
├── 02-requirements.md
├── 03-non-functional-requirements.md
├── 04-acceptance-criteria.md
└── 05-risk-register.md
```

### V1 需求范围

首版包含：

- 库初始化和关闭
- `HWND` RAII 封装
- 窗口类注册
- 消息分发
- Modeless/Modal 资源对话框
- Button/Edit/ComboBox/ListView/TreeView 封装
- Menu/Toolbar/StatusBar
- TabHost/Splitter
- 命令注册与路由
- Anchor/Dock 基础布局
- Light/Dark/System 主题
- DPI 和字体管理
- 窗口位置及布局持久化
- 键盘导航
- Win32 示例程序

V1 暂不包含：

- Ribbon
- 自动隐藏 Dock Pane
- Visual Studio 风格拖拽停靠
- UI 可视化设计器
- 完整 Property Grid
- 完整 Data Grid
- MFC 适配层
- 插件体系

停靠系统、Property Grid 和高级 Tab 进入 V2，Ribbon 进入单独的可行性评审，不默认承诺实现。

## 第二阶段：概要设计

预计 3～5 个工作日。

### 架构分层

```text
Application
    |
Public API
    |
+----------+----------+----------+
| Controls | Layout   | Command  |
+----------+----------+----------+
| Theme    | Resource | Persist  |
+----------+----------+----------+
| Core / Window / Message / DPI  |
+--------------------------------+
| Win32 / Comctl32 / UxTheme/DWM |
+--------------------------------+
```

### 模块规划

| 模块 | 职责 |
|---|---|
| `core` | 初始化、窗口、消息、句柄与线程约束 |
| `resource` | 对话框、字符串、图标及资源实例管理 |
| `command` | 菜单、工具栏、快捷键的统一命令路由 |
| `controls` | 原生控件的类型安全封装 |
| `layout` | Anchor、Splitter 和区域布局 |
| `theme` | 系统、浅色、深色主题与绘制参数 |
| `dpi` | Per-Monitor V2、尺寸、字体和图标缩放 |
| `persistence` | 窗口状态和布局保存、恢复 |
| `diagnostics` | 断言、错误信息、句柄泄漏辅助检查 |

### 关键架构决策

- 公共接口使用 `HWND`、资源 ID 和轻量值类型。
- 核心库不依赖 MFC。
- 默认不接管应用消息循环，只提供可选消息预处理。
- 尽量使用 Common Controls，必要位置才进行 Custom Draw。
- 应用业务资源由 EXE 管理，库内部资源独立管理。
- 第一版以 GDI、UxTheme 和 Buffered Paint 为主。
- 公共头文件尽量减少 Windows 头文件污染。
- 不允许跨库边界隐式转移 GDI/USER 句柄所有权。
- 所有控件保留原生 `HWND` 访问能力。

## 第三阶段：详细设计

预计 1～2 周。

需要完成以下设计说明：

```text
docs/design/
├── architecture.md
├── initialization.md
├── window-lifetime.md
├── message-dispatch.md
├── command-routing.md
├── resource-management.md
├── control-model.md
├── layout-system.md
├── theme-rendering.md
├── dpi-scaling.md
├── persistence-format.md
├── error-handling.md
└── public-api-rules.md
```

### 初步 API 形式

```cpp
#include <nfui/nfui.hpp>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    nfui::Application ui({
        .instance = instance,
        .dpi_awareness = nfui::DpiAwareness::PerMonitorV2,
        .theme = nfui::ThemeMode::System
    });

    nfui::MainWindow window;
    if (!window.create(IDD_MAIN_WINDOW)) {
        return 1;
    }

    window.show(show);
    return ui.run();
}
```

公共接口会区分：

- 借用句柄：`HWND`
- 拥有句柄：RAII 类型
- 资源标识：`ResourceId`
- 逻辑尺寸：DPI 无关像素
- 物理尺寸：设备像素

### 错误处理

建议采用：

- 可恢复错误返回 `Result<T>`
- Win32 错误保留 `GetLastError()` 信息
- 编程错误使用 Debug Assertion
- 窗口过程边界不得传播 C++ 异常
- 初始化失败提供可读诊断信息

## 第四阶段：迭代开发

采用 2 周一个迭代。

### Iteration 0：工程骨架

交付：

- CMake 工程
- MSVC 编译配置
- `NativeFrameUI` 静态库
- 示例 Win32 程序
- 单元测试工程
- CI 构建
- 编码规范
- 最小初始化 API

完成标准：

```text
NativeFrameUI.lib 可以生成
示例程序可以启动并退出
CTest 可以执行
x64 Debug/Release 可以编译
```

### Iteration 1：Core 与 Resource

交付：

- `Application`
- `Window`
- 窗口类注册
- 消息分发
- Dialog 资源加载
- 句柄 RAII
- DPI 初始化
- Common Controls 初始化

### Iteration 2：Command 与基础控件

交付：

- Command Registry
- Command Routing
- Menu 和 Accelerator
- Button/Edit/ComboBox
- ListView/TreeView
- 通知消息封装

### Iteration 3：框架控件

交付：

- Toolbar
- StatusBar
- TabHost
- Splitter
- Tooltip
- 图标管理

### Iteration 4：布局、主题与持久化

交付：

- Anchor Layout
- System/Light/Dark Theme
- DPI 动态切换
- 字体管理
- 窗口状态保存
- 布局版本管理

### Iteration 5：集成和稳定化

交付：

- 完整示例程序
- 公共 API 整理
- 集成文档
- 性能与泄漏测试
- Windows 10/11 验证
- V1 发布候选版本

## 第五阶段：测试

测试分为四层。

### 单元测试

覆盖：

- 逻辑尺寸换算
- 命令状态
- 布局计算
- 持久化格式
- 资源 ID 校验
- 主题参数
- 错误对象

### 集成测试

覆盖：

- 窗口创建和销毁
- 父子窗口生命周期
- `WM_COMMAND` 和 `WM_NOTIFY`
- Dialog 资源加载
- 菜单、工具栏和快捷键联动
- DPI 切换
- 主题切换
- 布局恢复

### UI 自动化测试

建议使用 WinAppDriver、FlaUI 或 Windows UI Automation：

- 启动和关闭
- 菜单命令
- Tab 切换
- 键盘导航
- 控件焦点
- 窗口缩放
- 状态保存和恢复

### 人工兼容性测试

测试矩阵：

```text
Windows 10 / Windows 11
x64 Debug / Release
100% / 125% / 150% / 200% DPI
单显示器 / 不同 DPI 的双显示器
System / Light / Dark / High Contrast
简体中文 / 英文
键盘 / 鼠标操作
```

还需监控：

- GDI 对象数量
- USER 对象数量
- 内存泄漏
- 无效窗口句柄
- 消息重入
- 焦点丢失
- 退出阶段访问已销毁窗口

## 第六阶段：验收

V1 必须同时满足以下条件：

1. 示例程序在 Windows 10/11 x64 正常运行。
2. 主项目只需包含头文件并链接 `.lib` 即可使用。
3. 核心库不依赖 MFC、ATL、WTL 或 BCG。
4. Dialog、Menu、Toolbar 和图标资源能稳定加载。
5. 支持 Per-Monitor DPI V2。
6. 菜单、工具栏、快捷键共享命令状态。
7. 窗口在常见 DPI 下无明显裁切或重叠。
8. 连续创建销毁窗口不存在持续的 GDI/USER 对象增长。
9. Debug/Release 构建通过，自动测试通过。
10. 对外 API、集成方式和已知限制有完整文档。
11. 现有项目完成至少一个真实窗口的试点接入。
12. 不包含来源不明或受 BCG 授权限制的代码和资源。

## 版本路线

```text
v0.1  工程骨架、Window、Message、Resource
v0.2  Command、Menu、基础控件
v0.3  Toolbar、StatusBar、Tab、Splitter
v0.4  Theme、DPI、Layout、Persistence
v0.9  主项目试点、兼容性测试、API 冻结
v1.0  首个验收版本

v1.1  Property Grid
v1.2  高级 Tab 和 Data Grid
v2.0  Dock Pane、Floating Pane、布局恢复
```

## 首轮需求确认

在开始详细设计前，需要冻结以下输入：

1. 首版最低支持 Windows 10，还是需要 Windows 7/8.1？
2. 当前项目使用纯 Win32、MFC，还是混合架构？
3. 构建方式是 Visual Studio 工程、CMake，还是两者都要？
4. 首版只支持 x64，还是必须同时支持 x86？
5. 运行库使用 `/MD` 还是 `/MT`？
6. 是否需要库自带内部 `.rc` 资源，还是所有资源均由应用提供？
7. 深色主题是硬性要求还是后续功能？
8. 首个需要接入的真实窗口包含哪些控件？

建议默认基线为：`Windows 10+`、`MSVC 2022`、`C++17`、`CMake + Visual Studio`、`x64`、`/MD`、`Unicode`、`Per-Monitor V2`。需求确认后，第一项工程任务就是建立 `NativeFrameUI.lib`、测试工程和可运行的纯 Win32 示例程序。

按照当前 Windows 桌面软件的主流工程实践，建议采用“现代平台优先、保留扩展能力、首版控制范围”的基线。下面逐项给出可选方案、推荐项和理由。

**1. 最低 Windows 版本**

可选方案：

| 方案 | 优点 | 缺点 |
|---|---|---|
| Windows 7+ | 覆盖老设备 | SDK、DPI、主题、编译器兼容成本很高 |
| Windows 10+ | 兼顾存量企业环境 | 需要处理 Win10 与 Win11 的视觉差异 |
| Windows 11+ | 实现最简单，系统能力最新 | 会排除大量 Win10 存量设备 |

推荐：

```text
最低：Windows 10 1809
主要验证：Windows 10 22H2、Windows 11 23H2/24H2
```

理由：

- Windows 7/8.1 已不适合作为新框架的默认支持目标。
- Windows 10 仍然存在大量企业和工业软件存量设备。
- Windows 10 1809 已具备项目需要的大部分 DPI、DWM 和现代桌面能力。
- 编译时建议将 `_WIN32_WINNT` 设置为 `0x0A00`。
- 对较新的 Windows API 使用运行时能力检测，避免绑定某个 Win11 小版本。

注意：普通 Windows 10 已结束常规支持，因此项目应以 Windows 11 为主要运行平台，Windows 10 作为兼容平台。

**2. 项目架构**

可选方案：

| 方案 | 适用场景 |
|---|---|
| 纯 Win32 | 新项目、轻依赖、希望长期独立维护 |
| MFC | 已有大型 MFC 工程，需要直接使用文档/视图和消息映射 |
| 混合架构 | 主项目包含 MFC，但新界面库希望保持独立 |

推荐：

```text
核心库：纯 Win32
集成方式：原生 HWND 接口
后续可选：独立 MFC Adapter
```

具体拆分：

```text
NativeFrameUI.lib             纯 Win32 核心
NativeFrameUI.MfcAdapter.lib  可选 MFC 适配层，后续版本
```

核心公共 API 只使用：

```cpp
HWND
HINSTANCE
HMENU
HICON
RECT
UINT
WPARAM
LPARAM
```

可以在实现内部使用 STL 和现代 C++，但不要让核心控件继承 `CWnd`。这样纯 Win32、MFC、ATL/WTL 项目都能接入。

**3. 构建方式**

可选方案：

| 方案 | 优点 | 问题 |
|---|---|---|
| 只维护 `.sln/.vcxproj` | Visual Studio 使用直接 | 自动化、复用和多配置管理较弱 |
| 只使用 CMake | 配置集中，适合 CI | 团队需要熟悉 CMake |
| CMake 和手工 VS 工程各维护一套 | 使用方式灵活 | 配置容易漂移，不建议 |
| CMake 生成 VS 工程 | 兼顾 CMake 和 VS | 最适合当前项目 |

推荐：

```text
CMake 作为唯一构建配置来源
Visual Studio 2022 通过 CMake Presets 使用
CMake 生成 Visual Studio 工程
```

推荐版本：

```text
CMake 3.25+
Visual Studio 2022
MSVC v143
Windows SDK 10.0.22621 或更高
C++20
```

虽然 C++17 足够，但新项目更推荐 C++20。公共 API 不应过度依赖 C++20 特性，降低后续接入门槛。

建议提供：

```text
CMakeLists.txt
CMakePresets.json
NativeFrameUIConfig.cmake
NativeFrameUI.props（发布阶段可选）
```

`.sln` 和 `.vcxproj` 作为生成结果，不手工维护、不提交仓库。

**4. CPU 架构**

可选方案：

| 方案 | 适用范围 |
|---|---|
| 仅 x64 | 当前绝大多数桌面软件 |
| x64 + x86 | 仍需加载 32 位插件或部署在旧系统 |
| x64 + ARM64 | 需要原生支持 Windows ARM 设备 |
| x64 + x86 + ARM64 | 产品化框架，但首版测试成本较高 |

推荐：

```text
V1 正式支持：x64
V1 编码约束：保证 64 位安全
V1.1 或有实际需求时：ARM64
x86：仅在现有业务明确需要时增加
```

框架从第一天禁止以下不安全转换：

```cpp
reinterpret_cast<LONG>(pointer)   // 错误
SetWindowLong(...)                // 指针场景不应使用
```

应该使用：

```cpp
LONG_PTR
UINT_PTR
DWORD_PTR
WPARAM
LPARAM
SetWindowLongPtr
GetWindowLongPtr
```

如果现有主项目、插件、驱动配套程序仍是 32 位，则首版需要同步支持 x86。否则不建议为了理论兼容增加一倍构建和测试矩阵。

**5. 运行库 `/MD` 或 `/MT`**

可选方案：

| 方案 | 特点 |
|---|---|
| `/MD` | 使用动态 MSVC Runtime，主流默认方案 |
| `/MT` | CRT 静态链接，部署文件更独立，但二进制更大 |
| 同时发布两套 | 兼容性最好，但产物和测试矩阵增加 |

推荐：

```text
默认：/MD
Debug：/MDd
Release：/MD
```

理由：

- 与大多数 Visual Studio 应用和第三方库一致。
- 安全更新和运行库维护更合理。
- 减少静态 CRT 重复。
- 对复杂桌面应用和插件系统更合适。

同时在 CMake 中把 Runtime 配置做成可覆盖选项：

```cmake
set(CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
```

重要约束：

- 静态库必须和最终应用使用相同 CRT 配置。
- 库的 API 不依赖调用方释放库内部内存。
- 不跨不兼容模块传递需要另一侧释放的 `new`、`FILE*`、STL 容器等对象。

如果目标软件要求单文件部署或处于封闭环境，可以选择 `/MT`，但整个应用及其静态依赖必须统一。

**6. `.rc` 资源归属**

可选方案：

| 方案 | 说明 |
|---|---|
| 所有资源由应用提供 | 简单，但库的独立性弱 |
| 所有资源放入静态库 | 表面自包含，实际容易遇到链接器裁剪和 ID 冲突 |
| 代码与资源混合分工 | 库提供内部资源，应用提供业务资源 |

推荐使用混合方案：

```text
应用负责：业务 Dialog、Menu、Accelerator、字符串、产品图标
库负责：框架内部字符串、光标、默认图标和内部控件资源
```

但不建议单纯依赖静态 `.lib` 自动携带 `.res`。推荐发布结构：

```text
NativeFrameUI.lib
include/nfui/
resources/NativeFrameUI.rc
resources/NativeFrameUIResource.h
cmake/NativeFrameUIResources.cmake
```

最终应用在资源编译阶段包含库的资源：

```rc
#include "NativeFrameUIResource.h"
#include "NativeFrameUI.rc"
```

或者由 CMake 自动把库的 `.rc` 加入最终 EXE。这样比依赖链接器从静态库中抽取资源对象更稳定。

资源规范：

- 库内部符号统一使用 `NFUI_` 前缀。
- 内部资源 ID 使用独立区间。
- 业务资源从应用的 `HINSTANCE` 加载。
- 库资源通过明确的资源模块句柄加载。
- 从设计上预留语言资源 DLL 支持。
- 公共 API 不假定资源必然位于当前 EXE。

**7. 深色主题**

可选方案：

| 方案 | 范围 |
|---|---|
| 后续支持 | V1 只依赖系统默认外观 |
| V1 基础支持 | System、Light、Dark，覆盖框架控件 |
| V1 完整支持 | 所有原生控件、菜单、标题栏和非客户区完全统一 |

推荐：

```text
V1 硬性要求：System / Light / Dark 基础主题
V1 硬性要求：High Contrast 正确回退
不要求 V1 实现所有原生控件的完全自绘
```

原因是主题系统会影响颜色、绘制、资源和控件接口。如果后期才加入，通常需要重构控件层。

V1 应实现：

- 主题切换通知
- 背景、文本、边框、选中、禁用和焦点颜色
- 菜单栏、工具栏、状态栏、Tab、Splitter 主题
- Win11 深色标题栏能力检测
- 系统主题变化监听
- 高对比度模式下停止自定义配色
- 主题切换后刷新字体、画刷和缓存资源

不要承诺所有系统控件都能依靠公开 Win32 API获得完全一致的深色外观。对于兼容性不稳定的系统主题接口，应提供框架自绘或系统外观回退。

**8. 首个真实窗口**

首个窗口既要覆盖框架核心能力，又不能把完整 Docking、Property Grid 等高风险模块提前拉进 V1。

推荐使用一个典型的“资源管理工作台”作为试点：

```text
MainFrame
├── MenuBar
├── ToolBar
├── Left Navigation Pane
│   ├── Search Edit
│   └── TreeView
├── Center Workspace
│   ├── TabHost
│   └── ListView / Detail View
├── Right Inspector Pane
│   ├── Static
│   ├── Edit
│   ├── ComboBox
│   ├── CheckBox
│   └── Button
└── StatusBar
```

V1 试点窗口包含：

- 资源定义的主菜单
- Accelerator 快捷键
- Toolbar
- StatusBar
- TreeView
- ListView
- Tab
- Splitter
- Edit
- ComboBox
- Button/CheckBox
- Tooltip
- 上下文菜单
- System/Light/Dark 切换
- DPI 动态变化
- 窗口尺寸和 Splitter 位置保存
- 菜单、快捷键、工具栏共享命令状态

首个试点暂不包含：

- 完整 Property Grid
- 拖拽 Docking
- 浮动面板
- 自动隐藏
- Ribbon
- MDI
- 完整 Data Grid

右侧 Inspector 首版使用普通控件和两列表格布局实现。等基础设施稳定后，再将它替换为正式的 `PropertyGrid`。

**推荐冻结基线**

建议将首版需求正式冻结为：

```text
项目名称：NativeFrame UI
工程名称：NativeFrameUI
命名空间：nfui

最低系统：Windows 10 1809
主要平台：Windows 11
框架架构：纯 Win32 核心
集成边界：原生 HWND
MFC 支持：后续独立 Adapter
开发工具：Visual Studio 2022 + MSVC v143
构建系统：CMake 3.25+ + CMake Presets
语言标准：C++20
首版架构：x64
后续架构：按需求增加 ARM64，x86 非默认
字符集：Unicode
运行库：/MD，Debug 使用 /MDd
DPI：Per-Monitor DPI Awareness V2
资源方案：应用业务资源 + 库内部资源模板
主题：System、Light、Dark，高对比度回退
绘制技术：Common Controls + GDI/UxTheme/Buffered Paint
首个窗口：菜单、工具栏、树、Tab、列表、Inspector、状态栏
交付形式：静态库、头文件、资源模板、CMake 包、示例程序
```

**建议补充冻结的需求**

除了这八项，还应在详细设计前补充以下决定：

1. **异常策略**：公共 API 默认不抛异常，窗口过程边界绝不传播异常。
2. **日志策略**：库提供日志回调，不绑定具体日志框架。
3. **线程模型**：所有窗口对象只能在创建它的 UI 线程操作。
4. **所有权模型**：明确每个 `HWND` 和 GDI 句柄是拥有还是借用。
5. **本地化**：V1 支持 Unicode 和资源字符串，至少验证简体中文与英文。
6. **可访问性**：V1 保证键盘导航、焦点提示和高对比度；UI Automation 逐步完善。
7. **版本策略**：V1 前 API 可调整，V1 发布后遵守语义化版本。
8. **许可证**：自研代码和资源不得复制 BCGControlBar Pro，第三方依赖必须有许可证清单。
9. **质量门槛**：Release 零编译警告，启用 `/W4`，CI 运行静态分析和测试。
10. **性能基线**：启动、窗口创建、布局刷新和主题切换应建立可测量指标。

这套基线兼顾了当前主流 Windows 桌面开发方式和企业存量环境，也能把第一版的复杂度控制在可交付范围内。

下一步进入“需求基线落库”：我先检查当前工作区结构与已有构建配置，然后建立项目章程、需求规格、非功能需求、验收标准和风险清单。若当前目录没有工程，再同步创建不影响后续详细设计的目录骨架。这一阶段只冻结“做什么”和“达到什么标准”，暂不提前固化类结构等详细实现。完成后会得到一份可评审的 V1 需求基线，并明确进入概要设计前仍需确认的业务项。下一步正式进入 **需求分析与基线冻结阶段**。先不设计具体类和接口，先把 V1 “要做什么、不做什么、如何验收”确定下来。

## 1. 建立需求基线

建议采用以下已确认的技术基线：

```text
项目名称：NativeFrame UI
工程名称：NativeFrameUI
C++ 命名空间：nfui

产品形态：纯 Win32 C++ 静态界面库
最低系统：Windows 10 1809
主要验证平台：Windows 10 22H2、Windows 11 23H2/24H2
开发工具：Visual Studio 2022、MSVC v143
构建系统：CMake 3.25+、CMake Presets
语言标准：C++20
字符集：Unicode
首版架构：x64
运行库：Release /MD、Debug /MDd
DPI：Per-Monitor DPI Awareness V2
主题：System、Light、Dark、高对比度回退
核心依赖：Win32、Common Controls、UxTheme、DWM、GDI
交付形式：静态库、头文件、资源模板、CMake 包、示例程序
```

## 2. V1 功能需求

### 核心框架

- 初始化和关闭界面库
- Common Controls 初始化
- 窗口类注册和注销
- `HWND` 生命周期封装
- 顶层窗口和子窗口创建
- Win32 消息分发
- 模态、非模态资源对话框
- UI 线程约束
- Win32 错误信息封装
- 日志回调

### 资源系统

- 从指定 `HINSTANCE` 加载资源
- Dialog、Menu、Accelerator
- String、Icon、Bitmap、Cursor
- 应用资源与库内部资源分离
- 独立资源 ID 区间
- 简体中文和英文资源验证
- 为后续语言资源 DLL 预留接口

### 命令系统

菜单、工具栏、快捷键和上下文菜单共享同一个命令模型，支持：

- 命令注册和注销
- 命令执行
- 启用和禁用
- 选中和未选中
- Radio 状态
- 命令状态刷新
- 父子窗口命令路由
- 未处理命令回退到原生窗口过程

### 基础控件

V1 封装：

```text
Button
CheckBox
RadioButton
Edit
Static
ComboBox
ListBox
TreeView
ListView
TabControl
Toolbar
StatusBar
Tooltip
ProgressBar
Splitter
Panel
Dialog
```

封装层保留原生 `HWND`，允许应用继续直接调用 Win32 API 和发送原生消息。

### 布局系统

- Anchor 布局
- 水平和垂直布局
- 固定、自动及弹性尺寸
- Splitter 双区域布局
- 最小尺寸约束
- DPI 变化后的重新布局
- 窗口缩放时避免控件重叠和裁切

V1 不实现拖拽停靠、浮动窗口和自动隐藏。

### 主题系统

- System、Light、Dark 模式
- 主题运行时切换
- 主题颜色和尺寸令牌
- 框架控件主题刷新
- 系统主题变化监听
- 高对比度模式自动回退
- 标题栏深色能力检测
- GDI 对象和主题资源缓存管理

### DPI 系统

- Per-Monitor V2
- 逻辑尺寸和物理像素转换
- `WM_DPICHANGED` 处理
- 字体、间距、边框和图标缩放
- 不同 DPI 显示器之间移动窗口
- 保存布局时避免持久化不可移植的物理像素

### 状态持久化

V1 保存：

- 主窗口位置和大小
- 最大化状态
- Splitter 位置
- 当前 Tab
- 主题选择
- ListView 列宽，可作为扩展项

恢复时需要处理显示器缺失、DPI 变化和配置损坏。

## 3. 首个示例程序

示例程序暂定为：

```text
NativeFrameUIWorkbench.exe
```

界面结构：

```text
Main Window
├── Menu
├── Toolbar
├── Left Pane
│   ├── Search Edit
│   └── TreeView
├── Center Workspace
│   ├── TabControl
│   └── ListView
├── Right Inspector
│   ├── Static
│   ├── Edit
│   ├── ComboBox
│   ├── CheckBox
│   └── Button
└── StatusBar
```

示例必须展示：

- 资源菜单和快捷键
- 工具栏命令
- 菜单、快捷键和工具栏共享命令状态
- TreeView 与 ListView 联动
- Splitter 调整区域尺寸
- Tab 切换
- 上下文菜单
- System、Light、Dark 切换
- 运行时 DPI 变化
- 窗口和布局状态恢复

这个程序既是使用示例，也是后续集成和验收载体。

## 4. 非功能要求

### 兼容性

- 支持 Windows 10 1809 及以上版本
- V1 正式支持 x64
- 不依赖 MFC、ATL/WTL、BCG 或其他商业 UI 库
- 应用可直接访问底层 `HWND`
- 新系统 API 必须进行运行时能力检测

### 质量

- MSVC `/W4`
- 项目代码零警告
- 窗口过程边界不传播 C++ 异常
- 无持续增长的 GDI、USER 对象泄漏
- 无未处理的无效句柄访问
- 公共 API 有使用说明
- 核心计算逻辑有单元测试

### 性能

建议将以下目标作为首版基线：

```text
库初始化：典型环境低于 50 ms
主窗口首次显示：典型环境低于 300 ms
普通布局更新：低于一帧，目标 16 ms
主题切换：典型界面低于 200 ms
空闲状态：不持续占用 CPU
```

这些指标应在固定测试设备上测量，不作为所有硬件上的绝对承诺。

### 安全和稳定性

- 对资源 ID、句柄和窗口状态进行有效性检查
- 持久化文件损坏时使用默认布局
- 公共 API 明确句柄所有权
- 不跨边界要求调用方释放库内部内存
- 不复制 BCGControlBar Pro 的代码、资源、类名和私有格式

## 5. V1 明确不做

以下功能推迟到后续版本：

```text
Ribbon
完整 Property Grid
完整 Data Grid
MDI 文档框架
Visual Studio 风格 Docking
浮动工具窗口
自动隐藏面板
可视化 UI 设计器
MFC Adapter
插件系统
完整 UI Automation Provider
全面自绘原生控件
Windows 7/8.1
x86/ARM64 正式支持
```

这样可以防止首版被高级 Docking 和全面自绘拖住。

## 6. 验收条件

V1 发布必须满足：

1. 生成 `NativeFrameUI.lib`，示例程序可正常链接。
2. Release 和 Debug x64 构建通过。
3. 核心库不依赖 MFC、ATL/WTL 和 BCG。
4. 示例程序在目标 Windows 10/11 环境正常运行。
5. Dialog、Menu、Accelerator、Icon 和 String 资源加载正常。
6. 菜单、工具栏和快捷键共享命令及状态。
7. 100%、125%、150%、200% DPI 下没有明显重叠或裁切。
8. 窗口可在不同 DPI 的显示器间移动。
9. System、Light、Dark 和高对比度回退工作正常。
10. 窗口及 Splitter 状态可以保存和恢复。
11. 反复创建、销毁窗口后，GDI/USER 对象不持续增长。
12. 单元测试和集成测试全部通过。
13. 至少完成一个真实业务窗口的试点接入。
14. 提供构建、集成、API、资源 ID 和已知限制文档。

## 7. 需求阶段交付物

下一阶段需要形成这些正式文档：

```text
docs/
├── 01-project-charter.md
├── 02-software-requirements-specification.md
├── 03-non-functional-requirements.md
├── 04-scope-and-roadmap.md
├── 05-acceptance-criteria.md
├── 06-risk-register.md
├── 07-requirements-traceability-matrix.md
└── decisions/
    ├── ADR-001-platform-baseline.md
    ├── ADR-002-pure-win32-core.md
    ├── ADR-003-cmake-build.md
    ├── ADR-004-resource-ownership.md
    └── ADR-005-theme-strategy.md
```

其中需求跟踪矩阵会把每个需求映射到设计模块、开发任务、测试用例和验收条件，防止功能遗漏。

## 8. 仍需确认的业务输入

技术选项可以按推荐基线冻结。开始概要设计前，只需补充三个业务输入：

1. **首个真实业务窗口是什么**
   可以先用 `NativeFrameUIWorkbench` 代替，后续再接入真实窗口。
2. **库的使用范围**
   建议暂定“当前项目内部使用”，暂不承担通用商业组件的兼容责任。
3. **配置存储方式**
   推荐 V1 使用版本化 JSON 文件，默认写入：
   `%LOCALAPPDATA%\<Company>\<Product>\ui-layout.json`。

如果暂时没有这三个答案，可以按上述默认值继续。随后进入 **概要设计阶段**，重点确定模块依赖、窗口生命周期、消息分发、命令路由、资源归属和错误处理模型；概要设计通过后，再进入类、接口和数据结构级别的详细设计。

我将按默认业务输入冻结需求，并开始把需求基线和概要设计落实到当前仓库。先检查工作区现状，确认是否已有工程或未提交改动，再决定只补文档还是同时建立项目骨架。可以。现在正式进入 **概要设计阶段**。以下内容作为 `NativeFrame UI` 的概要设计基线 `HLD v0.1`，后续详细设计、开发任务和测试用例都以此为依据。

## 一、需求冻结结果

未单独指定的业务项按推荐值冻结：

```text
项目名称：NativeFrame UI
工程名称：NativeFrameUI
命名空间：nfui

产品定位：项目内部使用的纯 Win32 静态界面库
首个试点：NativeFrameUI Workbench 示例工作台
最低系统：Windows 10 1809
主要平台：Windows 10 22H2、Windows 11
工具链：Visual Studio 2022 + MSVC v143
构建系统：CMake 3.25+ + CMake Presets
语言标准：C++20
CPU 架构：V1 正式支持 x64
运行库：Debug /MDd、Release /MD
字符集：Unicode
DPI：Per-Monitor DPI Awareness V2
主题：System、Light、Dark、高对比度回退
配置存储：版本化 JSON
核心依赖：Win32、Common Controls、UxTheme、DWM、GDI
```

配置默认保存到：

```text
%LOCALAPPDATA%\<Company>\<Product>\ui-layout.json
```

实际公司名和产品名通过初始化参数传入，不在库内写死。

## 二、系统总体架构

采用分层设计，依赖只能从上层指向下层。

```text
+------------------------------------------------------+
| NativeFrameUIWorkbench.exe                           |
| 示例程序、业务资源、业务命令、业务窗口               |
+------------------------------------------------------+
                         |
+------------------------------------------------------+
| Public API                                           |
| Application、Window、Dialog、Control、Result         |
+------------------------------------------------------+
                         |
+----------------+----------------+--------------------+
| Controls       | Layout         | Command            |
| 控件封装       | 布局计算       | 命令路由           |
+----------------+----------------+--------------------+
| Theme          | Resource       | Persistence        |
| 主题管理       | 资源加载       | 状态持久化         |
+----------------+----------------+--------------------+
| Core           | DPI            | Diagnostics        |
| 窗口及消息     | 缩放和字体     | 日志及错误          |
+----------------+----------------+--------------------+
                         |
+------------------------------------------------------+
| Win32 / Comctl32 / UxTheme / DWM / GDI / Shell       |
+------------------------------------------------------+
```

### 依赖规则

- `core` 不依赖具体控件。
- `resource` 不依赖业务应用。
- `controls` 可以依赖 `core`、`resource`、`theme` 和 `dpi`。
- `layout` 只负责尺寸计算和窗口定位，不负责主题绘制。
- `command` 不直接依赖 Toolbar、Menu 等具体控件。
- `persistence` 不保存原生指针或 `HWND`。
- 示例程序可以使用所有公共模块。
- 公共头文件不能依赖 MFC、ATL/WTL 或 BCG。
- 模块之间不得形成循环依赖。

## 三、构建目标

工程包含以下 CMake Target：

```text
NativeFrameUI
NativeFrameUIResources
NativeFrameUIWorkbench
NativeFrameUITests
```

### `NativeFrameUI`

核心静态库：

```text
NativeFrameUI.lib
```

包含：

- 窗口与消息系统
- 资源加载
- 命令路由
- 控件封装
- 布局
- 主题
- DPI
- 持久化
- 日志和错误处理

### `NativeFrameUIResources`

负责框架内部资源：

- 默认光标
- 内部字符串
- 必要图标
- 框架内部对话框资源
- 资源 ID 声明

资源在最终 EXE 的资源编译阶段合入，不依赖链接器从普通静态库中自动抽取 `.res`。

### `NativeFrameUIWorkbench`

纯 Win32 示例程序，同时承担：

- API 使用示例
- 集成测试载体
- DPI 和主题人工验证
- V1 验收演示
- 未来真实业务窗口的接入模板

### `NativeFrameUITests`

不创建 UI 的逻辑单元测试，以及必要的隐藏窗口集成测试。

## 四、项目目录

建议建立以下目录：

```text
NativeFrameUI/
├── CMakeLists.txt
├── CMakePresets.json
├── LICENSE
├── README.md
├── cmake/
│   ├── NativeFrameUIConfig.cmake.in
│   ├── NativeFrameUIOptions.cmake
│   └── NativeFrameUIResources.cmake
├── include/
│   └── nfui/
│       ├── nfui.hpp
│       ├── application.hpp
│       ├── window.hpp
│       ├── dialog.hpp
│       ├── control.hpp
│       ├── command.hpp
│       ├── layout.hpp
│       ├── resource.hpp
│       ├── theme.hpp
│       ├── dpi.hpp
│       ├── persistence.hpp
│       ├── result.hpp
│       └── version.hpp
├── src/
│   ├── core/
│   ├── resource/
│   ├── command/
│   ├── controls/
│   ├── layout/
│   ├── theme/
│   ├── dpi/
│   ├── persistence/
│   └── diagnostics/
├── resources/
│   ├── NativeFrameUI.rc
│   ├── NativeFrameUIResource.h
│   ├── strings/
│   ├── icons/
│   └── cursors/
├── samples/
│   └── Workbench/
│       ├── CMakeLists.txt
│       ├── main.cpp
│       ├── Workbench.rc
│       └── resource.h
├── tests/
│   ├── unit/
│   ├── integration/
│   └── fixtures/
└── docs/
    ├── requirements/
    ├── architecture/
    ├── design/
    ├── testing/
    └── decisions/
```

## 五、核心对象模型

### Application

`Application` 管理进程级和 UI 线程级初始化：

```cpp
namespace nfui {

struct ApplicationOptions {
    HINSTANCE instance{};
    HINSTANCE resource_instance{};
    DpiAwareness dpi_awareness{DpiAwareness::PerMonitorV2};
    ThemeMode theme{ThemeMode::System};
    std::wstring_view company_name;
    std::wstring_view product_name;
    LogCallback log_callback{};
};

class Application final {
public:
    explicit Application(const ApplicationOptions& options);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] int run();
    [[nodiscard]] bool process_message(MSG& message);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
```

职责：

- 初始化 Common Controls
- 设置 DPI Awareness
- 初始化 COM（按实际功能决定）
- 建立资源上下文
- 建立主题服务
- 建立日志服务
- 提供默认消息循环
- 执行 Accelerator 和 Dialog 消息预处理

约束：

- 同一 UI 线程只能存在一个活动的 `Application` 上下文。
- 构造失败时保留可查询的错误信息。
- 析构时不强制销毁仍由应用拥有的顶层窗口。

### Window

`Window` 是原生窗口的基础封装，不隐藏 `HWND`。

核心能力：

```cpp
class Window {
public:
    [[nodiscard]] HWND hwnd() const noexcept;
    [[nodiscard]] bool valid() const noexcept;

    bool create(const WindowCreateOptions& options);
    void destroy() noexcept;
    void show(int command);
    void enable(bool enabled);

protected:
    virtual LRESULT handle_message(
        UINT message,
        WPARAM wparam,
        LPARAM lparam);

    virtual void on_attached();
    virtual void on_detaching();

private:
    static LRESULT CALLBACK window_proc(
        HWND hwnd,
        UINT message,
        WPARAM wparam,
        LPARAM lparam);
};
```

窗口对象与 `HWND` 的关联使用：

```cpp
SetWindowLongPtr(hwnd, GWLP_USERDATA, ...)
```

禁止使用不具备 64 位安全性的 `SetWindowLong` 存储指针。

### Control

控件默认采用“借用原生句柄”模型：

```cpp
class Control {
public:
    Control() noexcept = default;
    explicit Control(HWND hwnd) noexcept;

    [[nodiscard]] HWND hwnd() const noexcept;
    [[nodiscard]] bool valid() const noexcept;

protected:
    HWND hwnd_{};
};
```

资源对话框中的控件由父 Dialog 创建和销毁，`Control` 不拥有该 `HWND`。由框架动态创建的控件则通过内部拥有型对象管理。

## 六、窗口生命周期

生命周期状态定义为：

```text
Empty
  ↓ create/attach
Creating
  ↓ WM_NCCREATE / WM_INITDIALOG
Alive
  ↓ WM_DESTROY
Destroying
  ↓ WM_NCDESTROY
Detached
```

关键规则：

- `HWND` 只有在 `WM_NCCREATE` 成功后才与 C++ 对象绑定。
- 对话框在 `WM_INITDIALOG` 时完成对象关联。
- 收到 `WM_NCDESTROY` 后必须清除对象和 `HWND` 的双向关联。
- `WM_NCDESTROY` 后，包装对象可以继续存在，但 `hwnd()` 必须返回空。
- 子控件默认由父窗口销毁。
- 禁止在析构阶段向已经失效的 `HWND` 发送消息。
- 窗口过程捕获所有 C++ 异常，异常不得越过 Win32 回调边界。

## 七、消息分发

消息处理采用以下顺序：

```text
Win32 WindowProc
    ↓
对象绑定检查
    ↓
框架预处理
    ↓
派生窗口 handle_message
    ↓
命令或通知路由
    ↓
默认处理 DefWindowProc
```

消息处理结果需要区分：

```cpp
struct MessageResult {
    bool handled{};
    LRESULT value{};
};
```

不能用返回值是否为零判断消息是否已经处理，因为部分已处理消息本来就要求返回零。

Dialog 使用 `INT_PTR` 和 DialogProc 语义，不与普通 WindowProc 的返回规则混用。

## 八、命令系统

命令系统将 Menu、Toolbar、Accelerator 和 Context Menu 解耦。

### 命令模型

```cpp
using CommandId = std::uint32_t;

struct CommandState {
    bool enabled{true};
    bool checked{false};
    bool visible{true};
};

class CommandHandler {
public:
    virtual ~CommandHandler() = default;

    virtual bool execute(CommandId id) = 0;
    virtual std::optional<CommandState> query_state(CommandId id) const = 0;
};
```

### 路由顺序

```text
当前焦点控件
    ↓
当前活动页面
    ↓
所属面板
    ↓
主窗口
    ↓
Application 全局处理器
```

命令状态更新流程：

```text
query_state()
    ↓
Menu Enable/Check
    ↓
Toolbar Enable/Check
    ↓
Context Menu Enable/Check
```

V1 不采用持续空闲轮询更新所有命令，而是在以下时机刷新：

- 菜单即将打开
- 活动页面变化
- 选中项变化
- 命令执行完成
- 应用显式调用刷新

这样可避免空闲时持续占用 CPU。

## 九、资源系统

资源查找不能默认只使用 `GetModuleHandle(nullptr)`。

定义资源上下文：

```cpp
struct ResourceContext {
    HINSTANCE application{};
    HINSTANCE framework{};
    HINSTANCE language{};
};
```

查找顺序建议为：

```text
指定资源模块
    ↓
当前语言模块
    ↓
应用模块
    ↓
框架模块
```

业务资源包括：

- 主窗口 Dialog/Menu
- Accelerator
- 产品字符串
- 产品图标
- 业务 Bitmap

框架资源包括：

- 内部提示字符串
- 默认光标
- 框架占位图标
- 内部控件资源

### 资源 ID

框架资源统一使用：

```text
NFUI_
```

建议内部 ID 区间：

```text
0xE000 - 0xEFFF
```

发布前需要验证该区间与应用、系统命令及 Common Controls 的 ID 不冲突。公共 API 不要求应用使用该区间。

## 十、布局系统

V1 采用“纯计算布局 + Win32 应用”两阶段模型：

```text
Measure
  ↓
Arrange
  ↓
生成目标 RECT
  ↓
BeginDeferWindowPos
  ↓
DeferWindowPos
  ↓
EndDeferWindowPos
```

支持：

- 固定尺寸
- 内容建议尺寸
- 弹性尺寸
- 最小和最大尺寸
- Margin
- Padding
- 水平、垂直排列
- Anchor
- 双面板 Splitter
- 控件显示和隐藏后的重新布局

内部布局使用 DPI 无关逻辑单位：

```text
96 DPI = 100% = 1 logical unit 对应 1 pixel
```

只在最终应用到窗口时转换成设备像素。

### Splitter

V1 Splitter 支持：

- 水平和垂直方向
- 鼠标拖动
- 键盘可调整
- 两侧最小尺寸
- DPI 缩放
- 位置保存和恢复
- 拖动时捕获鼠标
- 父窗口大小变化时保持比例或固定边距

V1 不支持：

- 多级 Dock Tree
- 浮动
- 自动隐藏
- 拖拽停靠预览

## 十一、DPI 设计

DPI 模块提供明确的值类型：

```cpp
struct LogicalSize {
    int width{};
    int height{};
};

struct PixelSize {
    int width{};
    int height{};
};
```

避免在接口中混用逻辑尺寸和物理像素。

顶层窗口收到 `WM_DPICHANGED` 时：

1. 记录新 DPI。
2. 接受系统建议窗口区域。
3. 重建 DPI 相关字体。
4. 重新计算 Toolbar、StatusBar 和 Splitter 尺寸。
5. 重新布局子控件。
6. 刷新 DPI 相关图标。
7. 使主题绘制缓存失效。
8. 重绘窗口。

需要验证：

```text
96 DPI   100%
120 DPI  125%
144 DPI  150%
192 DPI  200%
```

## 十二、主题系统

主题采用语义令牌，不允许控件散落硬编码颜色。

```cpp
enum class ThemeColor {
    WindowBackground,
    ControlBackground,
    TextPrimary,
    TextDisabled,
    Border,
    Accent,
    Selection,
    ToolbarBackground,
    StatusBarBackground,
    Splitter
};
```

主题模式：

```cpp
enum class ThemeMode {
    System,
    Light,
    Dark
};
```

主题切换过程：

```text
更新 ThemeContext
    ↓
销毁旧画刷、画笔和字体缓存
    ↓
通知所有已注册顶层窗口
    ↓
控件重新应用主题
    ↓
重新布局和重绘
```

高对比度启用时：

- 停止应用自定义浅色和深色配色。
- 使用系统颜色。
- 保留焦点矩形。
- 不以颜色作为唯一状态表达。
- 避免低对比度自绘图标。

对未公开或兼容性不稳定的深色系统 API，必须使用运行时能力检测，并提供正常回退。

## 十三、持久化设计

V1 使用带版本号的 JSON：

```json
{
  "schemaVersion": 1,
  "window": {
    "showState": "normal",
    "x": 120,
    "y": 80,
    "width": 1280,
    "height": 800,
    "savedDpi": 144
  },
  "layout": {
    "leftSplitter": 0.22,
    "rightSplitter": 0.76,
    "activeTab": "overview"
  },
  "theme": "system"
}
```

设计规则：

- 布局优先保存比例和逻辑尺寸。
- 读取失败时使用默认布局，不阻止应用启动。
- 配置写入采用临时文件加替换，避免中途崩溃损坏原文件。
- 恢复窗口时检查目标显示器是否仍然存在。
- 窗口必须至少有一部分位于可用工作区。
- 未知字段忽略，便于向前兼容。
- 不把 JSON 类型暴露为公共 API。
- V1 只实现项目需要的内部 JSON 编解码范围，不引入重量级第三方依赖。

持久化后端通过接口隔离，未来可增加 Registry 或应用自定义后端。

## 十四、错误处理和日志

公共 API 采用 `Result<T>` 表示可恢复错误：

```cpp
enum class ErrorCode {
    None,
    InvalidArgument,
    InvalidState,
    Win32Failure,
    ResourceNotFound,
    ClassRegistrationFailed,
    WindowCreationFailed,
    PersistenceFailure,
    UnsupportedOperation
};
```

错误信息应包含：

- 框架错误码
- 相关 Win32 错误码
- 出错操作
- 可读消息

日志等级：

```text
Trace
Debug
Info
Warning
Error
```

库只提供回调：

```cpp
using LogCallback = void (*)(
    LogLevel level,
    std::wstring_view category,
    std::wstring_view message);
```

不绑定 `spdlog` 等日志框架，不默认创建日志文件。

## 十五、线程和重入规则

V1 线程模型：

- 所有窗口和控件只能在创建它们的 UI 线程上操作。
- 后台线程不能直接调用控件方法。
- 后台线程通过 `PostMessage` 或框架 Dispatcher 投递到 UI 线程。
- 主题切换和布局操作只能在 UI 线程执行。
- `SendMessage` 引发的消息重入必须纳入生命周期检查。
- 禁止持锁调用未知业务回调。
- 窗口销毁过程中不再接受延迟布局任务。

V1 可以提供简单的：

```cpp
ui_dispatcher.post([] {
    // 在 UI 线程运行
});
```

不在 V1 实现协程 UI 调度框架。

## 十六、Workbench 概要设计

试点程序布局：

```text
+----------------------------------------------------------+
| Menu                                                     |
+----------------------------------------------------------+
| Toolbar                                                  |
+-------------+-------------------------+------------------+
| Search      | TabControl              | Inspector        |
|-------------|-------------------------|------------------|
| TreeView    | ListView / Detail View  | Edit             |
|             |                         | ComboBox         |
|             |                         | CheckBox         |
|             |                         | Button           |
+-------------+-------------------------+------------------+
| StatusBar                                               |
+----------------------------------------------------------+
```

建议命令 ID：

```text
ID_FILE_EXIT
ID_VIEW_THEME_SYSTEM
ID_VIEW_THEME_LIGHT
ID_VIEW_THEME_DARK
ID_VIEW_TOGGLE_LEFT_PANE
ID_VIEW_TOGGLE_INSPECTOR
ID_EDIT_REFRESH
ID_HELP_ABOUT
```

Workbench 必须证明：

- 资源可正常加载。
- 命令可从菜单、Toolbar 和 Accelerator 执行。
- 菜单和 Toolbar 状态保持一致。
- 三栏布局可以调整。
- TreeView 选择可更新 ListView。
- ListView 选择可更新 Inspector。
- 主题可以运行时切换。
- 不同 DPI 显示器之间移动正常。
- 退出后可以恢复窗口和 Splitter 状态。

## 十七、测试架构

### 单元测试

不依赖可见 UI：

- DPI 换算
- Layout Measure/Arrange
- Splitter 边界
- Command State
- 资源查找顺序
- JSON 序列化和容错
- 窗口恢复区域修正
- 主题令牌解析

### 集成测试

使用隐藏窗口或测试窗口：

- 窗口创建和销毁
- `WM_NCCREATE` 绑定
- `WM_NCDESTROY` 解绑
- 父子窗口生命周期
- `WM_COMMAND`
- `WM_NOTIFY`
- Dialog 初始化
- 资源加载
- Dispatcher 跨线程投递

### 人工/UI 测试

- Toolbar、Menu、Accelerator 联动
- 键盘焦点和 Tab 顺序
- Splitter 拖动
- 不同 DPI 显示器移动
- 深色和浅色切换
- 高对比度回退
- 配置损坏恢复
- 多次启动和退出
- GDI、USER 对象变化

## 十八、概要设计评审门槛

概要设计进入详细设计前必须满足：

- 模块职责没有明显重叠。
- 模块依赖不存在循环。
- `HWND` 和 GDI 句柄所有权有明确规则。
- WindowProc 与 DialogProc 语义明确分开。
- DPI 单位类型不混用。
- 资源模块来源可显式指定。
- 命令系统不依赖具体菜单或 Toolbar。
- 布局计算可以脱离窗口进行单元测试。
- 主题有高对比度回退。
- 配置损坏不影响程序启动。
- 公共 API 不暴露 MFC 或第三方 UI 类型。
- V1 范围中不包含高级 Docking 和 Ribbon。

## 十九、下一阶段任务

概要设计基线确定后，下一步是 **详细设计和工程骨架实现**，按以下顺序推进：

1. 编写 `Application`、`Window`、`Dialog` 的类级设计。
2. 确定 `Result<T>`、错误码、日志回调和句柄所有权。
3. 设计 WindowProc/DialogProc 消息绑定和异常边界。
4. 建立 CMake、Presets、静态库、测试和 Workbench 工程骨架。
5. 实现最小垂直切片：初始化库、创建主窗口、显示、关闭。
6. 为最小垂直切片编写生命周期和构建测试。

第一个开发里程碑定义为：

```text
M0：NativeFrameUI.lib 构建成功
M0：Workbench 链接静态库成功
M0：纯 Win32 主窗口可以创建、显示和安全销毁
M0：x64 Debug/Release 构建通过
M0：CTest 可以运行
```

这意味着下一步不再继续扩展需求，而是进入 `Core` 的详细设计，并立即建立可编译、可运行、可测试的最小工程骨架。

我先检查当前仓库结构和已有改动，再将 Core 详细设计落到 `docs/design`，并根据现状决定是否同步建立最小工程骨架。不会覆盖或回退现有未提交修改。我会重点确认三件事：当前是否已有 CMake/Visual Studio 工程、是否存在既有文档约定、以及工作区是否有需要保留的未提交改动。进入 Core 详细设计阶段。Core 是整个框架的基础层，第一版只负责窗口、消息、线程、句柄和初始化，不直接实现主题、布局、命令或具体控件。

# Core 详细设计 v0.1

## 1. Core 职责边界

Core 负责：

- UI 线程初始化和退出
- Win32 Common Controls 初始化
- DPI Awareness 初始化入口
- 窗口类注册与注销
- 原生窗口创建、销毁和句柄关联
- WindowProc 消息分发
- 消息循环
- UI 线程 Dispatcher
- Win32 错误捕获
- 窗口生命周期管理
- 基础窗口属性操作
- C++ 异常与 Win32 回调边界隔离

Core 不负责：

- 菜单、Toolbar、TreeView 等具体控件逻辑
- 命令路由
- 主题绘制
- 布局计算
- JSON 持久化
- 业务资源管理
- 业务线程池
- MFC 或 ATL 适配

依赖方向：

```text
Application
    |
Window / Dialog
    |
Message / Dispatcher / WindowClass
    |
Win32
```

## 2. Core 模块划分

```text
src/core/
├── application.cpp
├── application_impl.hpp
├── window.cpp
├── window_impl.hpp
├── dialog.cpp
├── dispatcher.cpp
├── message_loop.cpp
├── window_class.cpp
├── native_handle.cpp
├── thread_context.cpp
└── win32_error.cpp
```

公共头文件：

```text
include/nfui/
├── application.hpp
├── window.hpp
├── dialog.hpp
├── dispatcher.hpp
├── message.hpp
├── native_handle.hpp
├── window_class.hpp
├── error.hpp
└── result.hpp
```

## 3. 基本类型设计

### 3.1 WindowId

不使用裸 `HWND` 作为所有业务对象的唯一标识，内部保留窗口包装对象和原生句柄的对应关系。

```cpp
using WindowId = std::uint64_t;
```

V1 中 `WindowId` 主要用于日志、诊断和测试，不作为跨进程标识。

### 3.2 WindowHandle

窗口句柄属于 USER 对象，不允许由智能指针直接管理。

```cpp
class WindowHandle {
public:
    WindowHandle() noexcept = default;
    explicit WindowHandle(HWND hwnd) noexcept;

    [[nodiscard]] HWND get() const noexcept;
    [[nodiscard]] bool valid() const noexcept;

    void reset(HWND hwnd = nullptr) noexcept;

private:
    HWND hwnd_{};
};
```

`WindowHandle` 默认表示借用句柄，不负责调用 `DestroyWindow`。

拥有窗口生命周期的是 `Window` 对象，而不是 `WindowHandle`。

### 3.3 Win32 错误

```cpp
enum class ErrorCode {
    None,
    InvalidArgument,
    InvalidState,
    WrongThread,
    Win32Failure,
    ResourceNotFound,
    ClassRegistrationFailed,
    WindowCreationFailed,
    DialogCreationFailed,
    AlreadyInitialized,
    NotInitialized,
    UnsupportedOperation
};

class Error {
public:
    Error() = default;

    Error(
        ErrorCode code,
        DWORD native_code,
        std::wstring message);

    [[nodiscard]] ErrorCode code() const noexcept;
    [[nodiscard]] DWORD native_code() const noexcept;
    [[nodiscard]] std::wstring_view message() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

private:
    ErrorCode code_{ErrorCode::None};
    DWORD native_code_{ERROR_SUCCESS};
    std::wstring message_;
};
```

### 3.4 Result

建议内部和公共 API 都采用 `Result<T>`，避免用异常表达预期失败。

```cpp
template <typename T>
class Result;

template <>
class Result<void>;
```

使用形式：

```cpp
Result<WindowHandle> create_window(...);

auto result = create_window(options);
if (!result) {
    log(result.error());
    return;
}

HWND hwnd = result.value().get();
```

设计约束：

- `Result<T>` 不拥有外部资源。
- 错误对象必须可移动。
- 不在 `WindowProc` 中抛出异常。
- 对于不会失败的简单属性操作，可使用 `bool` 或 `void`.
- 构造函数不应隐式执行可能失败的窗口创建操作。

## 4. Application 详细设计

### 4.1 初始化选项

```cpp
enum class DpiAwareness {
    Unaware,
    System,
    PerMonitor,
    PerMonitorV2
};

struct ApplicationOptions {
    HINSTANCE instance{};
    HINSTANCE resource_instance{};

    DpiAwareness dpi_awareness{
        DpiAwareness::PerMonitorV2
    };

    bool initialize_common_controls{true};
    bool initialize_com{false};

    std::wstring company_name;
    std::wstring product_name;

    LogCallback log_callback{};
};
```

字段规则：

- `instance` 必须非空。
- `resource_instance` 为空时默认使用 `instance`。
- `PerMonitorV2` 是默认值。
- `initialize_com` 默认关闭，由需要 COM 的上层模块显式启用。
- `company_name` 和 `product_name` 用于后续持久化路径。
- Core 不自动创建日志文件。

### 4.2 初始化顺序

```text
Application 构造
    ↓
参数校验
    ↓
检查当前线程是否已有 Application
    ↓
设置 DPI Awareness
    ↓
初始化 Common Controls
    ↓
初始化可选 COM
    ↓
创建 ThreadContext
    ↓
创建 Dispatcher
    ↓
标记 Application 已初始化
```

如果中间步骤失败：

- 已成功初始化的资源按逆序释放。
- 构造结果必须可查询失败原因。
- 不允许留下半初始化的全局状态。

### 4.3 初始化状态

```cpp
enum class ApplicationState {
    Empty,
    Initializing,
    Initialized,
    Running,
    Stopping,
    Stopped,
    Failed
};
```

状态迁移：

```text
Empty -> Initializing -> Initialized -> Running
                         |              |
                         v              v
                       Failed         Stopping -> Stopped
```

约束：

- `run()` 只能从 `Initialized` 状态调用。
- 同一个 UI 线程不能嵌套运行两个主消息循环。
- `Application` 析构时，如果仍处于 `Running`，应先请求退出，再释放线程上下文。
- 不自动销毁业务窗口。

### 4.4 线程绑定

`Application` 在创建时记录：

```cpp
DWORD ui_thread_id_;
```

所有 UI 相关操作先检查：

```cpp
GetCurrentThreadId() == ui_thread_id_
```

错误返回：

```text
ErrorCode::WrongThread
```

调试版本可以增加断言，但 Release 版本必须返回可处理错误，不能仅依靠断言避免崩溃。

## 5. 消息循环设计

### 5.1 基本循环

```cpp
int Application::run() {
    MSG message{};

    state_ = ApplicationState::Running;

    while (true) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);

        if (result == -1) {
            state_ = ApplicationState::Stopping;
            return -1;
        }

        if (result == 0) {
            break;
        }

        if (!process_message(message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    state_ = ApplicationState::Stopped;
    return static_cast<int>(message.wParam);
}
```

`GetMessageW` 返回值必须区分：

```text
-1：读取消息失败
 0：收到 WM_QUIT
>0：正常消息
```

不能使用：

```cpp
while (GetMessageW(...)) {
}
```

因为这样会把错误 `-1` 当成循环结束处理。

### 5.2 消息预处理顺序

```text
Dispatcher 投递消息检查
    ↓
应用级预处理
    ↓
Accelerator 翻译
    ↓
IsDialogMessage
    ↓
TranslateMessage
    ↓
DispatchMessage
```

`process_message()` 的责任：

```cpp
bool Application::process_message(MSG& message);
```

返回 `true` 表示消息已经被处理，不能再次调用 `TranslateMessage` 或 `DispatchMessageW`。

V1 暂不引入复杂的全局消息过滤器链，只保留后续扩展点。

### 5.3 退出机制

```cpp
void Application::quit(int exit_code) noexcept;
```

实现：

```cpp
PostQuitMessage(exit_code);
```

不使用 `SendMessage` 触发退出，避免退出时同步重入。

如果需要提前通知窗口关闭，应由上层显式执行：

```text
请求主窗口关闭
    ↓
WM_CLOSE
    ↓
业务确认
    ↓
DestroyWindow
    ↓
WM_DESTROY
    ↓
PostQuitMessage
```

Core 不默认假定哪个窗口是主窗口。

## 6. Window 详细设计

### 6.1 创建参数

```cpp
struct WindowCreateOptions {
    std::wstring_view class_name;
    std::wstring_view title;

    DWORD style{WS_OVERLAPPEDWINDOW};
    DWORD extended_style{0};

    int x{CW_USEDEFAULT};
    int y{CW_USEDEFAULT};
    int width{CW_USEDEFAULT};
    int height{CW_USEDEFAULT};

    HWND parent{};
    HMENU menu{};
    HINSTANCE instance{};

    void* user_data{};
};
```

### 6.2 Window 接口

```cpp
class Window {
public:
    Window() noexcept = default;
    virtual ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    [[nodiscard]] HWND hwnd() const noexcept;
    [[nodiscard]] bool valid() const noexcept;

    Result<void> create(const WindowCreateOptions& options);
    Result<void> attach(HWND hwnd);
    HWND detach() noexcept;

    void show(int command = SW_SHOWNORMAL) noexcept;
    void hide() noexcept;
    void enable(bool enabled) noexcept;
    void destroy() noexcept;

protected:
    virtual MessageResult handle_message(
        UINT message,
        WPARAM wparam,
        LPARAM lparam);

    virtual void on_created();
    virtual void on_destroyed();
    virtual void on_dpi_changed(UINT dpi, const RECT& suggested_rect);
};
```

### 6.3 所有权规则

`Window` 对动态创建的顶层窗口和子窗口拥有销毁责任：

```text
create() 成功
    ↓
Window 拥有 HWND
    ↓
destroy() 或析构时销毁
```

`attach()` 的语义必须明确。推荐增加所有权参数：

```cpp
enum class WindowOwnership {
    Borrowed,
    Owned
};

Result<void> attach(HWND hwnd, WindowOwnership ownership);
```

规则：

- `Borrowed`：析构不调用 `DestroyWindow`。
- `Owned`：析构负责调用 `DestroyWindow`。
- 对话框子控件默认 `Borrowed`。
- 框架创建的独立窗口默认 `Owned`。
- 一个 `HWND` 同时只能被一个拥有型包装对象管理。

### 6.4 创建绑定流程

使用 `CREATESTRUCTW::lpCreateParams` 传递 `Window*`：

```text
CreateWindowExW
    ↓
WM_NCCREATE
    ↓
读取 CREATESTRUCTW::lpCreateParams
    ↓
SetWindowLongPtrW(GWLP_USERDATA, this)
    ↓
保存 HWND
    ↓
调用 on_created()
```

必须处理以下失败情况：

- `CreateWindowExW` 返回空句柄。
- `WM_NCCREATE` 返回失败。
- 对象在窗口创建回调中被销毁。
- `WM_NCDESTROY` 之前收到异常。
- 重复调用 `create()`。
- 对象已经绑定其他 `HWND`。

### 6.5 WindowProc

推荐结构：

```cpp
LRESULT CALLBACK Window::window_proc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) noexcept
{
    Window* window = nullptr;

    if (message == WM_NCCREATE) {
        const auto* create =
            reinterpret_cast<const CREATESTRUCTW*>(lparam);

        window = static_cast<Window*>(create->lpCreateParams);

        if (window == nullptr) {
            return FALSE;
        }

        window->bind(hwnd);
        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<Window*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (window != nullptr) {
        try {
            const auto result =
                window->dispatch_message(message, wparam, lparam);

            if (result.handled) {
                return result.value;
            }
        } catch (...) {
            window->report_callback_exception(message);
            return 0;
        }
    }

    if (message == WM_NCDESTROY) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);

        if (window != nullptr) {
            window->unbind(hwnd);
        }
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}
```

实际实现中需要确保：

- `WM_NCDESTROY` 清理逻辑即使派生处理函数返回已处理也会执行。
- `WM_NCCREATE` 的返回值符合 Win32 约定。
- 指针统一通过 `LONG_PTR` 转换。
- 异常处理不会再次触发可能抛异常的日志回调。
- 日志回调异常也必须被吞掉并转为内部诊断。

## 7. WindowClass 设计

### 7.1 注册参数

```cpp
struct WindowClassOptions {
    std::wstring name;
    HINSTANCE instance{};
    UINT style{CS_HREDRAW | CS_VREDRAW};
    WNDPROC window_proc{};
    int class_extra{};
    int window_extra{sizeof(void*)};
    HICON icon{};
    HCURSOR cursor{};
    HBRUSH background{};
    std::wstring menu_name;
};
```

### 7.2 注册策略

注册类名称使用稳定前缀：

```text
NativeFrameUI.Window.<hash>
```

或者使用明确固定名称：

```text
NativeFrameUI.MainWindow
NativeFrameUI.ChildWindow
NativeFrameUI.DialogHost
```

推荐固定名称加模块句柄，因为类行为必须稳定可诊断。

注册规则：

- 同一模块、同名且配置一致时允许复用。
- 配置不一致时返回错误，不覆盖已有窗口类。
- 析构时只注销由当前实例注册的窗口类。
- 若仍有窗口使用该类，注销失败应记录日志，不影响进程退出。
- 不注销应用自行注册的窗口类。

### 7.3 静态窗口类

Core 提供少量内部窗口类：

```text
nfui.Window
nfui.Panel
nfui.Splitter
```

具体控件类由 `controls` 模块按需注册，避免 Core 依赖控件实现。

## 8. Dialog 设计

Dialog 与普通 Window 分开处理，因为 Win32 回调和创建模型不同。

```cpp
class Dialog {
public:
    virtual ~Dialog();

    Result<HWND> create_modeless(
        HINSTANCE instance,
        int resource_id,
        HWND parent);

    Result<int> do_modal(
        HINSTANCE instance,
        int resource_id,
        HWND parent);

    void end_dialog(int result) noexcept;

    [[nodiscard]] HWND hwnd() const noexcept;

protected:
    virtual bool on_init_dialog(HWND focus);
    virtual bool on_command(
        WORD notify_code,
        WORD control_id,
        HWND control);
    virtual bool on_notify(
        int control_id,
        NMHDR* header);
    virtual INT_PTR handle_message(
        UINT message,
        WPARAM wparam,
        LPARAM lparam);
};
```

规则：

- Modeless Dialog 使用 `CreateDialogParamW`。
- Modal Dialog 使用 `DialogBoxParamW`。
- 初始化阶段通过 `lParam` 绑定 `Dialog*`。
- `WM_INITDIALOG` 返回 `TRUE` 表示使用默认焦点。
- `WM_COMMAND` 和 `WM_NOTIFY` 先交给 Dialog，再交给具体控件。
- Modal Dialog 关闭使用 `EndDialog`。
- Modeless Dialog 关闭使用 `DestroyWindow`。
- 两者不能混用销毁方式。

## 9. Dispatcher 设计

### 9.1 目标

Dispatcher 解决后台线程向 UI 线程投递任务的问题，但不允许后台线程直接操作窗口。

```cpp
class Dispatcher {
public:
    using Task = std::function<void()>;

    Result<void> post(Task task);
    Result<void> send(Task task);

    [[nodiscard]] DWORD thread_id() const noexcept;
};
```

### 9.2 投递机制

使用内部消息窗口：

```text
Dispatcher::post()
    ↓
线程安全任务队列
    ↓
PostMessageW(dispatcher_hwnd, NFUI_EXECUTE_TASK, ...)
    ↓
UI 线程取出任务
    ↓
执行任务
```

规则：

- `post()` 不等待 UI 线程。
- `send()` 仅作为后续扩展，V1 可先不公开。
- Dispatcher 销毁后，未执行任务全部丢弃并记录日志。
- 不在持有队列锁时执行任务。
- 任务抛异常时由 Dispatcher 捕获并交给日志系统。
- 任务不能依赖已经销毁的窗口对象，生命周期由调用方负责。

## 10. 消息和生命周期风险

重点防护以下场景：

### 延迟消息访问已销毁对象

窗口销毁后：

```text
WM_NCDESTROY
    ↓
清除 GWLP_USERDATA
    ↓
取消窗口相关延迟任务
```

### Window 对象先于 HWND 销毁

要求：

- 析构时先解除窗口关联。
- 如果对象拥有窗口，则先执行 `DestroyWindow`。
- `DestroyWindow` 产生的消息期间对象仍必须有效。
- 完成销毁后再释放对象成员。

### 同步消息重入

以下操作可能导致重入：

```text
SetWindowPos
SetFocus
SendMessage
DestroyWindow
SetWindowLongPtr
```

禁止在内部锁定状态下调用派生类回调。内部状态更新顺序应为：

```text
更新框架状态
    ↓
释放内部锁
    ↓
调用用户回调
```

### 关闭过程中的命令执行

窗口开始销毁后，应拒绝新的异步命令和布局任务：

```cpp
if (state_ >= WindowState::Destroying) {
    return ErrorCode::InvalidState;
}
```

## 11. Core 第一批实现顺序

建议按照最小可运行垂直切片实现：

### C01：基础类型

- `ErrorCode`
- `Error`
- `Result<T>`
- `MessageResult`
- `WindowHandle`

### C02：ThreadContext

- UI 线程 ID
- 当前线程检查
- Application 上下文注册
- 线程退出清理

### C03：WindowClass

- 窗口类注册
- 窗口类注销
- 注册失败诊断

### C04：Window

- `WindowProc`
- `WM_NCCREATE`
- `WM_NCDESTROY`
- `create`
- `destroy`
- `show`
- 默认消息处理

### C05：Application

- Common Controls 初始化
- DPI Awareness 设置
- 基础消息循环
- `WM_QUIT`
- 初始化失败回收

### C06：Workbench

- 创建主窗口
- 显示窗口
- 处理 `WM_CLOSE`
- 处理 `WM_DESTROY`
- 正常退出

## 12. Core 验收标准

Core 阶段完成时必须满足：

1. CMake 能生成 `NativeFrameUI` 静态库。
2. Workbench 能链接并启动。
3. 主窗口能够通过 `Window` 创建。
4. `WM_NCCREATE` 能正确绑定 C++ 对象。
5. `WM_NCDESTROY` 能清理 `GWLP_USERDATA`。
6. 窗口析构不会重复销毁 `HWND`。
7. `Application::run()` 能正确处理正常退出。
8. `GetMessageW` 返回 `-1` 时能报告错误。
9. WindowProc 不向外传播 C++ 异常。
10. 非 UI 线程调用窗口 API 能返回 `WrongThread`。
11. 64 位指针存储全部使用 `LONG_PTR`/`SetWindowLongPtrW`。
12. Debug/Release x64 均能构建。
13. 至少有窗口创建、销毁和消息分发测试。
14. 没有持续增长的窗口、USER 或 GDI 对象。

## 13. 详细设计阶段的下一项输出

Core 详细设计下一步应继续拆成以下三个设计文档：

```text
docs/design/core-api.md
docs/design/window-lifecycle.md
docs/design/message-dispatch.md
```

然后进入首个实现任务：

```text
M0-C01：Result/Error/MessageResult/WindowHandle
M0-C02：Application 和 ThreadContext
M0-C03：WindowClass 和 Window
M0-C04：最小 Workbench
M0-C05：Core 单元及集成测试
```

其中最先实现 `Result/Error` 和 `Window` 生命周期基础，因为后续 `Dialog`、控件、命令路由和布局系统都会依赖这套模型。

有。当前设计已经能支撑概要评审，但在开始编码前，Core 还有几项会直接影响公共 API、生命周期和错误恢复的决策需要冻结。尤其有几个现有设计点存在冲突，建议先修正。

**必须讨论并冻结**

1. **`Application` 如何报告构造失败**

当前一方面写了：

```cpp
explicit Application(const ApplicationOptions&);
```

另一方面又要求“不使用异常表达预期失败”和“构造失败可查询”。这会产生半初始化对象。

推荐改为工厂方法：

```cpp
class Application {
public:
    static Result<Application> create(ApplicationOptions options);
};
```

或者：

```cpp
Application application;
Result<void> initialize(ApplicationOptions options);
```

推荐第一种，避免 `Application` 处于可见但无效的状态。

2. **`Window` 是否允许移动**

当前设计允许移动构造和移动赋值，但 `HWND` 的 `GWLP_USERDATA` 保存的是对象地址。窗口创建后移动对象，会让窗口过程继续访问旧地址，造成悬空指针。

推荐冻结为：

```cpp
Window(const Window&) = delete;
Window& operator=(const Window&) = delete;
Window(Window&&) = delete;
Window& operator=(Window&&) = delete;
```

窗口对象地址必须从绑定开始到 `WM_NCDESTROY` 保持稳定。上层如需动态管理，应使用 `std::unique_ptr<DerivedWindow>`。

3. **`WM_NCDESTROY` 的强制清理路径**

当前伪代码中，如果派生类把 `WM_NCDESTROY` 标记为已处理并提前返回，解绑逻辑可能不会执行；异常路径也可能绕过清理。

应明确采用不可绕过的最终清理：

```text
进入 dispatch
    ↓
调用框架/派生处理
    ↓
若消息为 WM_NCDESTROY，无条件清理绑定
    ↓
返回派生结果或 DefWindowProcW 结果
```

还需要决定 `DefWindowProcW(WM_NCDESTROY)` 与解绑的先后顺序。推荐先执行消息处理和默认处理，最后清除 `GWLP_USERDATA`、置空 `hwnd_`、转换状态。

4. **窗口所有权模型**

`attach(HWND, Owned)` 风险较大，因为任意外部句柄都可能被错误接管。

推荐拆成语义明确的 API：

```cpp
Result<void> create(...);          // 拥有
Result<void> attach_borrowed(HWND); // 借用
HWND detach() noexcept;
```

V1 不公开 `attach_owned()`。另外必须冻结：

- 顶层窗口析构时是否自动 `DestroyWindow`
- 非 UI 线程析构如何处理
- 借用句柄收到 `WM_NCDESTROY` 时如何自动失效
- 父窗口销毁子窗口后包装对象如何获知句柄失效

推荐拥有型窗口析构必须发生在创建线程；线程错误时记录严重错误并断言，不能从错误线程调用 `DestroyWindow`。

5. **对象析构与窗口回调期间自删除**

需要规定是否允许以下写法：

```cpp
delete this; // 在消息处理函数中
```

推荐明确禁止在窗口回调中直接销毁 C++ 对象。允许调用 `DestroyWindow()`，但对象本身必须活到当前回调返回。异步释放可通过 Dispatcher 排队完成。

否则 `dispatch_message()` 返回后访问成员状态会发生悬空访问。

6. **`Application` 的作用域**

当前说“同一 UI 线程一个 Application”，但 DPI Awareness、Common Controls 和部分初始化实际是进程级状态。

需要拆分为：

```text
ProcessContext：进程级，只初始化一次
ThreadContext：每个 UI 线程一个
Application：主消息循环和服务入口
```

V1 若只支持单 UI 线程，也应明确写成硬约束，避免 API 看起来支持多 UI 线程但实现并不完整。

推荐 V1：

```text
一个进程一个 Application
一个受支持的 UI 线程
其他线程只能通过 Dispatcher 投递任务
```

这样比“每个线程一个 Application”更容易验证。

7. **DPI Awareness 初始化失败策略**

`SetProcessDpiAwarenessContext` 必须在创建窗口前调用，而且可能因为宿主 EXE manifest 或其他组件已设置而失败。

需要区分：

- 已经是目标模式：视为成功
- 已设置为其他模式：警告并使用实际模式
- API 不可用：按系统能力回退
- 真正调用失败：返回初始化错误

此外，DPI Awareness 更推荐由最终 EXE manifest 声明，库只做验证和兼容性设置。静态库无法独立控制整个进程的 DPI 策略。

8. **COM 初始化的所有权**

如果调用方已经调用 `CoInitializeEx`，库再次初始化可能返回 `S_FALSE` 或 `RPC_E_CHANGED_MODE`。

必须记录 Core 是否真正持有一次成功的 COM 初始化，并且只有成功获得初始化计数时才调用 `CoUninitialize`。考虑到 Core V1 暂无 COM 依赖，更推荐从 `ApplicationOptions` 删除该项，由真正依赖 COM 的模块自行管理。

9. **Dispatcher 的关闭和唤醒语义**

需要冻结：

- `post()` 在关闭后返回什么
- 已入队任务在退出时执行还是丢弃
- 消息窗口销毁与队列清空顺序
- 投递消息失败时如何移除对应任务
- 是否允许 UI 线程内 `post()`
- 是否提供取消句柄
- 任务捕获窗口对象时如何避免悬空

推荐 V1 仅提供异步 `post()`，不提供 `send()`。同步跨线程调用很容易引入死锁。关闭后拒绝新任务，退出阶段丢弃未执行任务。

10. **主窗口与 `PostQuitMessage` 的关系**

Core 不假设哪个窗口是主窗口是合理的，但 Workbench 仍需明确退出策略。

推荐由应用层实现：

```cpp
case WM_DESTROY:
    application.quit(0);
    return handled(0);
```

不能让所有顶层 `Window` 在 `WM_DESTROY` 时自动发出 `WM_QUIT`，否则工具窗口也可能终止应用。

**建议在编码前明确**

11. **消息处理 API 的形式**

建议统一成：

```cpp
struct MessageResult {
    bool handled;
    LRESULT value;

    static constexpr MessageResult handled_with(LRESULT value = 0);
    static constexpr MessageResult unhandled();
};
```

不要让普通消息和 Dialog 消息共享同一返回抽象。Dialog 的“消息已处理”“返回值”“焦点由系统设置”存在特殊语义，应单独设计 `DialogMessageResult`，否则 `WM_INITDIALOG` 很容易处理错误。

12. **框架消息处理与派生处理顺序**

并非所有消息都适合同一个顺序。例如：

- `WM_NCCREATE` 必须先由框架绑定。
- `WM_NCDESTROY` 必须由框架最终解绑。
- `WM_DPICHANGED` 可能先更新内部 DPI，再通知派生类。
- `WM_COMMAND` 可先交业务层。
- `WM_GETMINMAXINFO` 可能需要布局模块参与。

建议为强制生命周期消息建立不可覆盖的内部处理层，派生类只接收安全通知，不直接控制绑定过程。

13. **异常处理后的返回策略**

WindowProc 捕获异常后一律返回 `0` 并不总是正确，例如 `WM_NCCREATE` 返回 `0` 表示创建失败，而部分消息需要调用默认过程。

推荐：

- `WM_NCCREATE` 异常：返回 `FALSE`，创建失败。
- `WM_CREATE` 异常：返回 `-1`。
- 其他消息：记录异常后调用 `DefWindowProcW`。
- `WM_NCDESTROY` 异常：仍完成清理，然后执行默认处理。
- 日志回调必须标记为 `noexcept` 语义，框架仍需捕获其异常。

14. **窗口类注册表的并发和复用**

应明确注册表按什么键识别：

```text
HINSTANCE + class name
```

还要比较哪些字段判断“配置一致”。不能仅看到 `ERROR_CLASS_ALREADY_EXISTS` 就当作成功，因为同名类可能使用不同的 `WNDPROC`。

V1 可以简化为只注册框架固定类，并用进程内引用计数管理，不急于公开通用 `WindowClass` 注册 API。

15. **错误信息与 `GetLastError` 捕获时机**

Win32 调用失败后必须立即保存 `GetLastError()`，再执行字符串分配、日志或其他 API。建议提供内部助手：

```cpp
Error last_win32_error(
    ErrorCode code,
    std::wstring_view operation,
    DWORD native_code = GetLastError());
```

另外要规定哪些 Win32 API 的返回值不能通过 `GetLastError` 直接判断，例如 `SetWindowLongPtrW` 返回 `0` 时，需要先 `SetLastError(ERROR_SUCCESS)`。

16. **公共 API 的 ABI 目标**

当前是静态库，ABI 稳定压力相对小，但仍需冻结：

- 公共头文件是否允许 STL 类型
- Debug/Release 是否允许混链
- 是否支持不同 MSVC 工具集
- `Result<T>` 是否全部头文件实现
- 编译选项和宏是否构成公共契约

推荐 V1 明示：

```text
仅支持同一 MSVC toolset、架构和 CRT 配置链接
公共 API 可以使用 std::wstring_view 和 std::unique_ptr
不保证跨编译器 ABI
```

17. **Unicode 与字符串生命周期**

`WindowCreateOptions` 使用 `std::wstring_view` 没问题，因为 `CreateWindowExW` 在调用期间消费字符串；但异步保存这些选项时就会悬空。

需要规定：

- 立即消费的参数可用 `wstring_view`
- 需要存储的字段必须复制为 `std::wstring`
- 所有 Win32 API 固定使用 `W` 版本
- 公共 API 不接受当前代码页窄字符串

18. **测试可注入性**

`GetMessageW` 错误极难在正常运行中制造。若验收要求测试该分支，就需要提前决定：

- 抽象最小 `Win32Api` 接口用于测试注入，或者
- 将消息循环核心写成可传入 `get_message` 函数的内部单元，或者
- 只做代码审查和静态验证

推荐只为消息循环和纯逻辑引入轻量函数注入，不要把所有 Win32 API 包装成大型接口。

**建议补充的设计文档**

开始 M0 前，Core 至少还应形成以下决策记录：

```text
docs/design/core-api.md
docs/design/window-lifecycle.md
docs/design/message-dispatch.md
docs/design/application-and-threading.md
docs/design/dispatcher-shutdown.md

docs/decisions/
├── ADR-006-application-creation.md
├── ADR-007-window-address-stability.md
├── ADR-008-window-ownership.md
├── ADR-009-dpi-initialization.md
└── ADR-010-ui-thread-model.md
```

**我的推荐冻结结论**

为了尽快进入实现，建议直接采用以下结论：

```text
Application 使用 Result<Application> 工厂创建
一个进程一个 Application，V1 只支持一个 UI 线程
Window 不可复制、不可移动
create() 创建拥有型窗口，attach_borrowed() 只借用
WM_NCDESTROY 清理不可被派生处理绕过
禁止消息回调中直接 delete Window
Core 不负责 COM 初始化
DPI 首选 EXE manifest，Core 负责验证和兼容回退
Dispatcher V1 只提供 post()，不提供同步 send()
Dialog 使用独立消息结果语义
窗口类注册先保持内部能力，不急于公开
```

这些决定冻结后，Core 详细设计就足以进入实现。当前最需要修订的是 `Application` 创建方式、`Window` 移动语义和 `WM_NCDESTROY` 清理流程，这三项如果留到编码后再改，会波及几乎所有后续模块。

