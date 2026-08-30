@echo off
setlocal

rem ---------------------------------------------------------------------------
rem ChronoSpace - Windows Development package
rem
rem Same pipeline as Package.bat, built in the Development configuration:
rem logging, stat commands and the console stay in, and the binary is not
rem optimized as hard. Use this for internal test builds.
rem
rem Like Package.bat, this passes -AdditionalCookerOptions so the cook
rem process's Model Context Protocol server does not collide with the running
rem editor on port 8000. Safe to run with the editor open.
rem
rem Usage:  Package_Dev.bat [ArchiveDirectory]
rem   default archive dir : <project>\Packaged_Dev
rem   engine override     : set UE_ROOT=D:\UE_5.8
rem
rem Shipping build: Package.bat
rem ---------------------------------------------------------------------------

set "PROJECT_DIR=%~dp0"
set "PROJECT=%PROJECT_DIR%ChronoSpace.uproject"

if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "RUNUAT=%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat"

if "%~1"=="" (set "ARCHIVE=%PROJECT_DIR%Packaged_Dev") else (set "ARCHIVE=%~1")

rem Must differ from the editor's port
rem (Project Settings > Model Context Protocol > Server Port Number, default 8000).
set "COOK_MCP_PORT=8123"

if not exist "%RUNUAT%" (
    echo [Package_Dev] RunUAT.bat not found: "%RUNUAT%"
    echo [Package_Dev] Set UE_ROOT to your engine folder, e.g.  set UE_ROOT=D:\UE_5.8
    exit /b 1
)

echo [Package_Dev] Project : %PROJECT%
echo [Package_Dev] Engine  : %UE_ROOT%
echo [Package_Dev] Config  : Development
echo [Package_Dev] Archive : %ARCHIVE%
echo.

call "%RUNUAT%" BuildCookRun ^
    -project="%PROJECT%" ^
    -target=ChronoSpace ^
    -platform=Win64 ^
    -clientconfig=Development ^
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
    echo [Package_Dev] FAILED - Development - exit code %EXITCODE%
) else (
    echo [Package_Dev] OK - Development  -^>  %ARCHIVE%
)

rem Keep the window open when launched by double-click from Explorer.
echo %cmdcmdline% | findstr /i /c:"/c" >nul && pause

exit /b %EXITCODE%
