@echo off
rem Ki Blast Override: equip a charged ki blast on Customize > Skills (from ki_blast_override.json).
rem
rem   build.bat             build build\Release\KiBlastOverride.dll (XV2 Mod Loader mod)
rem   build.bat clean       wipe the build folder first
rem   build.bat install     build, then install into XV2 Mod Loader's mods folder
rem   build.bat uninstall   remove KiBlastOverride.dll (your config is kept)
rem   build.bat release    build, then write ..\..\_Releases\KiBlastOverride-<VERSION>-XV2_<GAME_VERSION>.zip (extract into the game folder)
rem
rem The game folder is %XV2_GAME% if set, otherwise Steam's own library.
rem Needs cmake + Visual Studio 2022.
setlocal
set "SRC=%~dp0"
set "OUT=%SRC%build"

if defined XV2_GAME (
    set "GAME=%XV2_GAME%"
) else (
    for /f "tokens=2,*" %%a in ('reg query "HKCU\Software\Valve\Steam" /v SteamPath 2^>nul ^| "%SystemRoot%\System32\find.exe" "SteamPath"') do set "STEAM=%%b"
)
if not defined GAME if defined STEAM set "GAME=%STEAM:/=\%\steamapps\common\DB Xenoverse 2"
set "MOD=%GAME%\XV2ModLoader\mods\KiBlastOverride"

if /i "%~1"=="uninstall" goto uninstall

where cmake >nul 2>nul || (echo cmake not found on PATH & exit /b 1)
if /i "%~1"=="clean" if exist "%OUT%" rmdir /s /q "%OUT%"
if not exist "%OUT%\CMakeCache.txt" (
    cmake -S "%SRC%." -B "%OUT%" -G "Visual Studio 17 2022" -A x64 || exit /b 1
)
cmake --build "%OUT%" --config Release || (echo. & echo BUILD FAILED & exit /b 1)
echo.
echo Built: %OUT%\Release\KiBlastOverride.dll

if /i "%~1"=="release" goto release
if /i not "%~1"=="install" exit /b 0
call :checkgame || exit /b 1
if not exist "%GAME%\XV2ModLoader" (
    echo XV2 Mod Loader is not installed - install it first, this mod is loaded by it.
    exit /b 1
)
tasklist /fi "imagename eq DBXV2.exe" | "%SystemRoot%\System32\find.exe" /i "DBXV2.exe" >nul && (echo The game is running - close it first. & exit /b 1)
if not exist "%MOD%" mkdir "%MOD%"
copy /y "%OUT%\Release\KiBlastOverride.dll" "%MOD%\" >nul || exit /b 1
fc /b "%OUT%\Release\KiBlastOverride.dll" "%MOD%\KiBlastOverride.dll" >nul || (echo The installed copy differs from the build - install again. & exit /b 1)
if not exist "%MOD%\ki_blast_override.json" copy "%SRC%ki_blast_override.json" "%MOD%\" >nul
echo Installed: %MOD%\KiBlastOverride.dll
echo Config:    %MOD%\ki_blast_override.json
exit /b 0

:release
rem The zip mirrors the game folder: XV2ModLoader\mods\KiBlastOverride\ with the DLL,
rem the default config, the README and VERSION. Staged in build\package
rem (not build\release: paths ignore case, that is build\Release with the DLL).
set "STAGE=%OUT%\package"
set "PKG=%STAGE%\XV2ModLoader\mods\KiBlastOverride"
rem The VERSION file names the zip and ships in the mod folder (CHANGELOG.md);
rem GAME_VERSION (repo root) is the game version the build is for.
set /p VERSION=<"%SRC%VERSION"
set /p GAME_VERSION=<"%SRC%..\GAME_VERSION"
set "ZIP=%SRC%..\..\_Releases\KiBlastOverride-%VERSION%-XV2_%GAME_VERSION%.zip"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%PKG%" || exit /b 1
copy /y "%OUT%\Release\KiBlastOverride.dll" "%PKG%\" >nul || exit /b 1
copy /y "%SRC%ki_blast_override.json" "%PKG%\" >nul || exit /b 1
copy /y "%SRC%README.md" "%PKG%\" >nul || exit /b 1
copy /y "%SRC%VERSION" "%PKG%\" >nul || exit /b 1
if not exist "%SRC%..\..\_Releases" mkdir "%SRC%..\..\_Releases"
if exist "%ZIP%" del "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\XV2ModLoader' -DestinationPath '%ZIP%'" || exit /b 1
echo Release: %ZIP%
exit /b 0

:uninstall
call :checkgame || exit /b 1
if exist "%MOD%\KiBlastOverride.dll" del "%MOD%\KiBlastOverride.dll"
echo Removed %MOD%\KiBlastOverride.dll
rem Animations the mod converted for CaCs and wrote into the skill folders, listed one per line.
if exist "%MOD%\converted_eans.txt" (
    for /f "usebackq delims=" %%f in ("%MOD%\converted_eans.txt") do del /q "%GAME%\data\%%f" 2>nul
    del "%MOD%\converted_eans.txt"
    echo Removed the converted animations
)
echo Your settings are still in %MOD% - delete that folder too to remove everything.
exit /b 0

:checkgame
if exist "%GAME%\bin\DBXV2.exe" exit /b 0
echo Could not find the game at "%GAME%".
echo Set XV2_GAME to the game folder (the one containing bin), e.g.
echo   set XV2_GAME=D:\SteamLibrary\steamapps\common\DB Xenoverse 2
exit /b 1

