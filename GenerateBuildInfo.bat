@echo off
setlocal

set SOLUTION_DIR=%~dp0
set OUTPUT_FILE=%SOLUTION_DIR%KraftonEngine\Source\Engine\Platform\BuildInfo.h
set PRODUCT_NAME=KraftonEngine_Team7
set SYMBOL_PATH=srv*C:\\SymbolCache*\\\\SYMBOL-SERVER\\Symbols\\Team7

set BUILD_CONFIG=%~1
set BUILD_VERSION=%~2

if "%BUILD_CONFIG%"=="" (
    set BUILD_CONFIG=Release
)

if "%BUILD_VERSION%"=="" (
    set BUILD_VERSION=LocalBuild
)

set GIT_COMMIT=Unknown
for /f %%i in ('git -C "%SOLUTION_DIR%." rev-parse --short HEAD 2^>nul') do set GIT_COMMIT=%%i

set BUILD_TIME=Unknown
for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "Get-Date -Format 'yyyy-MM-dd HH:mm:ss'"`) do set BUILD_TIME=%%i

echo ========================================
echo Generating BuildInfo.h
echo Config: %BUILD_CONFIG%
echo Version: %BUILD_VERSION%
echo Commit: %GIT_COMMIT%
echo BuildTime: %BUILD_TIME%
echo ========================================

(
echo #pragma once
echo.
echo namespace BuildInfo
echo {
echo 	inline constexpr const char* ProductName = "%PRODUCT_NAME%";
echo 	inline constexpr const char* BuildConfig = "%BUILD_CONFIG%";
echo 	inline constexpr const char* BuildVersion = "%BUILD_VERSION%";
echo 	inline constexpr const char* GitCommit = "%GIT_COMMIT%";
echo 	inline constexpr const char* SymbolPath = "%SYMBOL_PATH%";
echo 	inline constexpr const char* BuildTime = "%BUILD_TIME%";
echo }
) > "%OUTPUT_FILE%"

endlocal
