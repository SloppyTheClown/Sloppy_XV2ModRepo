@echo off
rem Builds the loader (winmm.dll), its libraries (Lib\lua54.dll,
rem Lib\XV2ModLoaderLib.dll, Lib\XV2UserInterfaceLib.dll) and both launchers into build\Release.
rem   build.bat            incremental Release build
rem   build.bat clean      wipe the build folder first
rem   build.bat install    build, then install into the game
rem   build.bat uninstall  remove the loader and launchers (mods and settings are kept)
rem   build.bat release    build, then write ..\..\_Releases\XV2ModLoader-<VERSION>-XV2_<GAME_VERSION>.zip (extract into the game folder)
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

if /i "%~1"=="uninstall" goto uninstall

where cmake >nul 2>nul || (echo cmake not found on PATH & exit /b 1)

if /i "%~1"=="clean" if exist "%OUT%" rmdir /s /q "%OUT%"

if not exist "%OUT%\CMakeCache.txt" (
    cmake -S "%SRC%." -B "%OUT%" -G "Visual Studio 17 2022" -A x64 || exit /b 1
)
cmake --build "%OUT%" --config Release || (echo. & echo BUILD FAILED & exit /b 1)
echo.
echo Built: %OUT%\Release\

if /i "%~1"=="release" goto release
if /i not "%~1"=="install" exit /b 0
call :checkgame || exit /b 1
tasklist /fi "imagename eq DBXV2.exe" | "%SystemRoot%\System32\find.exe" /i "DBXV2.exe" >nul && (echo The game is running - close it first. & exit /b 1)
rem a disabled copy left by "Launch Unmodded" would be renamed over the new one later
if exist "%GAME%\bin\winmm.dll_disabled" del "%GAME%\bin\winmm.dll_disabled"
set "ML=%GAME%\XV2ModLoader"
if not exist "%ML%\mods" mkdir "%ML%\mods"
if not exist "%ML%\Lib" mkdir "%ML%\Lib"
call :copydll winmm.dll "%GAME%\bin" || exit /b 1
rem the loader's libraries: Lua and the in-game menu / search boxes / file edits
call :copydll lua54.dll "%ML%\Lib" || exit /b 1
call :copydll XV2ModLoaderLib.dll "%ML%\Lib" || exit /b 1
rem the UI library: several mods editing one menu
call :copydll XV2UserInterfaceLib.dll "%ML%\Lib" || exit /b 1
copy /y "%OUT%\Release\Launch Modded.exe" "%ML%\" >nul || exit /b 1
copy /y "%OUT%\Release\Launch Unmodded.exe" "%ML%\" >nul || exit /b 1
copy /y "%SRC%Add To Steam.bat" "%ML%\" >nul || exit /b 1
copy /y "%SRC%Add To Steam.sh" "%ML%\" >nul || exit /b 1
if not exist "%ML%\loader.json" copy "%SRC%loader.json" "%ML%\" >nul
rem the example Lua script, switched off ("_"); left alone once it exists in either state
if not exist "%ML%\mods\_LuaExample" if not exist "%ML%\mods\LuaExample" xcopy /e /i /q "%SRC%examples\_LuaExample" "%ML%\mods\_LuaExample" >nul
rem older versions put these in the game folder itself
call :oldfiles
echo Installed: %GAME%\bin\winmm.dll
echo            %ML%\Lib\lua54.dll
echo            %ML%\Lib\XV2ModLoaderLib.dll
echo            %ML%\Lib\XV2UserInterfaceLib.dll
echo            %ML%\Launch Modded.exe
echo            %ML%\Launch Unmodded.exe
echo            %ML%\Add To Steam.bat
echo            %ML%\Add To Steam.sh   (Steam Deck / Linux)
echo Mods go in %ML%\mods\^<ModName^>\
echo Example Lua script (off): %ML%\mods\_LuaExample\
exit /b 0

:release
rem The zip mirrors what install puts into the game folder: bin\winmm.dll and
rem XV2ModLoader\ with Lib\ (the three DLLs), both launchers, the Steam shortcut
rem scripts, the default loader.json, the switched-off Lua example, README and
rem VERSION. Staged in build\package (not build\release: paths ignore case,
rem that is build\Release with the DLLs).
set "STAGE=%OUT%\package"
set "PKG=%STAGE%\XV2ModLoader"
rem The VERSION file names the zip and ships in the mod loader folder (CHANGELOG.md);
rem GAME_VERSION (repo root) is the game version the build is for.
set /p VERSION=<"%SRC%VERSION"
set /p GAME_VERSION=<"%SRC%..\GAME_VERSION"
set "ZIP=%SRC%..\..\_Releases\XV2ModLoader-%VERSION%-XV2_%GAME_VERSION%.zip"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%\bin" || exit /b 1
mkdir "%PKG%\Lib" || exit /b 1
mkdir "%PKG%\mods" || exit /b 1
copy /y "%OUT%\Release\winmm.dll" "%STAGE%\bin\" >nul || exit /b 1
for %%f in (lua54.dll XV2ModLoaderLib.dll XV2UserInterfaceLib.dll) do copy /y "%OUT%\Release\%%f" "%PKG%\Lib\" >nul || exit /b 1
copy /y "%OUT%\Release\Launch Modded.exe" "%PKG%\" >nul || exit /b 1
copy /y "%OUT%\Release\Launch Unmodded.exe" "%PKG%\" >nul || exit /b 1
copy /y "%SRC%Add To Steam.bat" "%PKG%\" >nul || exit /b 1
copy /y "%SRC%Add To Steam.sh" "%PKG%\" >nul || exit /b 1
copy /y "%SRC%loader.json" "%PKG%\" >nul || exit /b 1
xcopy /e /i /q "%SRC%examples\_LuaExample" "%PKG%\mods\_LuaExample" >nul || exit /b 1
copy /y "%SRC%README.md" "%PKG%\" >nul || exit /b 1
copy /y "%SRC%VERSION" "%PKG%\" >nul || exit /b 1
if not exist "%SRC%..\..\_Releases" mkdir "%SRC%..\..\_Releases"
if exist "%ZIP%" del "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\bin','%PKG%' -DestinationPath '%ZIP%'" || exit /b 1
echo Release: %ZIP%
exit /b 0

:uninstall
call :checkgame || exit /b 1
for %%f in ("%GAME%\bin\winmm.dll" "%GAME%\bin\winmm.dll_disabled" "%GAME%\XV2ModLoader\Launch Modded.exe" "%GAME%\XV2ModLoader\Launch Unmodded.exe" "%GAME%\XV2ModLoader\Add To Steam.bat" "%GAME%\XV2ModLoader\Add To Steam.sh" "%GAME%\XV2ModLoader\Lib\lua54.dll" "%GAME%\XV2ModLoader\Lib\XV2ModLoaderLib.dll" "%GAME%\XV2ModLoader\Lib\XV2UserInterfaceLib.dll") do if exist %%f del %%f
rem only removed when empty: a library someone else added stays
if exist "%GAME%\XV2ModLoader\Lib" rmdir "%GAME%\XV2ModLoader\Lib" 2>nul
call :oldfiles
echo Removed the loader, its libraries and the launchers.
echo Your mods and settings are still in %GAME%\XV2ModLoader - delete that folder too to remove everything.
exit /b 0

rem :copydll name folder - copies build\Release\<name> into folder and compares
rem the copy: once one taken right after linking came out with its last 76 KB
rem zeroed, and the game then wouldn't start. Retries once, then fails.
:copydll
copy /y "%OUT%\Release\%~1" "%~2\" >nul || exit /b 1
"%SystemRoot%\System32\fc.exe" /b "%OUT%\Release\%~1" "%~2\%~1" >nul && exit /b 0
copy /y "%OUT%\Release\%~1" "%~2\" >nul
"%SystemRoot%\System32\fc.exe" /b "%OUT%\Release\%~1" "%~2\%~1" >nul && exit /b 0
echo %~1 did not copy correctly into "%~2" - install again.
exit /b 1

:oldfiles
for %%f in ("%GAME%\Launch Modded.exe" "%GAME%\Launch Unmodded.exe" "%GAME%\Add To Steam.bat") do if exist %%f del %%f
exit /b 0

:checkgame
if exist "%GAME%\bin\DBXV2.exe" exit /b 0
echo Could not find the game at "%GAME%".
echo Set XV2_GAME to the game folder (the one containing bin), e.g.
echo   set XV2_GAME=D:\SteamLibrary\steamapps\common\DB Xenoverse 2
exit /b 1
