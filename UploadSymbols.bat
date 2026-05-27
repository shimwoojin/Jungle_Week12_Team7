@echo off
setlocal

set DEBUGGER_PATH=C:\Program Files (x86)\Windows Kits\10\Debuggers\x64
set SRCSRV_PATH=%DEBUGGER_PATH%\srcsrv
set SYMBOL_STORE=\\SYMBOL-SERVER\Symbols\Team7
set PRODUCT_NAME=KraftonEngine_Team7
set RELEASE_BIN=C:\development\Jungle_Week12_Team7\KraftonEngine\Bin\Release
set LOG_DIR=C:\development\Jungle_Week12_Team7\KraftonEngine\Saved\SymbolLogs
set REPO_ROOT=C:\development\Jungle_Week12_Team7
set SOURCE_INDEX_SCRIPT=%REPO_ROOT%\Scripts\GenerateSrcSrvStream.ps1

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

if not exist "%SRCSRV_PATH%\srctool.exe" (
    echo ERROR: srctool.exe not found.
    exit /b 1
)

if not exist "%SRCSRV_PATH%\pdbstr.exe" (
    echo ERROR: pdbstr.exe not found.
    exit /b 1
)

if not exist "%RELEASE_BIN%" (
    echo ERROR: Release bin folder not found: %RELEASE_BIN%
    exit /b 1
)

if not exist "%SOURCE_INDEX_SCRIPT%" (
    echo ERROR: Source indexing script not found: %SOURCE_INDEX_SCRIPT%
    exit /b 1
)

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

echo ========================================
echo Source indexing Release PDBs...
echo Version: %VERSION_NAME%
echo ========================================

for %%F in ("%RELEASE_BIN%\*.pdb") do (
    if exist "%%~fF" call :SourceIndexPdb "%%~fF" "%VERSION_NAME%"
)
if errorlevel 1 exit /b 1

echo ========================================
echo Checking Release PDB source info...
echo Version: %VERSION_NAME%
echo Logs: %LOG_DIR%
echo ========================================

for %%F in ("%RELEASE_BIN%\*.pdb") do (
    if exist "%%~fF" call :CheckPdbSourceInfo "%%~fF" "Release" "%VERSION_NAME%"
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
echo Source info logs: %LOG_DIR%
if /i not "%NO_PAUSE%"=="--no-pause" pause

endlocal
exit /b 0

:SourceIndexPdb
set PDB_FILE=%~1
set VERSION_NAME=%~2

echo.
echo Source indexing PDB:
echo %PDB_FILE%

powershell -NoProfile -ExecutionPolicy Bypass -File "%SOURCE_INDEX_SCRIPT%" -PdbFile "%PDB_FILE%" -RepoRoot "%REPO_ROOT%" -SrcSrvPath "%SRCSRV_PATH%" -LogDir "%LOG_DIR%" -VersionName "%VERSION_NAME%"
exit /b %ERRORLEVEL%

:CheckPdbSourceInfo
set PDB_FILE=%~1
set CONFIG_NAME=%~2
set VERSION_NAME=%~3
set PDB_NAME=%~n1

echo.
echo Checking PDB:
echo %PDB_FILE%

"%SRCSRV_PATH%\srctool.exe" -r "%PDB_FILE%" > "%LOG_DIR%\%CONFIG_NAME%_%VERSION_NAME%_%PDB_NAME%_sources.txt" 2>&1

"%SRCSRV_PATH%\srctool.exe" -u "%PDB_FILE%" > "%LOG_DIR%\%CONFIG_NAME%_%VERSION_NAME%_%PDB_NAME%_unindexed.txt" 2>&1

"%SRCSRV_PATH%\pdbstr.exe" -r -p:"%PDB_FILE%" -s:srcsrv > "%LOG_DIR%\%CONFIG_NAME%_%VERSION_NAME%_%PDB_NAME%_srcsrv.txt" 2>&1

exit /b 0
