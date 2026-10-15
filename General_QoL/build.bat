@echo off
rem Builds build\Release\General_QoL.dll, a mod for XV2 Mod Loader.
rem
rem   build.bat            incremental Release build
rem   build.bat clean      wipe the build folder first
rem   build.bat install    build, then install into XV2 Mod Loader's mods folder
rem   build.bat uninstall  remove General_QoL.dll (your config is kept)
rem
rem The game folder is %XV2_GAME% if set, otherwise Steam's own library.
setlocal
set "SRC=%~dp0"
set "OUT=%SRC%build"

if defined XV2_GAME (
    set "GAME=%XV2_GAME%"
) else (
    for /f "tokens=2,*" %%a in ('reg query "HKCU\Software\Valve\Steam" /v SteamPath 2^>nul ^| "%SystemRoot%\System32\find.exe" "SteamPath"') do set "STEAM=%%b"
)
if not defined GAME if defined STEAM set "GAME=%STEAM:/=\%\steamapps\common\DB Xenoverse 2"
set "MOD=%GAME%\XV2ModLoader\mods\General_QoL"

if /i "%~1"=="uninstall" goto uninstall

where cmake >nul 2>nul || (echo cmake not found on PATH & exit /b 1)

if /i "%~1"=="clean" if exist "%OUT%" rmdir /s /q "%OUT%"

if not exist "%OUT%\CMakeCache.txt" (
    cmake -S "%SRC%." -B "%OUT%" -G "Visual Studio 17 2022" -A x64 || exit /b 1
)
cmake --build "%OUT%" --config Release || (echo. & echo BUILD FAILED & exit /b 1)
echo.
echo Built: %OUT%\Release\General_QoL.dll

if /i not "%~1"=="install" exit /b 0
call :checkgame || exit /b 1
if not exist "%GAME%\XV2ModLoader" (
    echo XV2 Mod Loader is not installed - install it first, this mod is loaded by it.
    exit /b 1
)
tasklist /fi "imagename eq DBXV2.exe" | "%SystemRoot%\System32\find.exe" /i "DBXV2.exe" >nul && (echo The game is running - close it first. & exit /b 1)
if not exist "%MOD%" mkdir "%MOD%"
copy /y "%OUT%\Release\General_QoL.dll" "%MOD%\" >nul || exit /b 1
if not exist "%MOD%\general_qol.json" copy "%SRC%general_qol.json" "%MOD%\" >nul
echo Installed: %MOD%\General_QoL.dll
echo Config:    %MOD%\general_qol.json
exit /b 0

:uninstall
call :checkgame || exit /b 1
if exist "%MOD%\General_QoL.dll" del "%MOD%\General_QoL.dll"
echo Removed %MOD%\General_QoL.dll
echo Your settings are still in %MOD% - delete that folder too to remove everything.
exit /b 0

:checkgame
if exist "%GAME%\bin\DBXV2.exe" exit /b 0
echo Could not find the game at "%GAME%".
echo Set XV2_GAME to the game folder (the one containing bin), e.g.
echo   set XV2_GAME=D:\SteamLibrary\steamapps\common\DB Xenoverse 2
exit /b 1
