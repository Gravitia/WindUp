@echo off
setlocal

rem ---------------------------------------------------------------------------
rem ChronoSpace - Windows Shipping package
rem
rem Runs the same Build + Cook + Stage + Package + Archive pipeline as the
rem editor's File > Package Project, through UAT.
rem
rem Difference from the editor button: this passes -AdditionalCookerOptions,
rem which the editor's packaging path cannot. It moves the cook process's
rem Model Context Protocol server to a spare port, so it no longer collides
rem with the running editor on 8000. Safe to run with the editor open.
rem
rem Usage:  Package.bat [ArchiveDirectory]
rem   default archive dir : <project>\Packaged
rem   engine override     : set UE_ROOT=D:\UE_5.8
rem ---------------------------------------------------------------------------

set "PROJECT_DIR=%~dp0"
set "PROJECT=%PROJECT_DIR%ChronoSpace.uproject"

if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "RUNUAT=%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat"

if "%~1"=="" (set "ARCHIVE=%PROJECT_DIR%Packaged") else (set "ARCHIVE=%~1")

rem Must differ from the editor's port
rem (Project Settings > Model Context Protocol > Server Port Number, default 8000).
set "COOK_MCP_PORT=8123"

if not exist "%RUNUAT%" (
    echo [Package] RunUAT.bat not found: "%RUNUAT%"
    echo [Package] Set UE_ROOT to your engine folder, e.g.  set UE_ROOT=D:\UE_5.8
    exit /b 1
)

echo [Package] Project : %PROJECT%
echo [Package] Engine  : %UE_ROOT%
echo [Package] Archive : %ARCHIVE%
echo.

call "%RUNUAT%" BuildCookRun ^
    -project="%PROJECT%" ^
    -target=ChronoSpace ^
    -platform=Win64 ^
    -clientconfig=Shipping ^
    -installed ^
    -nop4 ^
    -utf8output ^
    -nocompileeditor ^
    -skipbuildeditor ^
    -build ^
    -cook ^
    -stage ^
    -package ^
    -archive ^
    -archivedirectory="%ARCHIVE%" ^
    -pak ^
    -iostore ^
    -compressed ^
    -prereqs ^
    -CrashReporter ^
    -AdditionalCookerOptions="-ModelContextProtocolPort=%COOK_MCP_PORT%"

set "EXITCODE=%ERRORLEVEL%"
echo.
if not "%EXITCODE%"=="0" (
    echo [Package] FAILED - exit code %EXITCODE%
) else (
    echo [Package] OK  -^>  %ARCHIVE%
)

rem Keep the window open when launched by double-click from Explorer.
echo %cmdcmdline% | findstr /i /c:"/c" >nul && pause

exit /b %EXITCODE%
