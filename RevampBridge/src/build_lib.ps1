# Builds lib\: Revamp's own Xv2CoreLib (the fork its installer was written
# against) with the IDB, QXD and CST readers of a current LB Mod Installer,
# which know the item, quest and character-select formats of game 1.26.
#
#   1. take both Xv2CoreLibs out of the two installers (Costura resources)
#   2. decompile both (ilspycmd), put the donor's IDB/QXD/CST folders into
#      Revamp's source, fix the known decompiler artifacts
#   3. compile, then check every attribute of the result against the
#      originals - a mismatch fails the build
param(
    [Parameter(Mandatory)] [string]$RevampExe,
    [Parameter(Mandatory)] [string]$Donor,
    [Parameter(Mandatory)] [string]$OutLib
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
$src = $PSScriptRoot
$work = Join-Path $env:TEMP 'XV2_RevampBridge_build'
# Whole format folders, plus the character-slot classes: XV2Patcher 4.7's
# XV2P_SLOTS.x2s has an 11th field per slot (flag_kfk) that the fork rejects.
$swapped = 'IDB', 'QXD', 'CST'
$swappedFiles = 'Eternity\CharaSlotsFile.cs', 'Eternity\CharaSlot.cs', 'Eternity\CharaCostumeSlot.cs'
$swapRe = '^Xv2CoreLib\.((' + ($swapped -join '|') + ')\.|Eternity\.(CharaSlotsFile|CharaSlot|CharaCostumeSlot)(::| ))'
$fw = "$env:SystemRoot\Microsoft.NET\Framework64\v4.0.30319"

function Step($msg) { Write-Host "== $msg" }

# Costura stores dependencies as deflated "costura.<name>.dll.compressed".
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

function Decompile($dll, $dir) {
    & $ilspy -p -o $dir $dll | Out-Null
    if ($LASTEXITCODE) { throw "ilspycmd failed on $dll" }
}

$ilspy = Join-Path $env:USERPROFILE '.dotnet\tools\ilspycmd.exe'
if (-not (Test-Path $ilspy)) {
    throw "ilspycmd not found. Install it with:  dotnet tool install -g ilspycmd --version 9.1.0.7988"
}
if (-not (Get-Command dotnet -ErrorAction SilentlyContinue)) { throw 'The .NET SDK (dotnet) is required.' }
if (-not (Test-Path "${env:ProgramFiles(x86)}\Reference Assemblies\Microsoft\Framework\.NETFramework\v4.7")) {
    throw '.NET Framework 4.7 targeting pack not found (Visual Studio installer: ".NET Framework 4.7 targeting pack").'
}

if (Test-Path $work) { Remove-Item -Recurse -Force $work }
New-Item -ItemType Directory $work | Out-Null

Step 'extracting libraries from the installers'
# The Revamp installer embeds Xv2CoreLib + YAXLib; Xv2CoreLib in turn embeds
# its own dependencies (LB_Common, image libraries, ...).
Unpack $RevampExe "$work\revamp"
Unpack "$work\revamp\xv2corelib.dll" "$work\refs"
Copy-Item "$work\revamp\yaxlib.dll" "$work\refs\yaxlib.dll"
Unpack $Donor "$work\donor"
if (-not (Test-Path "$work\donor\xv2corelib.dll")) { throw "No Xv2CoreLib inside $Donor - is it an LB Mod Installer?" }
$donorText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes("$work\donor\xv2corelib.dll"))
if ($donorText.IndexOf('ENTRY_SIZE_V3') -lt 0) {
    throw "The Xv2CoreLib in $Donor is too old for game 1.26 (no IDB v3). Use a newer LB Mod Installer."
}

Step 'decompiling (a few minutes)'
Decompile "$work\revamp\xv2corelib.dll" "$work\src"
Decompile "$work\donor\xv2corelib.dll" "$work\donor_src"

Step "taking $($swapped -join ', ') and the character slot classes from the donor"
foreach ($n in $swapped) {
    Remove-Item -Recurse -Force "$work\src\Xv2CoreLib.$n"
    Copy-Item -Recurse "$work\donor_src\Xv2CoreLib.$n" "$work\src\Xv2CoreLib.$n"
}
foreach ($f in $swappedFiles) {
    $ns, $file = $f -split '\\'
    Copy-Item -Force "$work\donor_src\Xv2CoreLib.$ns\$file" "$work\src\Xv2CoreLib.$ns\$file"
}

Step 'fixing decompiler artifacts'
function Edit-Source($rel, [scriptblock]$fix) {
    $p = Join-Path "$work\src" $rel
    $t = [IO.File]::ReadAllText($p); $n = & $fix $t
    if ($n -eq $t) { throw "Expected decompiler artifact not found in $rel - the Revamp library differs from the one this script was written for." }
    [IO.File]::WriteAllText($p, $n)
}
# Half -> float conversions came out as explicit operator calls.
Get-ChildItem -Recurse "$work\src" -Filter *.cs | ForEach-Object {
    $t = [IO.File]::ReadAllText($_.FullName)
    if ($t.Contains('Half.op_Implicit(')) { [IO.File]::WriteAllText($_.FullName, $t.Replace('Half.op_Implicit(', '(float)(')) }
}
Edit-Source 'Xv2CoreLib.EMB_CLASS\EmbEntry.cs' {
    param($t) $t.Replace('((object)System.Runtime.CompilerServices.Unsafe.As<ImageEngineFormat, ImageEngineFormat>(ref ImageFormat)/*cast due to .constrained prefix*/).ToString()', 'ImageFormat.ToString()') }
Edit-Source 'Xv2CoreLib.Resource.Image\TextureHelper.cs' {
    param($t) $t.Replace('bitmap.Save(memoryStream, ImageFormat.Png)', 'bitmap.Save(memoryStream, System.Drawing.Imaging.ImageFormat.Png)').
        Replace("`t`tImageFormat format = image.Format;", "`t`tPfim.ImageFormat format = image.Format;").
        Replace('switch (dxgiFormat - 70)', 'switch ((int)dxgiFormat - 70)') }
Edit-Source 'Xv2CoreLib.Resource\AsyncObservableCollection.cs' {
    param($t) $t.Replace("`tpublic event NotifyCollectionChangedEventHandler CollectionChanged;", "`t[field: NonSerialized]`n`tpublic event NotifyCollectionChangedEventHandler CollectionChanged;") }

# Attribute dumps of the originals: the truth for DefaultValue types, and
# what the result is checked against.
& "$fw\csc.exe" -nologo -platform:x86 "-out:$work\attrdump.exe" "$src\attrdump.cs"
if ($LASTEXITCODE) { throw 'attrdump.cs failed to compile' }
function Dump($dll, $file) {
    & "$work\attrdump.exe" $dll "$work\refs" | Set-Content -Encoding UTF8 $file
    if ($LASTEXITCODE) { throw "attrdump failed on $dll" }
}
Dump "$work\revamp\xv2corelib.dll" "$work\attr_revamp.txt"
Dump "$work\donor\xv2corelib.dll" "$work\attr_donor.txt"
$expected = @(Get-Content "$work\attr_revamp.txt" | Where-Object { $_ -notmatch $swapRe }) +
            @(Get-Content "$work\attr_donor.txt" | Where-Object { $_ -match $swapRe })
$expected | Set-Content -Encoding UTF8 "$work\attr_expected.txt"
& python "$src\fix_defaults.py" "$work\src" "$work\attr_expected.txt"
if ($LASTEXITCODE) { throw 'fix_defaults.py failed (Python 3 is required)' }

Step 'compiling'
$proj = "$work\src\xv2corelib.csproj"
$x = [IO.File]::ReadAllText($proj)
$x = $x.Replace('<HintPath>..\YAXLib.dll</HintPath>', "<HintPath>$work\refs\yaxlib.dll</HintPath>").
        Replace('<RootNamespace />', "<RootNamespace />`n    <AssemblySearchPaths>$work\refs;`$(AssemblySearchPaths)</AssemblySearchPaths>`n    <NoWarn>`$(NoWarn);CS0108;CS0114;CS0162;CS0168;CS0169;CS0219;CS0414;CS0649;CS8981</NoWarn>")
[IO.File]::WriteAllText($proj, $x)
$env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
dotnet build $proj -c Release -nologo -v:q | Where-Object { $_ -match ' error ' } | Select-Object -First 30
if ($LASTEXITCODE) { throw 'Xv2CoreLib failed to compile - see the errors above.' }
$built = "$work\src\bin\Release\net47\Xv2CoreLib.dll"

Step 'checking attributes against the originals'
Dump $built "$work\attr_built.txt"
$diff = Compare-Object (Get-Content "$work\attr_expected.txt") (Get-Content "$work\attr_built.txt") |
        Where-Object { $_.InputObject -notmatch 'RefSafetyRulesAttribute' }
if ($diff) {
    $diff | Select-Object -First 20 | ForEach-Object { Write-Host "  $($_.SideIndicator) $($_.InputObject)" }
    throw "The rebuilt Xv2CoreLib differs from the originals in $(@($diff).Count) attribute(s)."
}

Step "writing $OutLib"
if (Test-Path $OutLib) { Remove-Item -Recurse -Force $OutLib }
New-Item -ItemType Directory $OutLib | Out-Null
Copy-Item $built "$OutLib\xv2corelib.dll"
# Xv2CoreLib's own dependencies; the .NET facades (System.*, netstandard) are
# left to the framework, except System.Half which is a real library.
Get-ChildItem "$work\refs" -Filter *.dll |
    Where-Object { $_.Name -notmatch '^(system\.|microsoft\.win32\.|netstandard)' -or $_.Name -eq 'system.half.dll' } |
    Copy-Item -Destination $OutLib
Remove-Item -Recurse -Force $work
Write-Host "Built $OutLib"
