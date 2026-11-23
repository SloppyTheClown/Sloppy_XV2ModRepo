# Builds the release: build\package\Revamp Bridge (...)\ and
# _Releases\RevampBridge-<VERSION> (Revamp 5.1.1 to XV2 <GAME_VERSION>).zip.
#
# The release carries no file of Revamp's: "Revamp Bridge.exe" is the bridge
# with the patch set of src\make_patches.py embedded, and rebuilds lib\ and
# epatches\ from the player's Revamp zip (src\Release.cs).
#
# Needs a finished normal build (build.bat), RevampDir in RevampBridge.ini and
# Python 3 with bsdiff4.
param([Parameter(Mandatory)] [string]$Project)
$ErrorActionPreference = 'Stop'
$build = Join-Path $Project 'build'
$work = Join-Path $env:TEMP 'XV2_RevampBridge_release'
$fw = "$env:SystemRoot\Microsoft.NET\Framework64\v4.0.30319"

function Step($msg) { Write-Host "== $msg" }

$revampDir = (Get-Content (Join-Path $Project 'RevampBridge.ini') | Where-Object { $_ -match '^\s*RevampDir\s*=' }) -replace '^\s*RevampDir\s*=\s*', '' -replace '"', ''
$revampExe = Join-Path $revampDir 'Revamp Xenoverse 2 Installer.exe'
$installinfo = Join-Path $revampDir 'Revamp Xenoverse 2 Installer.installinfo'
if (-not (Test-Path $installinfo)) { throw "RevampDir in RevampBridge.ini does not contain the Revamp installer: $revampDir" }
if (-not (Test-Path "$build\lib\xv2corelib.dll")) { throw 'build\lib is missing - run build.bat first.' }

# Costura stores dependencies as deflated "costura.<name>.dll.compressed"
# (the same reading as src\Release.cs does at install time).
Add-Type -AssemblyName System.IO.Compression
function Unpack($asmFile, $dir) {
    New-Item -ItemType Directory -Force $dir | Out-Null
    $asm = [Reflection.Assembly]::ReflectionOnlyLoad([IO.File]::ReadAllBytes($asmFile))
    foreach ($res in $asm.GetManifestResourceNames()) {
        if ($res -notmatch '^costura\.(.+\.dll)\.compressed$') { continue }
        $in = New-Object IO.Compression.DeflateStream($asm.GetManifestResourceStream($res), [IO.Compression.CompressionMode]::Decompress)
        $out = [IO.File]::Create((Join-Path $dir $matches[1]))
        $in.CopyTo($out); $out.Close(); $in.Close()
    }
}

if (Test-Path $work) { Remove-Item -Recurse -Force $work }
Step 'unpacking the libraries Revamp''s installer embeds'
Unpack $revampExe "$work\exe"
Unpack "$work\exe\xv2corelib.dll" "$work\corelib"

Step 'making the patch set'
& python (Join-Path $PSScriptRoot 'make_patches.py') "$work\exe" "$work\corelib" $installinfo $build "$work\out"
if ($LASTEXITCODE) { throw 'make_patches.py failed (needs Python 3 with bsdiff4: pip install bsdiff4)' }

Step 'compiling Revamp Bridge.exe with the patch set'
$game = (Get-Content (Join-Path $Project '..\GAME_VERSION') -TotalCount 1).Trim()
$stage = Join-Path $build "package\Revamp Bridge (Revamp 5.1.1 to XV2 $game)"
if (Test-Path (Split-Path $stage)) { Remove-Item -Recurse -Force (Split-Path $stage) }
New-Item -ItemType Directory $stage | Out-Null
$resources = Get-ChildItem -Recurse -File "$work\out" | ForEach-Object {
    $name = $_.FullName.Substring("$work\out\".Length).Replace('\', '/')
    "/resource:$($_.FullName),$name"
}
& "$fw\csc.exe" /nologo /target:winexe /platform:x64 /optimize+ `
    "/r:$fw\WPF\PresentationFramework.dll" "/r:$fw\WPF\PresentationCore.dll" "/r:$fw\WPF\WindowsBase.dll" `
    /r:System.Xaml.dll /r:System.Xml.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll `
    $resources "/out:$stage\Revamp Bridge.exe" `
    "$PSScriptRoot\RevampBridge.cs" "$PSScriptRoot\PostInstall.cs" "$PSScriptRoot\Release.cs"
if ($LASTEXITCODE) { throw 'compiling Revamp Bridge.exe failed' }
Copy-Item (Join-Path $Project 'release\*') $stage

Step 'checking that no file of Revamp''s is in the package'
$revampHashes = @{}
Get-ChildItem -Recurse -File "$work\exe", "$work\corelib" | ForEach-Object { $revampHashes[(Get-FileHash $_.FullName).Hash] = $_.Name }
$revampHashes[(Get-FileHash $revampExe).Hash] = 'the installer exe'
foreach ($f in Get-ChildItem -Recurse -File $stage) {
    if ($revampHashes.ContainsKey((Get-FileHash $f.FullName).Hash)) { throw "$($f.Name) is Revamp's $($revampHashes[(Get-FileHash $f.FullName).Hash])" }
    if ($f.Extension -notin '.exe', '.txt', '.bat') { throw "unexpected file in the package: $($f.Name)" }
}

$version = (Get-Content (Join-Path $Project 'VERSION') -TotalCount 1).Trim()
$releases = [IO.Path]::GetFullPath((Join-Path $Project '..\..\_Releases'))
New-Item -ItemType Directory -Force $releases | Out-Null
# The name says which Revamp it takes and which game it targets (players
# check the Revamp version first; a newer Revamp needs a new bridge).
$zip = Join-Path $releases "RevampBridge-$version (Revamp 5.1.1 to XV2 $game).zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path $stage -DestinationPath $zip
Remove-Item -Recurse -Force $work
Write-Host "Release: $zip ($([int]((Get-Item $zip).Length / 1KB)) KB)"
