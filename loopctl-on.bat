@echo off
REM loopctl-on.bat - 在当前项目目录启用 loopctl 自动续接循环。
REM
REM 用法:
REM   loopctl-on.bat                          启用, 默认 --max 8, push 智能检测
REM   loopctl-on.bat --max 12 --no-push       自定义参数
REM   loopctl-on.bat --preset find-bugs       使用预设提示词
REM   loopctl-on.bat --prompt "..."           自定义提示词
REM
REM 所有参数会原样转发给 `loopctl on`；现有 maxRounds / push 只在显式传入时覆盖。
REM
REM push 智能检测 (无参数时生效):
REM   - 当前目录是 git 仓库且当前分支已配置 upstream tracking branch -> 默认 --push
REM   - 否则 (无仓库 / 无 upstream) -> 默认 --no-push, 避免每轮报 push 失败

setlocal

where node >nul 2>nul
if errorlevel 1 (
    echo Node.js not found. Please install Node.js and try again.
    exit /b 1
)

set "LOOPCTL=%USERPROFILE%\.claude\loopctl.js"
if not exist "%LOOPCTL%" (
    echo Error: loopctl.js not found at %LOOPCTL%
    echo 请先运行 install-loopctl.bat 安装。
    exit /b 1
)

REM Auto-detect: push is viable only if cwd is a git repo with upstream tracking branch
set "PUSH_FLAG="
git rev-parse --is-inside-work-tree >nul 2>nul
if not errorlevel 1 (
    git rev-parse --abbrev-ref --symbolic-full-name @{u} >nul 2>nul
    if not errorlevel 1 (
        set "PUSH_FLAG=--push"
    )
)

echo.
echo === Loopctl 启用 ===
echo   项目: %CD%
if defined PUSH_FLAG (
    echo   push: 启用 ^(git 仓库 + upstream 已配置^)
) else (
    echo   push: 禁用 ^(无 git 仓库或无 upstream; 传 --push 强制开启^)
)
echo.

if "%~1"=="" (
    if defined PUSH_FLAG (
        node "%LOOPCTL%" on --max 8 --push
    ) else (
        node "%LOOPCTL%" on --max 8 --no-push
    )
) else (
    node "%LOOPCTL%" on %*
)

endlocal