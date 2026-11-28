@echo off
rem Builds build\"Revamp Xenoverse 2 Installer.exe" (runs the unmodified Revamp
rem installer on game 1.26.01 / 1.27), build\epatches\<build> (Revamp's exe
rem patches ported to each) and build\lib (Revamp's Xv2CoreLib with current file readers).
rem
rem   build.bat          build; lib\ only if it is missing
rem   build.bat lib      rebuild lib\ as well
rem   build.bat clean    wipe the build folder, then build everything
rem   build.bat release  build, then write ..\..\_Releases\RevampBridge-<VERSION>-XV2_<GAME_VERSION>.zip
rem                      ("Revamp Bridge.exe": no file of Revamp's, rebuilds them from the player's zip)
rem
rem Paths come from RevampBridge.ini (copy RevampBridge.example.ini): RevampDir
rem (the Revamp "Installer" folder) and LibDonor (any LB Mod Installer 3.32+ exe).
rem Needs Python 3 with capstone (release: also bsdiff4); lib\ also the .NET SDK
rem and ilspycmd - see README.md.
setlocal
set "SRC=%~dp0"
set "OUT=%SRC%build"
set "INI=%SRC%RevampBridge.ini"
set "FW=%SystemRoot%\Microsoft.NET\Framework64\v4.0.30319"

if not exist "%INI%" (echo RevampBridge.ini not found - copy RevampBridge.example.ini to RevampBridge.ini and set its paths. & exit /b 1)
for /f "usebackq tokens=1,* delims==" %%a in ("%INI%") do (
    if /i "%%a"=="RevampDir" set "REVAMP=%%~b"
    if /i "%%a"=="LibDonor" set "DONOR=%%~b"
)

if /i "%~1"=="clean" if exist "%OUT%" rmdir /s /q "%OUT%"
if not exist "%OUT%" mkdir "%OUT%"

"%FW%\csc.exe" /nologo /target:winexe /platform:x64 /optimize+ ^
  /r:"%FW%\WPF\PresentationFramework.dll" /r:"%FW%\WPF\PresentationCore.dll" /r:"%FW%\WPF\WindowsBase.dll" /r:System.Xaml.dll /r:System.Xml.dll ^
  /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /win32manifest:"%SRC%src\app.manifest" ^
  /out:"%OUT%\Revamp Xenoverse 2 Installer.exe" "%SRC%src\RevampBridge.cs" "%SRC%src\PostInstall.cs" "%SRC%src\Release.cs" "%SRC%src\AssemblyInfo.cs" || (echo. & echo BUILD FAILED & exit /b 1)
copy /y "%INI%" "%OUT%\" >nul || exit /b 1
if exist "%OUT%\iggymaps.txt" del "%OUT%\iggymaps.txt"
copy /y "%SRC%src\iggymaps_*.txt" "%OUT%\" >nul || exit /b 1

rem Revamp's exe patches, ported to each supported build (read from the .installinfo).
if not exist "%OUT%\epatches" mkdir "%OUT%\epatches"
del /q "%OUT%\epatches\*.xml" 2>nul
python "%SRC%src\port_epatches.py" "%REVAMP%\Revamp Xenoverse 2 Installer.installinfo" "%OUT%\epatches" || (echo. & echo EPATCH PORT FAILED - needs Python 3 with capstone: pip install capstone & exit /b 1)

if /i "%~1"=="lib" goto lib
if exist "%OUT%\lib\xv2corelib.dll" goto done
:lib
if not exist "%REVAMP%\Revamp Xenoverse 2 Installer.exe" (echo RevampDir in RevampBridge.ini does not contain the Revamp installer. & exit /b 1)
if not exist "%DONOR%" (echo LibDonor in RevampBridge.ini does not exist: "%DONOR%" & exit /b 1)
powershell -NoProfile -ExecutionPolicy Bypass -File "%SRC%src\build_lib.ps1" -RevampExe "%REVAMP%\Revamp Xenoverse 2 Installer.exe" -Donor "%DONOR%" -OutLib "%OUT%\lib" || (echo. & echo LIB BUILD FAILED & exit /b 1)

:done
echo.
echo Built: %OUT%\Revamp Xenoverse 2 Installer.exe
if /i not "%~1"=="release" exit /b 0
echo.
powershell -NoProfile -ExecutionPolicy Bypass -File "%SRC%src\make_release.ps1" -Project "%SRC%." || (echo. & echo RELEASE FAILED & exit /b 1)
exit /b 0
