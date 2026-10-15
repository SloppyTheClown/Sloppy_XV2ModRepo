@echo off
rem Builds build\Release\SaveSync.dll, a mod for XV2 Mod Loader.
rem
rem   build.bat            incremental Release build
rem   build.bat clean      wipe the build folder first
rem   build.bat install    build, then install into XV2 Mod Loader's mods folder
rem   build.bat uninstall  remove SaveSync.dll (your config and toggles are kept)
rem   build.bat release    build, then write ..\..\_Releases\SaveSync-<VERSION>.zip (extract into the game folder)
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
set "MOD=%GAME%\XV2ModLoader\mods\SaveSync"

if /i "%~1"=="uninstall" goto uninstall

where cmake >nul 2>nul || (echo cmake not found on PATH & exit /b 1)

if /i "%~1"=="clean" if exist "%OUT%" rmdir /s /q "%OUT%"

if not exist "%OUT%\CMakeCache.txt" (
    cmake -S "%SRC%." -B "%OUT%" -G "Visual Studio 17 2022" -A x64 || exit /b 1
)
cmake --build "%OUT%" --config Release || (echo. & echo BUILD FAILED & exit /b 1)
echo.
echo Built: %OUT%\Release\SaveSync.dll

if /i "%~1"=="release" goto release
if /i not "%~1"=="install" exit /b 0
call :checkgame || exit /b 1
if not exist "%GAME%\XV2ModLoader" (
    echo XV2 Mod Loader is not installed - install it first, this mod is loaded by it.
    exit /b 1
)
tasklist /fi "imagename eq DBXV2.exe" | "%SystemRoot%\System32\find.exe" /i "DBXV2.exe" >nul && (echo The game is running - close it first. & exit /b 1)
if not exist "%MOD%" mkdir "%MOD%"
copy /y "%OUT%\Release\SaveSync.dll" "%MOD%\" >nul || exit /b 1
fc /b "%OUT%\Release\SaveSync.dll" "%MOD%\SaveSync.dll" >nul || (echo The copied DLL differs from the built one - run install again. & exit /b 1)
if not exist "%MOD%\savesync.json" copy "%SRC%savesync.json" "%MOD%\" >nul
echo Installed: %MOD%\SaveSync.dll
echo Config:    %MOD%\savesync.json
exit /b 0

:release
rem The zip mirrors the game folder: XV2ModLoader\mods\SaveSync\ with the DLL,
rem the default config and the README. Staged in build\package (not
rem build\release: paths ignore case, that is build\Release with the DLL).
set "STAGE=%OUT%\package"
rem The VERSION file names the zip and ships in the mod folder (CHANGELOG.md).
set /p VERSION=<"%SRC%VERSION"
set "ZIP=%SRC%..\..\_Releases\SaveSync-%VERSION%.zip"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%\XV2ModLoader\mods\SaveSync" || exit /b 1
copy /y "%OUT%\Release\SaveSync.dll" "%STAGE%\XV2ModLoader\mods\SaveSync\" >nul || exit /b 1
copy /y "%SRC%savesync.json" "%STAGE%\XV2ModLoader\mods\SaveSync\" >nul || exit /b 1
copy /y "%SRC%README.md" "%STAGE%\XV2ModLoader\mods\SaveSync\" >nul || exit /b 1
copy /y "%SRC%VERSION" "%STAGE%\XV2ModLoader\mods\SaveSync\" >nul || exit /b 1
if not exist "%SRC%..\..\_Releases" mkdir "%SRC%..\..\_Releases"
if exist "%ZIP%" del "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\XV2ModLoader' -DestinationPath '%ZIP%'" || exit /b 1
echo Release: %ZIP%
exit /b 0

:uninstall
call :checkgame || exit /b 1
if exist "%MOD%\SaveSync.dll" del "%MOD%\SaveSync.dll"
echo Removed %MOD%\SaveSync.dll
echo Your settings and toggles (savesync.sav) are still in %MOD% - delete that folder too to remove everything.
exit /b 0

:checkgame
if exist "%GAME%\bin\DBXV2.exe" exit /b 0
echo Could not find the game at "%GAME%".
echo Set XV2_GAME to the game folder (the one containing bin), e.g.
echo   set XV2_GAME=D:\SteamLibrary\steamapps\common\DB Xenoverse 2
exit /b 1
