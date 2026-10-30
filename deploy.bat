@echo off
REM ============================================================
REM  MeterMonitor - build a redistributable package
REM
REM  NOTE: this file is intentionally ASCII only.
REM        Windows cmd.exe reads .bat files with the OEM codepage
REM        (949 on Korean Windows), so UTF-8 Korean text breaks it.
REM
REM  Usage
REM    1) Build the project in Release first (Qt Creator or nmake).
REM    2) Run this file from the project folder:  deploy.bat
REM    3) A "deploy" folder and MeterMonitor.zip are created.
REM       Copy that folder (or the zip) to any PC - no Qt needed.
REM ============================================================

setlocal

REM ---- settings ---------------------------------------------------------
set "QTDIR=C:\Qt\6.6.2\msvc2019_64"
set "APPNAME=MeterMonitor"
set "OUTDIR=%~dp0deploy"

echo.
echo === %APPNAME% deploy ===
echo.

REM ---- locate the built exe --------------------------------------------
REM  Search order:
REM    1. release\ next to this script          (nmake build)
REM    2. next to this script
REM    3. anywhere under the project folder, release build preferred
REM       (covers build\Desktop_Qt_6_6_2_MSVC2019_64bit-Release\release\)
REM    4. Qt Creator shadow build folders beside the project folder
set "EXE="
if exist "%~dp0release\%APPNAME%.exe"            set "EXE=%~dp0release\%APPNAME%.exe"
if not defined EXE if exist "%~dp0%APPNAME%.exe" set "EXE=%~dp0%APPNAME%.exe"

if not defined EXE (
    for /f "delims=" %%F in ('dir /b /s "%~dp0%APPNAME%.exe" 2^>nul ^| findstr /i /c:"\release" ^| findstr /v /i /c:"\deploy"') do set "EXE=%%F"
)
if not defined EXE (
    for /d %%D in ("%~dp0..\build-%APPNAME%-*") do (
        if exist "%%~fD\release\%APPNAME%.exe" set "EXE=%%~fD\release\%APPNAME%.exe"
    )
)
REM last resort: any build of the exe under the project folder
if not defined EXE (
    for /f "delims=" %%F in ('dir /b /s "%~dp0%APPNAME%.exe" 2^>nul ^| findstr /v /i /c:"\deploy"') do set "EXE=%%F"
)

if not defined EXE (
    echo [ERROR] %APPNAME%.exe not found.
    echo         Build the project in Release mode first,
    echo         or copy %APPNAME%.exe next to this script.
    goto :fail
)

echo %EXE% | findstr /i /c:"\debug" >nul && (
    echo [WARN] This looks like a DEBUG build:
    echo        %EXE%
    echo        Debug binaries need the Qt debug DLLs and will not run
    echo        on a PC without Visual Studio. Build in Release if possible.
    echo.
)

if not exist "%QTDIR%\bin\windeployqt.exe" (
    echo [ERROR] windeployqt not found:
    echo         %QTDIR%\bin\windeployqt.exe
    echo         Edit the QTDIR line at the top of this script.
    goto :fail
)

REM ---- running instance check -------------------------------------------
REM  A running copy locks the exe and the files under deploy\log,
REM  which makes the folder impossible to clean.
tasklist /fi "imagename eq %APPNAME%.exe" /nh 2>nul | findstr /i /c:"%APPNAME%.exe" >nul
if not errorlevel 1 (
    echo [ERROR] %APPNAME%.exe is running right now.
    echo         Close every running copy - including the one started
    echo         from the deploy folder - and run this script again.
    goto :fail
)

REM ---- 1/4 : prepare output folder --------------------------------------
REM  The folder is reused and overwritten. Only "log" is kept, so measurement
REM  data produced by a previously deployed copy is never deleted.
echo [1/4] Preparing folder : %OUTDIR%
if not exist "%OUTDIR%" (
    mkdir "%OUTDIR%" || goto :fail
) else (
    for /d %%D in ("%OUTDIR%\*") do (
        if /i not "%%~nxD"=="log" rmdir /s /q "%%D" 2>nul
    )
    del /q /f "%OUTDIR%\*" >nul 2>nul
)
if not exist "%OUTDIR%" (
    echo [ERROR] Could not create %OUTDIR%
    goto :fail
)

REM ---- 2/4 : copy exe ---------------------------------------------------
echo [2/4] Copying exe      : %EXE%
copy /y "%EXE%" "%OUTDIR%\" >nul || goto :fail

REM ---- 3/4 : collect Qt runtime ----------------------------------------
echo [3/4] Collecting Qt runtime (windeployqt)
"%QTDIR%\bin\windeployqt.exe" --release --no-translations --no-opengl-sw --no-system-d3d-compiler "%OUTDIR%\%APPNAME%.exe"
if errorlevel 1 (
    echo [ERROR] windeployqt failed.
    goto :fail
)

REM ---- 4/4 : zip (log folder is not shipped) ----------------------------
echo [4/4] Creating zip
REM  NOTE: Get-ChildItem -Exclude does not filter child items of a folder,
REM        so the log folder is removed with Where-Object instead. Log files
REM        may be open in Excel or in a running copy, and Compress-Archive
REM        cannot read a locked file.
powershell -NoProfile -ExecutionPolicy Bypass -Command "$items = Get-ChildItem -LiteralPath '%OUTDIR%' | Where-Object { $_.Name -ne 'log' }; if ($items) { Compress-Archive -Path $items.FullName -DestinationPath '%~dp0%APPNAME%.zip' -Force }"
if errorlevel 1 (
    echo [WARN] zip step failed - the deploy folder is still usable.
)

echo.
echo ============================================================
echo  DONE
echo    folder : %OUTDIR%
echo    zip    : %~dp0%APPNAME%.zip
echo.
echo  If the target PC shows a VCRUNTIME140.dll error, install
echo  the Visual C++ redistributable (x64):
echo    https://aka.ms/vs/17/release/vc_redist.x64.exe
echo ============================================================
echo.
pause
endlocal
exit /b 0

:fail
echo.
echo Deploy aborted.
echo.
pause
endlocal
exit /b 1
