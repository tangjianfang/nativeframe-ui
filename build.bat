@echo off
REM Force UTF-8 code page so any Chinese comment lines stay readable
REM when this script is launched from a console in the OEM code page.
chcp 65001 >nul

REM build.bat - one-click build for NativeFrameUI (x64-debug + x64-release)
REM with optional ctest. Designed to be runnable from cmd.exe, PowerShell,
REM or Git Bash. Stops on the first failed step and returns non-zero.
REM
REM Usage:
REM   build.bat                  build and test debug + release (default)
REM   build.bat --skip-tests     build only, no tests
REM   build.bat --debug-only     build only x64-debug
REM   build.bat --release-only   build only x64-release
REM   build.bat -h | --help      show this help

setlocal

set "RUN_TESTS=1"
set "BUILD_DEBUG=1"
set "BUILD_RELEASE=1"

:parse_args
if "%~1"=="" goto :parse_done
if /i "%~1"=="--skip-tests" goto :set_skip_tests
if /i "%~1"=="--no-tests" goto :set_skip_tests
if /i "%~1"=="--debug-only" goto :set_debug_only
if /i "%~1"=="--release-only" goto :set_release_only
if /i "%~1"=="-h" goto :show_help
if /i "%~1"=="--help" goto :show_help
echo Unknown argument: %~1
echo.
echo Run "build.bat --help" for usage.
endlocal
exit /b 1

:set_skip_tests
set "RUN_TESTS=0"
shift
goto :parse_args

:set_debug_only
set "BUILD_RELEASE=0"
shift
goto :parse_args

:set_release_only
set "BUILD_DEBUG=0"
shift
goto :parse_args

:show_help
echo.
echo build.bat - one-click build for NativeFrameUI.
echo.
echo Usage:
echo   build.bat                  build and test debug + release
echo   build.bat --skip-tests     build only, no tests
echo   build.bat --debug-only     build only x64-debug
echo   build.bat --release-only   build only x64-release
echo.
endlocal
exit /b 0

:parse_done
echo.
echo ==========================================================
echo  NativeFrameUI one-click build
echo  ROOT: %CD%
echo  Tests: %RUN_TESTS%  (1=run, 0=skip)
echo  Debug: %BUILD_DEBUG%  Release: %BUILD_RELEASE%
echo ==========================================================
echo.

if "%BUILD_DEBUG%"=="1" (
    call :build_preset "x64-debug" "Debug" || goto :error
)
if "%BUILD_RELEASE%"=="1" (
    call :build_preset "x64-release" "Release" || goto :error
)

echo.
echo ==========================================================
echo  Build completed successfully.
echo  Debug output:   out\build\x64-debug\
echo  Release output: out\build\x64-release\
echo ==========================================================
endlocal
exit /b 0

REM ----------------------------------------------------------------------
REM :build_preset <preset> <display>
REM   Run configure, build, and (optionally) test sequentially.
REM   Returns non-zero from the function on the first failed step.
REM ----------------------------------------------------------------------
:build_preset
echo.
echo === configure %~2 preset=%~1 ===
cmake --preset %~1
if errorlevel 1 exit /b 1

echo.
echo === build %~2 preset=%~1 ===
cmake --build --preset %~1
if errorlevel 1 exit /b 1

if "%RUN_TESTS%"=="1" (
    echo.
    echo === test %~2 preset=%~1 ===
    ctest --preset %~1 --output-on-failure
    if errorlevel 1 exit /b 1
)
exit /b 0

:error
echo.
echo ==========================================================
echo  *** BUILD FAILED ***
echo  See the log above for details.
echo ==========================================================
endlocal
exit /b 1