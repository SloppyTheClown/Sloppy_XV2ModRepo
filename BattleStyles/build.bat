@echo off
rem Builds build\Release\BattleStyles.dll, a mod for XV2 Mod Loader.
rem
rem   build.bat            incremental Release build
rem   build.bat clean      wipe the build folder first
rem   build.bat install    build, then install into XV2 Mod Loader's mods folder
rem   build.bat uninstall  remove BattleStyles.dll, data\battle_styles\fit,
rem                        data\sound\SE\Battle\Chara\BattleStyles and the prop
rem                        files data\chara\*\*_bsprop* (your config is kept)
rem   build.bat release    build, then write ..\..\_Releases\BattleStyles-<VERSION>.zip (extract into the game folder)
rem
rem install and release also put the moveset converter in the mod folder:
rem "Convert_Moveset (Drag installers here).bat" + tools\moveset_to_style.exe:
rem tools\moveset_to_style.py and the cpk reader it uses (XV2DataEdit's
rem xv2data\cpk.py) frozen with PyInstaller, so players need no Python. Building
rem it needs Python 3 on PATH; PyInstaller goes into a venv under %TEMP%.
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
set "MOD=%GAME%\XV2ModLoader\mods\BattleStyles"

if /i "%~1"=="uninstall" goto uninstall

where cmake >nul 2>nul || (echo cmake not found on PATH & exit /b 1)

if /i "%~1"=="clean" if exist "%OUT%" rmdir /s /q "%OUT%"

if not exist "%OUT%\CMakeCache.txt" (
    cmake -S "%SRC%." -B "%OUT%" -G "Visual Studio 17 2022" -A x64 || exit /b 1
)
cmake --build "%OUT%" --config Release || (echo. & echo BUILD FAILED & exit /b 1)
echo.
echo Built: %OUT%\Release\BattleStyles.dll

if /i not "%~1"=="release" if /i not "%~1"=="install" exit /b 0
call :buildconverter || exit /b 1
if /i "%~1"=="release" goto release
call :checkgame || exit /b 1
if not exist "%GAME%\XV2ModLoader" (
    echo XV2 Mod Loader is not installed - install it first, this mod is loaded by it.
    exit /b 1
)
tasklist /fi "imagename eq DBXV2.exe" | "%SystemRoot%\System32\find.exe" /i "DBXV2.exe" >nul && (echo The game is running - close it first. & exit /b 1)
if not exist "%MOD%" mkdir "%MOD%"
copy /y "%OUT%\Release\BattleStyles.dll" "%MOD%\" >nul || exit /b 1
fc /b "%OUT%\Release\BattleStyles.dll" "%MOD%\BattleStyles.dll" >nul || (echo The copied DLL differs from the built one - run install again. & exit /b 1)
if not exist "%MOD%\battle_styles.json" copy "%SRC%battle_styles.json" "%MOD%\" >nul
call :converter "%MOD%" || exit /b 1
echo Installed: %MOD%\BattleStyles.dll
echo Config:    %MOD%\battle_styles.json
echo Converter: %MOD%\Convert_Moveset (Drag installers here).bat
exit /b 0

:release
rem The zip mirrors the game folder: XV2ModLoader\mods\BattleStyles\ with the
rem DLL, the default config, the README and the converter. Staged in
rem build\package (not build\release: paths ignore case, that is build\Release).
set "STAGE=%OUT%\package"
set "PKG=%STAGE%\XV2ModLoader\mods\BattleStyles"
rem The VERSION file names the zip and ships in the mod folder (CHANGELOG.md).
set /p VERSION=<"%SRC%VERSION"
set "ZIP=%SRC%..\..\_Releases\BattleStyles-%VERSION%.zip"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%PKG%" || exit /b 1
copy /y "%OUT%\Release\BattleStyles.dll" "%PKG%\" >nul || exit /b 1
copy /y "%SRC%battle_styles.json" "%PKG%\" >nul || exit /b 1
copy /y "%SRC%README.md" "%PKG%\" >nul || exit /b 1
copy /y "%SRC%VERSION" "%PKG%\" >nul || exit /b 1
call :converter "%PKG%" || exit /b 1
if not exist "%SRC%..\..\_Releases" mkdir "%SRC%..\..\_Releases"
if exist "%ZIP%" del "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\XV2ModLoader' -DestinationPath '%ZIP%'" || exit /b 1
echo Release: %ZIP%
exit /b 0

:buildconverter
rem Freezes tools\moveset_to_style.py into %OUT%\converter\moveset_to_style.exe.
rem PyInstaller works in %TEMP% and the exe is then copied with copy (CopyFile):
rem on the Z: DrivePool drive an exe written by Python read+write won't start
rem (CLAUDE.md Pitfalls). Also writes the version note and Python's licence.
where python >nul 2>nul || (echo Python 3 not found on PATH - needed to build the moveset converter exe. & exit /b 1)
set "PYBUILD=%TEMP%\BattleStyles_converter_build"
set "PYVENV=%PYBUILD%\venv"
rem 6.11.1: first PyInstaller release with Python 3.13 support.
set "PYINSTALLER_VERSION=6.11.1"
if not exist "%PYVENV%\Scripts\pyinstaller.exe" (
    python -m venv "%PYVENV%" || exit /b 1
    "%PYVENV%\Scripts\python.exe" -m pip install --disable-pip-version-check -q "pyinstaller==%PYINSTALLER_VERSION%" || exit /b 1
)
"%PYVENV%\Scripts\pyinstaller.exe" --noconfirm --clean --log-level WARN --onefile --console ^
    --name moveset_to_style --paths "%SRC%..\..\Tools\XV2DataEdit" --hidden-import xv2data.cpk ^
    --distpath "%PYBUILD%\dist" --workpath "%PYBUILD%\work" --specpath "%PYBUILD%" ^
    "%SRC%tools\moveset_to_style.py" || (echo CONVERTER BUILD FAILED & exit /b 1)
if not exist "%OUT%\converter" mkdir "%OUT%\converter" || exit /b 1
copy /y "%PYBUILD%\dist\moveset_to_style.exe" "%OUT%\converter\" >nul || exit /b 1
"%PYVENV%\Scripts\python.exe" -c "import sys, PyInstaller; print('tools\\moveset_to_style.exe = tools\\moveset_to_style.py + XV2DataEdit xv2data\\cpk.py, frozen with:'); print('Python ' + sys.version.split()[0] + ' - https://www.python.org/ - PSF License (moveset_to_style.PYTHON_LICENSE.txt)'); print('PyInstaller ' + PyInstaller.__version__ + ' - https://pyinstaller.org/ - GPL 2 with the bootloader exception (the exe may be any licence)')" > "%OUT%\converter\moveset_to_style.VERSION.txt" || exit /b 1
for /f "usebackq delims=" %%P in (`python -c "import sys; print(sys.base_prefix)"`) do copy /y "%%P\LICENSE.txt" "%OUT%\converter\moveset_to_style.PYTHON_LICENSE.txt" >nul || exit /b 1
echo Built: %OUT%\converter\moveset_to_style.exe
exit /b 0

:converter
rem The moveset converter into folder %1 (the exe finds the game it sits in).
if not exist "%~1\tools" mkdir "%~1\tools" || exit /b 1
copy /y "%SRC%Convert_Moveset (Drag installers here).bat" "%~1\" >nul || exit /b 1
copy /y "%OUT%\converter\*" "%~1\tools\" >nul || exit /b 1
fc /b "%OUT%\converter\moveset_to_style.exe" "%~1\tools\moveset_to_style.exe" >nul || (echo The copied converter exe differs from the built one - run again. & exit /b 1)
rem Older installs ran the script with the player's Python.
if exist "%~1\tools\moveset_to_style.py" del "%~1\tools\moveset_to_style.py"
if exist "%~1\tools\xv2data" rmdir /s /q "%~1\tools\xv2data"
if exist "%~1\tools\__pycache__" rmdir /s /q "%~1\tools\__pycache__"
rem LB Mod Installer for mods without their own (x2m, rar of loose files).
if not exist "%~1\tools\lb_installer" mkdir "%~1\tools\lb_installer" || exit /b 1
copy /y "%SRC%tools\lb_installer\*" "%~1\tools\lb_installer\" >nul || exit /b 1
exit /b 0

:uninstall
call :checkgame || exit /b 1
if exist "%MOD%\BattleStyles.dll" del "%MOD%\BattleStyles.dll"
echo Removed %MOD%\BattleStyles.dll
if exist "%MOD%\Convert_Moveset (Drag installers here).bat" del "%MOD%\Convert_Moveset (Drag installers here).bat"
if exist "%MOD%\Convert_Moveset.log" del "%MOD%\Convert_Moveset.log"
if exist "%MOD%\tools" rmdir /s /q "%MOD%\tools"
echo Removed the moveset converter from %MOD%
rem Moveset copies and merged sound banks the mod writes while playing (fitted to the race).
if exist "%GAME%\data\battle_styles\fit" rmdir /s /q "%GAME%\data\battle_styles\fit"
if exist "%GAME%\data\battle_styles" rmdir "%GAME%\data\battle_styles" 2>nul
echo Removed %GAME%\data\battle_styles\fit
if exist "%GAME%\data\sound\SE\Battle\Chara\BattleStyles" rmdir /s /q "%GAME%\data\sound\SE\Battle\Chara\BattleStyles"
echo Removed %GAME%\data\sound\SE\Battle\Chara\BattleStyles
rem Prop meshes and skeletons the mod writes into the style's chara folder (prop_fit.h kPropFileTag).
for /d %%c in ("%GAME%\data\chara\*") do del /q "%%c\*_bsprop*" 2>nul
echo Removed %GAME%\data\chara\*\*_bsprop*
echo Your settings are still in %MOD% - delete that folder too to remove everything.
exit /b 0

:checkgame
if exist "%GAME%\bin\DBXV2.exe" exit /b 0
echo Could not find the game at "%GAME%".
echo Set XV2_GAME to the game folder (the one containing bin), e.g.
echo   set XV2_GAME=D:\SteamLibrary\steamapps\common\DB Xenoverse 2
exit /b 1
