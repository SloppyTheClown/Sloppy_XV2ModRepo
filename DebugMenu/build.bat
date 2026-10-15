@echo off
rem Installs debug_menu.lua, a Lua script for XV2 Mod Loader (nothing to build).
rem The item getter reads the item list from the game's own files when it is
rem first opened, so nothing else is needed.
rem
rem   build.bat install    copy it into XV2 Mod Loader's mods\DebugMenu folder
rem   build.bat uninstall  remove that folder
rem   build.bat release    write ..\..\_Releases\DebugMenu-<VERSION>.zip (extract into the game folder)
rem
rem Scripts can be (re)installed while the game runs: F1 in game reloads them.
rem The game folder is %XV2_GAME% if set, otherwise Steam's own library.
setlocal
set "SRC=%~dp0"

if defined XV2_GAME (
    set "GAME=%XV2_GAME%"
) else (
    for /f "tokens=2,*" %%a in ('reg query "HKCU\Software\Valve\Steam" /v SteamPath 2^>nul ^| "%SystemRoot%\System32\find.exe" "SteamPath"') do set "STEAM=%%b"
)
if not defined GAME if defined STEAM set "GAME=%STEAM:/=\%\steamapps\common\DB Xenoverse 2"
set "MOD=%GAME%\XV2ModLoader\mods\DebugMenu"

if not exist "%GAME%\bin\DBXV2.exe" (
    echo Could not find the game at "%GAME%".
    echo Set XV2_GAME to the game folder ^(the one containing bin^).
    exit /b 1
)
if /i "%~1"=="uninstall" goto uninstall
if /i "%~1"=="release" goto release
if /i not "%~1"=="install" (echo usage: build.bat install^|uninstall^|release & exit /b 1)
if not exist "%GAME%\XV2ModLoader" (
    echo XV2 Mod Loader is not installed - install it first, this script is run by it.
    exit /b 1
)
if not exist "%MOD%" mkdir "%MOD%"
copy /y "%SRC%debug_menu.lua" "%MOD%\" >nul || exit /b 1
rem older versions wrote the item list into io_packages at install
if exist "%MOD%\io_packages\debug_menu_items.lua" del "%MOD%\io_packages\debug_menu_items.lua"
if exist "%MOD%\io_packages" rmdir "%MOD%\io_packages" 2>nul
echo Installed: %MOD%\debug_menu.lua   (~ in game opens the menu, F1 reloads scripts)
exit /b 0

:release
rem The zip mirrors the game folder: XV2ModLoader\mods\DebugMenu\ with the
rem script and the README.
set "STAGE=%SRC%build\package"
rem The VERSION file names the zip and ships in the mod folder (CHANGELOG.md).
set /p VERSION=<"%SRC%VERSION"
set "ZIP=%SRC%..\..\_Releases\DebugMenu-%VERSION%.zip"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%\XV2ModLoader\mods\DebugMenu" || exit /b 1
copy /y "%SRC%debug_menu.lua" "%STAGE%\XV2ModLoader\mods\DebugMenu\" >nul || exit /b 1
copy /y "%SRC%README.md" "%STAGE%\XV2ModLoader\mods\DebugMenu\" >nul || exit /b 1
copy /y "%SRC%VERSION" "%STAGE%\XV2ModLoader\mods\DebugMenu\" >nul || exit /b 1
if not exist "%SRC%..\..\_Releases" mkdir "%SRC%..\..\_Releases"
if exist "%ZIP%" del "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\XV2ModLoader' -DestinationPath '%ZIP%'" || exit /b 1
echo Release: %ZIP%
exit /b 0

:uninstall
if exist "%MOD%" rmdir /s /q "%MOD%"
echo Removed %MOD%
exit /b 0
