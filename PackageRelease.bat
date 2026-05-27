@echo off
setlocal

set SOLUTION_DIR=%~dp0
set PROJECT_DIR=%SOLUTION_DIR%KraftonEngine
set BUILD_OUTPUT=%PROJECT_DIR%\Bin\Release
set RELEASE_DIR=%SOLUTION_DIR%ReleaseBuild
set RELEASE_BIN=%RELEASE_DIR%\Bin
set VERSION_NAME=%~1
set PRODUCT_NAME=KraftonEngine_Team7
set SYMBOL_PATH=srv*C:\SymbolCache*\\SYMBOL-SERVER\Symbols\Team7

if "%VERSION_NAME%"=="" (
    echo Usage: PackageRelease.bat [VersionName]
    echo Example: PackageRelease.bat Week12_Release_20260527_2130
    exit /b 1
)

set GIT_COMMIT=Unknown
for /f %%i in ('git -C "%SOLUTION_DIR%." rev-parse --short HEAD 2^>nul') do set GIT_COMMIT=%%i

set BUILD_TIME=Unknown
for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "Get-Date -Format 'yyyy-MM-dd HH:mm:ss'"`) do set BUILD_TIME=%%i

echo ============================================
echo  Package Release
echo  Version: %VERSION_NAME%
echo ============================================

echo.
echo [1/5] Generating build metadata...
call "%SOLUTION_DIR%GenerateBuildInfo.bat" Release "%VERSION_NAME%"
if errorlevel 1 exit /b 1

set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -property installationPath`) do set VS_PATH=%%i
if not defined VS_PATH (
    echo ERROR: Visual Studio installation not found.
    exit /b 1
)
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -no_logo

echo.
echo [2/5] Building Release x64...
msbuild "%SOLUTION_DIR%KraftonEngine.sln" /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if errorlevel 1 exit /b 1

echo.
echo [3/5] Preparing package directory...
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"
mkdir "%RELEASE_BIN%"

echo.
echo [4/5] Copying runtime files...
copy "%BUILD_OUTPUT%\KraftonEngine.exe" "%RELEASE_BIN%\" >nul
xcopy "%BUILD_OUTPUT%\*.dll" "%RELEASE_BIN%\" /y /q >nul
xcopy "%PROJECT_DIR%\Shaders" "%RELEASE_DIR%\Shaders\" /e /i /q >nul
xcopy "%PROJECT_DIR%\Content" "%RELEASE_DIR%\Content\" /e /i /q >nul
xcopy "%PROJECT_DIR%\Settings" "%RELEASE_DIR%\Settings\" /e /i /q >nul

(
echo @echo off
echo cd /d "%%~dp0"
echo start "" "%%~dp0Bin\KraftonEngine.exe"
) > "%RELEASE_DIR%\Play.bat"

(
echo Product: %PRODUCT_NAME%
echo Config: Release
echo BuildVersion: %VERSION_NAME%
echo GitCommit: %GIT_COMMIT%
echo SymbolPath: %SYMBOL_PATH%
echo Executable: KraftonEngine.exe
echo BuildTime: %BUILD_TIME%
) > "%RELEASE_DIR%\BuildInfo.txt"

echo.
echo [5/5] Uploading symbols...
call "%SOLUTION_DIR%UploadSymbols.bat" "%VERSION_NAME%" --no-pause
if errorlevel 1 exit /b 1

echo.
echo ============================================
echo  Package complete: %RELEASE_DIR%
echo ============================================

endlocal
