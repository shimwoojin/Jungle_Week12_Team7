@echo off
setlocal

set DEBUGGER_PATH=C:\Program Files (x86)\Windows Kits\10\Debuggers\x64
set SYMBOL_STORE=\\SYMBOL-SERVER\Symbols\Team7
set PRODUCT_NAME=KraftonEngine_Team7
set RELEASE_BIN=C:\development\Jungle_Week12_Team7\KraftonEngine\Bin\Release

set VERSION_NAME=%~1
set NO_PAUSE=%~2

if "%VERSION_NAME%"=="" (
    echo Usage: UploadSymbols.bat [VersionName]
    echo Example: UploadSymbols.bat Week12_Release_20260527_2130
    exit /b 1
)

if not exist "%DEBUGGER_PATH%\symstore.exe" (
    echo ERROR: symstore.exe not found.
    exit /b 1
)

if not exist "%RELEASE_BIN%" (
    echo ERROR: Release bin folder not found: %RELEASE_BIN%
    exit /b 1
)

if not exist "%SYMBOL_STORE%" (
    echo ERROR: Symbol store path not accessible: %SYMBOL_STORE%
    exit /b 1
)

echo ========================================
echo Uploading Release symbols...
echo Version: %VERSION_NAME%
echo ========================================

"%DEBUGGER_PATH%\symstore.exe" add /r /f "%RELEASE_BIN%\*.pdb" /s "%SYMBOL_STORE%" /t "%PRODUCT_NAME%" /v "%VERSION_NAME%"
if errorlevel 1 exit /b 1

"%DEBUGGER_PATH%\symstore.exe" add /r /f "%RELEASE_BIN%\*.exe" /s "%SYMBOL_STORE%" /t "%PRODUCT_NAME%" /v "%VERSION_NAME%"
if errorlevel 1 exit /b 1

"%DEBUGGER_PATH%\symstore.exe" add /r /f "%RELEASE_BIN%\*.dll" /s "%SYMBOL_STORE%" /t "%PRODUCT_NAME%" /v "%VERSION_NAME%"
if errorlevel 1 exit /b 1

echo.
echo Upload complete.
if /i not "%NO_PAUSE%"=="--no-pause" pause

endlocal
