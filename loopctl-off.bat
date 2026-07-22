@echo off
REM loopctl-off.bat - 在当前项目目录关闭 loopctl 自动续接循环。

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

echo.
echo === Loopctl 关闭 ===
echo   项目: %CD%
echo.

node "%LOOPCTL%" off

endlocal