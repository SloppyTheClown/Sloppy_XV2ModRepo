@echo off
rem Installs chara_search.lua, a Lua script for XV2 Mod Loader, and the
rem character select scripts it hands to the loader (iggy\CharaSele.abc and
rem iggy\CharaSele_XV2Patcher.abc, built by ClaudeTools\iggy\menu_abc.py). The
rem loader swaps one into the game's own CHARASELE.iggy as the game reads it;
rem no game file is replaced.
rem
rem   build.bat install    chara_search.lua + the .abc files -> XV2 Mod Loader's mods\CharaSearch
rem   build.bat uninstall  remove them (favorite_characters.sav, the player's favorites, stays)
rem
rem Both also remove what the older CharaSearch.dll version installed (the
rem DLL, its chara_search.json and log, only along with the DLL). Scripts can be (re)installed while
rem the game runs: F1 in game reloads them; the menu file is read when the
rem character select opens.
rem The game folder is %XV2_GAME% if set, otherwise Steam's own library.
setlocal
set "SRC=%~dp0"

if defined XV2_GAME (
    set "GAME=%XV2_GAME%"
) else (
    for /f "tokens=2,*" %%a in ('reg query "HKCU\Software\Valve\Steam" /v SteamPath 2^>nul ^| "%SystemRoot%\System32\find.exe" "SteamPath"') do set "STEAM=%%b"
)
if not defined GAME if defined STEAM set "GAME=%STEAM:/=\%\steamapps\common\DB Xenoverse 2"
set "MOD=%GAME%\XV2ModLoader\mods\CharaSearch"

if not exist "%GAME%\bin\DBXV2.exe" (
    echo Could not find the game at "%GAME%".
    echo Set XV2_GAME to the game folder ^(the one containing bin^).
    exit /b 1
)
if /i "%~1"=="uninstall" goto uninstall
if /i not "%~1"=="install" (echo usage: build.bat install^|uninstall & exit /b 1)
if not exist "%GAME%\XV2ModLoader" (
    echo XV2 Mod Loader is not installed - install it first, this script runs in it.
    exit /b 1
)
for %%f in (CharaSele.abc CharaSele_XV2Patcher.abc) do if not exist "%SRC%iggy\%%f" (
    echo iggy\%%f is missing - run ClaudeTools\iggy\menu_abc.py first.
    exit /b 1
)
call :olddll || exit /b 1
if not exist "%MOD%" mkdir "%MOD%"
copy /y "%SRC%chara_search.lua" "%MOD%\" >nul || exit /b 1
copy /y "%SRC%iggy\CharaSele.abc" "%MOD%\" >nul || exit /b 1
copy /y "%SRC%iggy\CharaSele_XV2Patcher.abc" "%MOD%\" >nul || exit /b 1
echo Installed: %MOD%\chara_search.lua   (F1 in game reloads scripts)
echo            %MOD%\CharaSele.abc, CharaSele_XV2Patcher.abc
exit /b 0

:uninstall
call :olddll || exit /b 1
rem favorite_characters.sav (the player's favorites) stays; the folder goes only when empty
for %%f in (chara_search.lua CharaSele.abc CharaSele_XV2Patcher.abc favorite_characters.sav.tmp) do if exist "%MOD%\%%f" del "%MOD%\%%f"
if exist "%MOD%" rmdir "%MOD%" 2>nul
if exist "%MOD%\favorite_characters.sav" (echo Removed Chara Search; kept %MOD%\favorite_characters.sav) else echo Removed %MOD%
exit /b 0

rem Removes the older CharaSearch.dll version (the loader loads every DLL in
rem the folder: both would hook the character select). Needs the game closed.
:olddll
if not exist "%MOD%\CharaSearch.dll" exit /b 0
tasklist /fi "imagename eq DBXV2.exe" | "%SystemRoot%\System32\find.exe" /i "DBXV2.exe" >nul && (echo The game is running - close it first ^(the old CharaSearch.dll is in use^). & exit /b 1)
del "%MOD%\CharaSearch.dll" || exit /b 1
echo Removed the old CharaSearch.dll
rem its config and log (chara_search.json is the script's own config since)
for %%f in (chara_search.json chara_search.log) do if exist "%MOD%\%%f" del "%MOD%\%%f"
exit /b 0
