@echo off
rem Training QoL: in Training, a button hands your controller and the camera
rem to the Training partner and back.
rem
rem   build.bat             build build\Release\Training_QoL.dll (XV2 Mod Loader mod)
rem   build.bat clean       wipe the build folder first
rem   build.bat install     build, then install into XV2 Mod Loader's mods folder
rem   build.bat uninstall   remove Training_QoL.dll (your config is kept)
rem   build.bat release     build, then write ..\..\_Releases\Training_QoL-<VERSION>-XV2_<GAME_VERSION>.zip (extract into the game folder)
rem
rem XV2Patcher-only version (an Epatch, no mod loader; epatch\make_epatch.py):
rem   build.bat epatch            build build\epatch\Training_QoL.xml
rem   build.bat epatch_install    build, then copy it into XV2PATCHER\Epatches
rem   build.bat epatch_uninstall  remove it from XV2PATCHER\Epatches
rem   build.bat epatch_release    build, then write ..\..\_Releases\Training_QoL_Epatch_1.27-<VERSION>.zip
rem Only one version can be installed: each install refuses while the other is in.
rem The Epatch is built per game version; the targets above are for 1.27, these
rem for older versions (no install target):
rem   build.bat epatch126          build build\epatch_1.26.01\Training_QoL.xml
rem   build.bat epatch126_release  build, then write ..\..\_Releases\Training_QoL_Epatch_1.26.01-<VERSION>.zip
rem   build.bat epatch125          build build\epatch_1.25.02\Training_QoL.xml  (1.25.02, the version Revamp 5.1.1 targets)
rem   build.bat epatch125_release  build, then write ..\..\_Releases\Training_QoL_Epatch_1.25.02-<VERSION>.zip
rem
rem The game folder is %XV2_GAME% if set, otherwise Steam's own library.
rem Needs cmake + Visual Studio 2022 (the Epatch: Python + Visual Studio's ml64).
setlocal
set "SRC=%~dp0"
set "OUT=%SRC%build"

if defined XV2_GAME (
    set "GAME=%XV2_GAME%"
) else (
    for /f "tokens=2,*" %%a in ('reg query "HKCU\Software\Valve\Steam" /v SteamPath 2^>nul ^| "%SystemRoot%\System32\find.exe" "SteamPath"') do set "STEAM=%%b"
)
if not defined GAME if defined STEAM set "GAME=%STEAM:/=\%\steamapps\common\DB Xenoverse 2"
set "MOD=%GAME%\XV2ModLoader\mods\Training_QoL"
set "EPATCH=%GAME%\XV2PATCHER\Epatches\Training_QoL.xml"

if /i "%~1"=="uninstall" goto uninstall
if /i "%~1"=="epatch_uninstall" goto epatch_uninstall
if /i "%~1"=="epatch" goto epatch
if /i "%~1"=="epatch_install" goto epatch
if /i "%~1"=="epatch_release" goto epatch
if /i "%~1"=="epatch126" goto epatch
if /i "%~1"=="epatch126_release" goto epatch
if /i "%~1"=="epatch125" goto epatch
if /i "%~1"=="epatch125_release" goto epatch

where cmake >nul 2>nul || (echo cmake not found on PATH & exit /b 1)
if /i "%~1"=="clean" if exist "%OUT%" rmdir /s /q "%OUT%"
if not exist "%OUT%\CMakeCache.txt" (
    cmake -S "%SRC%." -B "%OUT%" -G "Visual Studio 17 2022" -A x64 || exit /b 1
)
cmake --build "%OUT%" --config Release || (echo. & echo BUILD FAILED & exit /b 1)
echo.
echo Built: %OUT%\Release\Training_QoL.dll

if /i "%~1"=="release" goto release
if /i not "%~1"=="install" exit /b 0
call :checkgame || exit /b 1
if not exist "%GAME%\XV2ModLoader" (
    echo XV2 Mod Loader is not installed - install it first, this mod is loaded by it.
    exit /b 1
)
tasklist /fi "imagename eq DBXV2.exe" | "%SystemRoot%\System32\find.exe" /i "DBXV2.exe" >nul && (echo The game is running - close it first. & exit /b 1)
if exist "%EPATCH%" (
    echo The XV2Patcher version is installed - remove it first ^(Build_Epatch_Uninstall.bat^).
    exit /b 1
)
if not exist "%MOD%" mkdir "%MOD%"
copy /y "%OUT%\Release\Training_QoL.dll" "%MOD%\" >nul || exit /b 1
fc /b "%OUT%\Release\Training_QoL.dll" "%MOD%\Training_QoL.dll" >nul || (echo The installed copy differs from the build - install again. & exit /b 1)
if not exist "%MOD%\training_qol.json" copy "%SRC%training_qol.json" "%MOD%\" >nul
echo Installed: %MOD%\Training_QoL.dll
echo Config:    %MOD%\training_qol.json
exit /b 0

:release
rem The zip mirrors the game folder: XV2ModLoader\mods\Training_QoL\ with the
rem DLL, the default config, the README and VERSION. Staged in build\package
rem (not build\release: paths ignore case, that is build\Release with the DLL).
set "STAGE=%OUT%\package"
set /p VERSION=<"%SRC%VERSION"
set /p GAME_VERSION=<"%SRC%..\GAME_VERSION"
set "ZIP=%SRC%..\..\_Releases\Training_QoL-%VERSION%-XV2_%GAME_VERSION%.zip"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%\XV2ModLoader\mods\Training_QoL" || exit /b 1
copy /y "%OUT%\Release\Training_QoL.dll" "%STAGE%\XV2ModLoader\mods\Training_QoL\" >nul || exit /b 1
copy /y "%SRC%training_qol.json" "%STAGE%\XV2ModLoader\mods\Training_QoL\" >nul || exit /b 1
copy /y "%SRC%README.md" "%STAGE%\XV2ModLoader\mods\Training_QoL\" >nul || exit /b 1
copy /y "%SRC%VERSION" "%STAGE%\XV2ModLoader\mods\Training_QoL\" >nul || exit /b 1
if not exist "%SRC%..\..\_Releases" mkdir "%SRC%..\..\_Releases"
if exist "%ZIP%" del "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\XV2ModLoader' -DestinationPath '%ZIP%'" || exit /b 1
echo Release: %ZIP%
exit /b 0

:uninstall
call :checkgame || exit /b 1
if exist "%MOD%\Training_QoL.dll" del "%MOD%\Training_QoL.dll"
echo Removed %MOD%\Training_QoL.dll
echo Your settings are still in %MOD% - delete that folder too to remove everything.
exit /b 0

:epatch
rem GAME_VERSION picks epatch\game_<version>.inc (the addresses) in make_epatch.py.
set "GAME_VERSION=1.27"
set "EPATCH_OUT=%OUT%\epatch"
if /i "%~1"=="epatch126" set "GAME_VERSION=1.26.01"
if /i "%~1"=="epatch126_release" set "GAME_VERSION=1.26.01"
if /i "%~1"=="epatch125" set "GAME_VERSION=1.25.02"
if /i "%~1"=="epatch125_release" set "GAME_VERSION=1.25.02"
if not "%GAME_VERSION%"=="1.27" set "EPATCH_OUT=%OUT%\epatch_%GAME_VERSION%"
where python >nul 2>nul || (echo python not found on PATH & exit /b 1)
if not exist "%EPATCH_OUT%" mkdir "%EPATCH_OUT%"
python "%SRC%epatch\make_epatch.py" %GAME_VERSION% "%EPATCH_OUT%\Training_QoL.xml" || (echo. & echo BUILD FAILED & exit /b 1)
if /i "%~1"=="epatch_release" goto epatch_release
if /i "%~1"=="epatch126_release" goto epatch_release
if /i "%~1"=="epatch125_release" goto epatch_release
if /i not "%~1"=="epatch_install" exit /b 0
call :checkgame || exit /b 1
if not exist "%GAME%\XV2PATCHER\Epatches" (
    echo XV2Patcher is not installed - install it first, it applies this patch.
    exit /b 1
)
tasklist /fi "imagename eq DBXV2.exe" | "%SystemRoot%\System32\find.exe" /i "DBXV2.exe" >nul && (echo The game is running - close it first. & exit /b 1)
if exist "%MOD%\Training_QoL.dll" (
    echo The mod loader version is installed - remove it first ^(Build_Uninstall.bat^).
    exit /b 1
)
copy /y "%EPATCH_OUT%\Training_QoL.xml" "%EPATCH%" >nul || exit /b 1
echo Installed: %EPATCH%
exit /b 0

:epatch_release
rem The zip mirrors the game folder: XV2PATCHER\Epatches\Training_QoL.xml.
set "STAGE=%EPATCH_OUT%\package"
set /p VERSION=<"%SRC%VERSION"
set "ZIP=%SRC%..\..\_Releases\Training_QoL_Epatch_%GAME_VERSION%-%VERSION%.zip"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%\XV2PATCHER\Epatches" || exit /b 1
copy /y "%EPATCH_OUT%\Training_QoL.xml" "%STAGE%\XV2PATCHER\Epatches\" >nul || exit /b 1
if not exist "%SRC%..\..\_Releases" mkdir "%SRC%..\..\_Releases"
if exist "%ZIP%" del "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\XV2PATCHER' -DestinationPath '%ZIP%'" || exit /b 1
echo Release: %ZIP%
exit /b 0

:epatch_uninstall
call :checkgame || exit /b 1
if exist "%EPATCH%" del "%EPATCH%"
echo Removed %EPATCH%
exit /b 0

:checkgame
if exist "%GAME%\bin\DBXV2.exe" exit /b 0
echo Could not find the game at "%GAME%".
echo Set XV2_GAME to the game folder (the one containing bin), e.g.
echo   set XV2_GAME=D:\SteamLibrary\steamapps\common\DB Xenoverse 2
exit /b 1
