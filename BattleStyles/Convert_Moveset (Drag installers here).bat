@echo off
rem Turns a moveset mod (LB Mod Installer .exe / .installinfo, an .x2m
rem character, a .rar/.zip/.7z download or a folder of any of these or of
rem loose moveset files) into a separate Battle Style with its own installer,
rem written next to the mod as "<name> Installer (Battle Styles Compatible).exe"
rem and then started. Drop the mod's folder or file on this file, or
rem double-click it and pick the folder. The output is also written to
rem Convert_Moveset.log next to this file.
setlocal
set "INPUT=%~1"
if "%INPUT%"=="" for /f "usebackq delims=" %%P in (`powershell -NoProfile -Command "Add-Type -AssemblyName System.Windows.Forms; $dialog = New-Object System.Windows.Forms.FolderBrowserDialog; $dialog.Description = 'Pick the moveset mod folder'; if ($dialog.ShowDialog() -eq 'OK') { $dialog.SelectedPath }"`) do set "INPUT=%%P"
if "%INPUT%"=="" (
  echo No mod picked.
  pause
  exit /b 1
)
rem tools\moveset_to_style.exe = tools\moveset_to_style.py with Python bundled
rem (PyInstaller, build.bat :converter), so no Python install is needed.
"%~dp0tools\moveset_to_style.exe" "%INPUT%"
pause