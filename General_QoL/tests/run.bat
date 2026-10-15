@echo off
rem Offline check of the file edits against the game's own files.
rem Needs Python and the game (XV2_GAME, or the default Steam path).
setlocal
set "ROOT=%~dp0.."
set "OUT=%ROOT%\build"
if not defined XV2_GAME set "XV2_GAME=%ProgramFiles(x86)%\Steam\steamapps\common\DB Xenoverse 2"
if not exist "%OUT%\CMakeCache.txt" (echo Build the mod first: build.bat & exit /b 1)
cmake --build "%OUT%" --config Release --target tsrtest datatest maptest hudtest || exit /b 1
python "%~dp0check.py" extract "%XV2_GAME%" "%OUT%\orig.tsd" || exit /b 1
"%OUT%\Release\tsrtest.exe" "%OUT%\orig.tsd" "%OUT%\patched.tsd" || exit /b 1
python "%~dp0check.py" compare "%OUT%\orig.tsd" "%OUT%\patched.tsd" || exit /b 1
python "%~dp0check.py" data "%XV2_GAME%" "%OUT%\Release\datatest.exe" "%OUT%" || exit /b 1
python "%~dp0check.py" map "%XV2_GAME%" "%OUT%\Release\maptest.exe" "%OUT%" || exit /b 1
rem HUD stage names: only with x2m transformations installed (pre-baked.xml)
if exist "%XV2_GAME%\data\pre-baked.xml" "%OUT%\Release\hudtest.exe" "%XV2_GAME%\data"
