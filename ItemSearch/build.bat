@echo off
rem Installs item_search.lua, a Lua script for XV2 Mod Loader, and the item
rem list script it hands to the loader (iggy\ItemList.abc, built by
rem ClaudeTools\iggy\itemlist_abc.py). The loader swaps it into the game's own
rem ITEMLIST.iggy as the game reads it; no game file is replaced.
rem
rem   build.bat install    item_search.lua + ItemList.abc -> XV2 Mod Loader's mods\ItemSearch
rem   build.bat uninstall  remove them (favorites.sav, the player's favorites, stays)
rem
rem Both also remove the loose data\ui\iggy\ITEMLIST.iggy that older versions
rem installed (only when it is that file: a copy was kept in mods\ItemSearch).
rem Scripts can be (re)installed while the game runs: F1 in game reloads them;
rem the menu file is read when a list opens.
rem The game folder is %XV2_GAME% if set, otherwise Steam's own library.
setlocal
set "SRC=%~dp0"

if defined XV2_GAME (
    set "GAME=%XV2_GAME%"
) else (
    for /f "tokens=2,*" %%a in ('reg query "HKCU\Software\Valve\Steam" /v SteamPath 2^>nul ^| "%SystemRoot%\System32\find.exe" "SteamPath"') do set "STEAM=%%b"
)
if not defined GAME if defined STEAM set "GAME=%STEAM:/=\%\steamapps\common\DB Xenoverse 2"
set "MOD=%GAME%\XV2ModLoader\mods\ItemSearch"
set "IGGY=%GAME%\data\ui\iggy\ITEMLIST.iggy"

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
if not exist "%SRC%iggy\ItemList.abc" (
    echo iggy\ItemList.abc is missing - run ClaudeTools\iggy\itemlist_abc.py first.
    exit /b 1
)
call :oldiggy
if not exist "%MOD%" mkdir "%MOD%"
copy /y "%SRC%item_search.lua" "%MOD%\" >nul || exit /b 1
copy /y "%SRC%iggy\ItemList.abc" "%MOD%\" >nul || exit /b 1
echo Installed: %MOD%\item_search.lua   (F1 in game reloads scripts)
echo            %MOD%\ItemList.abc
if exist "%IGGY%" (
    echo Note: another mod's "%IGGY%" is installed. The search still filters,
    echo but the rows only change when a list is opened again.
)
exit /b 0

:uninstall
call :oldiggy
rem favorites.sav (the player's favorites) stays; the folder goes only when empty
for %%f in (item_search.lua ItemList.abc favorites.sav.tmp) do if exist "%MOD%\%%f" del "%MOD%\%%f"
if exist "%MOD%" rmdir "%MOD%" 2>nul
if exist "%MOD%\favorites.sav" (echo Removed Item Search; kept %MOD%\favorites.sav) else echo Removed %MOD%
exit /b 0

rem Removes the loose ITEMLIST.iggy an older Item Search installed: the one
rem identical to its copy in mods\ItemSearch. Another mod's is left alone.
:oldiggy
if not exist "%MOD%\ITEMLIST.iggy" exit /b 0
if exist "%IGGY%" fc /b "%IGGY%" "%MOD%\ITEMLIST.iggy" >nul 2>&1 && del "%IGGY%" && echo Removed the old loose %IGGY%
del "%MOD%\ITEMLIST.iggy"
exit /b 0
