@echo off
REM Force UTF-8 code page so any Chinese comment lines stay readable
REM when this script is launched from a console in the OEM code page.
chcp 65001 >nul

REM cleanup.bat - drop the runtime detritus that the polish/audit
REM pipeline leaves behind (audit intermediates, smoke logs, leftover
REM auto-loop tooling) without touching anything that would force a
REM rebuild or remove final audit artefacts.
REM
REM Always deleted:
REM   out\screens\              intermediate audit screenshots
REM   out\smoke*.{log,out,txt}  test logs / capture stdout+stderr
REM   docs\VISUAL_AUDIT\*.bmp   diagnostic BMPs (final artefacts are .png)
REM   node_modules\              auto-loop deps (gitignored)
REM   package.json / package-lock.json
REM                              auto-loop manifests (gitignored)
REM
REM Optional (--include-build):
REM   build\                    legacy root-level CMake cache (NOT out\build\)
REM
REM Never deleted:
REM   out\build\                 current CMake build output (would force rebuild)
REM   docs\VISUAL_AUDIT\*.png   final audit screenshots (30 captures)
REM   anything tracked in git   use `git clean -idx` for that
REM
REM Usage:
REM   cleanup.bat                standard pass (safe set above)
REM   cleanup.bat --dry-run      print what would be deleted, do nothing
REM   cleanup.bat --include-build  also remove legacy root-level build\
REM   cleanup.bat -h, --help     show this help
REM
REM Implementation note: cmd.exe rejects quoted paths that contain
REM backslashes ("out\screens" -> "system cannot find the path") and
REM also rejects unquoted paths that contain forward slashes ("/screens"
REM is parsed as a switch). All paths in this script are therefore
REM unquoted backslash paths. The `echo` lines and the for-loop glob are
REM the only places that need quotes.

setlocal

set "DRY_RUN=0"
set "INCLUDE_BUILD=0"
set "DELETED=0"
set "SKIPPED=0"

:parse_args
if "%~1"=="" goto :parse_done
if /i "%~1"=="--dry-run" (
    set "DRY_RUN=1"
    shift
    goto :parse_args
)
if /i "%~1"=="--include-build" (
    set "INCLUDE_BUILD=1"
    shift
    goto :parse_args
)
if /i "%~1"=="-h" goto :show_help
if /i "%~1"=="--help" goto :show_help
echo Unknown argument: %~1
echo.
echo Run "cleanup.bat --help" for usage.
endlocal
exit /b 1

:show_help
echo.
echo cleanup.bat - drop polish/audit runtime detritus.
echo.
echo Usage:
echo   cleanup.bat                  standard pass (always-delete set)
echo   cleanup.bat --dry-run        print what would be deleted, do nothing
echo   cleanup.bat --include-build  also remove legacy root-level build\
echo   cleanup.bat -h, --help       show this help
echo.
echo Always deleted (idempotent):
echo   out\screens\
echo   out\smoke*.{log,out,txt}
echo   docs\VISUAL_AUDIT\*.bmp
echo   node_modules\
echo   package.json / package-lock.json
echo.
echo Never deleted:
echo   out\build\                  current CMake build output
echo   docs\VISUAL_AUDIT\*.png      final audit screenshots
echo.
endlocal
exit /b 0

:parse_done
echo.
echo ==========================================================
echo  NativeFrameUI cleanup
echo  ROOT: %CD%
echo  Mode: %DRY_RUN%   Include legacy build\: %INCLUDE_BUILD%
echo ==========================================================
echo.

REM ----------------------------------------------------------------------
REM Each block uses the same shape:
REM   if exist PATH (
REM       dry-run? echo + skip del
REM       del / rmdir
REM       verify gone
REM       set /a DELETED+=1
REM   ) else (
REM       echo [skip]
REM       set /a SKIPPED+=1
REM   )
REM ----------------------------------------------------------------------

REM --- Directories ---

if exist out\screens (
    if "%DRY_RUN%"=="1" (echo [dry-run] out\screens) else (rmdir /s /q out\screens >nul 2>&1)
    if "%DRY_RUN%"=="1" (echo [del ] out\screens) else (if exist out\screens (echo [FAIL] out\screens - rmdir did not remove it) else (echo [del ] out\screens))
    set /a DELETED+=1
) else (
    echo [skip] out\screens
    set /a SKIPPED+=1
)

if exist node_modules (
    if "%DRY_RUN%"=="1" (echo [dry-run] node_modules) else (rmdir /s /q node_modules >nul 2>&1)
    if "%DRY_RUN%"=="1" (echo [del ] node_modules) else (if exist node_modules (echo [FAIL] node_modules) else (echo [del ] node_modules))
    set /a DELETED+=1
) else (
    echo [skip] node_modules
    set /a SKIPPED+=1
)

REM --- Single files ---

if exist out\smoke.log (
    if "%DRY_RUN%"=="1" (echo [dry-run] out\smoke.log) else (del /q out\smoke.log >nul 2>&1)
    if "%DRY_RUN%"=="1" (echo [del ] out\smoke.log) else (if exist out\smoke.log (echo [FAIL] out\smoke.log) else (echo [del ] out\smoke.log))
    set /a DELETED+=1
) else (
    echo [skip] out\smoke.log
    set /a SKIPPED+=1
)

if exist out\smoke.out (
    if "%DRY_RUN%"=="1" (echo [dry-run] out\smoke.out) else (del /q out\smoke.out >nul 2>&1)
    if "%DRY_RUN%"=="1" (echo [del ] out\smoke.out) else (if exist out\smoke.out (echo [FAIL] out\smoke.out) else (echo [del ] out\smoke.out))
    set /a DELETED+=1
) else (
    echo [skip] out\smoke.out
    set /a SKIPPED+=1
)

if exist out\smoke-error.txt (
    if "%DRY_RUN%"=="1" (echo [dry-run] out\smoke-error.txt) else (del /q out\smoke-error.txt >nul 2>&1)
    if "%DRY_RUN%"=="1" (echo [del ] out\smoke-error.txt) else (if exist out\smoke-error.txt (echo [FAIL] out\smoke-error.txt) else (echo [del ] out\smoke-error.txt))
    set /a DELETED+=1
) else (
    echo [skip] out\smoke-error.txt
    set /a SKIPPED+=1
)

if exist out\smoke-output.txt (
    if "%DRY_RUN%"=="1" (echo [dry-run] out\smoke-output.txt) else (del /q out\smoke-output.txt >nul 2>&1)
    if "%DRY_RUN%"=="1" (echo [del ] out\smoke-output.txt) else (if exist out\smoke-output.txt (echo [FAIL] out\smoke-output.txt) else (echo [del ] out\smoke-output.txt))
    set /a DELETED+=1
) else (
    echo [skip] out\smoke-output.txt
    set /a SKIPPED+=1
)

if exist package.json (
    if "%DRY_RUN%"=="1" (echo [dry-run] package.json) else (del /q package.json >nul 2>&1)
    if "%DRY_RUN%"=="1" (echo [del ] package.json) else (if exist package.json (echo [FAIL] package.json) else (echo [del ] package.json))
    set /a DELETED+=1
) else (
    echo [skip] package.json
    set /a SKIPPED+=1
)

if exist package-lock.json (
    if "%DRY_RUN%"=="1" (echo [dry-run] package-lock.json) else (del /q package-lock.json >nul 2>&1)
    if "%DRY_RUN%"=="1" (echo [del ] package-lock.json) else (if exist package-lock.json (echo [FAIL] package-lock.json) else (echo [del ] package-lock.json))
    set /a DELETED+=1
) else (
    echo [skip] package-lock.json
    set /a SKIPPED+=1
)

REM --- Glob: docs\VISUAL_AUDIT\*.bmp ---
REM The glob is expanded by `for %%F in (...)`. We need the full path for
REM the `if exist`/delete calls; `%%~fF` gives it. We quote the full path
REM inside the loop body because `%%~fF` produces an absolute path that
REM contains spaces if the project root is under a path with spaces
REM (it is: C:\tjf\github\NativeFrameUI), and `del` is happy with
REM quoted absolute paths as long as they don't contain forward slashes.

if exist docs\VISUAL_AUDIT (
    for %%F in (docs\VISUAL_AUDIT\*.bmp) do (
        if exist "%%~fF" (
            if "%DRY_RUN%"=="1" (
                echo [dry-run] %%~fF
            ) else (
                del /q "%%~fF" >nul 2>&1
            )
            if "%DRY_RUN%"=="1" (
                echo [del ] %%~fF
            ) else (
                if exist "%%~fF" (
                    echo [FAIL] %%~fF
                ) else (
                    echo [del ] %%~fF
                )
            )
            set /a DELETED+=1
        ) else (
            echo [skip] %%~fF
            set /a SKIPPED+=1
        )
    )
)

REM --- Optional legacy root-level build\ ---

if "%INCLUDE_BUILD%"=="1" (
    if exist build (
        if "%DRY_RUN%"=="1" (echo [dry-run] build) else (rmdir /s /q build >nul 2>&1)
        if "%DRY_RUN%"=="1" (echo [del ] build) else (if exist build (echo [FAIL] build) else (echo [del ] build))
        set /a DELETED+=1
    ) else (
        echo [skip] build
        set /a SKIPPED+=1
    )
)

echo.
echo ==========================================================
echo  Cleanup complete. Deleted: %DELETED%   Skipped: %SKIPPED%
if "%DRY_RUN%"=="1" echo  (dry-run mode - nothing was actually removed)
echo  out\build\ preserved.
echo  docs\VISUAL_AUDIT\*.png preserved.
echo ==========================================================
endlocal
exit /b 0