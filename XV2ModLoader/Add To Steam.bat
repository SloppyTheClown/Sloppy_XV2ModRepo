<# :
@echo off
rem Adds "Launch Modded.exe" and "Launch Unmodded.exe" to Steam as non-Steam
rem games, with the game's own library art.
rem   Add To Steam.bat          add them (or update ones already there)
rem   Add To Steam.bat remove   take them out of Steam again
rem Sits in <game>\XV2ModLoader, next to the launchers. The PowerShell below
rem does the work.
setlocal
set "XV2_SELF=%~f0"
set "XV2_ARG=%~1"
powershell -NoProfile -ExecutionPolicy Bypass -Command "iex ([IO.File]::ReadAllText($env:XV2_SELF))"
set "RC=%ERRORLEVEL%"
if not defined NOPAUSE pause
exit /b %RC%
#>

$ErrorActionPreference = 'Stop'
$remove = $env:XV2_ARG -ieq 'remove'
$here   = Split-Path -Parent $env:XV2_SELF
$game   = $here
if (-not (Test-Path -LiteralPath (Join-Path $game 'bin\DBXV2.exe'))) { $game = Split-Path -Parent $here }
$gameId = 454650
$launchers = @(
    @{ File = 'Launch Modded.exe';   Name = 'DRAGON BALL XENOVERSE 2 (Modded)' },
    @{ File = 'Launch Unmodded.exe'; Name = 'DRAGON BALL XENOVERSE 2 (Unmodded)' }
)
# grid file suffix -> the game's own art: custom grid name, library cache name(s)
$artKinds = @(
    @{ Suffix = 'p';     Custom = "${gameId}p";     Cache = @('library_600x900.jpg') },
    @{ Suffix = '';      Custom = "$gameId";        Cache = @('header.jpg', 'library_header.jpg') },
    @{ Suffix = '_hero'; Custom = "${gameId}_hero"; Cache = @('library_hero.jpg') },
    @{ Suffix = '_logo'; Custom = "${gameId}_logo"; Cache = @('logo.png') }
)
$imageExts = @('.png', '.jpg', '.jpeg')

function Fail($msg) { Write-Host $msg -ForegroundColor Red; exit 1 }

if (-not (Test-Path -LiteralPath (Join-Path $game 'bin\DBXV2.exe'))) {
    Fail 'The game was not found. This file belongs in the XV2ModLoader folder inside the game folder.'
}
foreach ($l in $launchers) {
    $l.Path = Join-Path $here $l.File
    # where older versions put the launchers; shortcuts to there are moved over
    $l.OldPath = Join-Path $game $l.File
    if (-not $remove -and -not (Test-Path -LiteralPath $l.Path)) {
        Fail "$($l.File) was not found next to this file."
    }
}

# Downloaded files carry Windows' "from the internet" mark, and SmartScreen
# silently blocks a marked launcher when Steam starts it. Clear the mark on
# everything in this folder (launchers, this file, mods) and on the loader.
if (-not $remove) {
    $files = @(Get-ChildItem -LiteralPath $here -Recurse -File)
    $loader = Join-Path $game 'bin\winmm.dll'
    if (Test-Path -LiteralPath $loader) { $files += Get-Item -LiteralPath $loader }
    $unblocked = 0
    foreach ($f in $files) {
        if (Get-Item -LiteralPath $f.FullName -Stream Zone.Identifier -ErrorAction SilentlyContinue) {
            try { Unblock-File -LiteralPath $f.FullName; $unblocked++ }
            catch { Write-Host "Could not unblock $($f.Name) - $($_.Exception.Message)" -ForegroundColor Yellow }
        }
    }
    if ($unblocked) { Write-Host "Unblocked $unblocked downloaded file(s)." }
}

# --- Steam and its accounts -------------------------------------------------

$steam = $env:XV2_STEAM
if (-not $steam) {
    try { $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam').SteamPath } catch { }
}
if (-not $steam -or -not (Test-Path -LiteralPath (Join-Path $steam 'userdata'))) { Fail 'Could not find Steam.' }
$steam = [IO.Path]::GetFullPath($steam)
$steamExe = Join-Path $steam 'steam.exe'

# every account that has logged in on this PC
$users = @()
$loginUsers = Join-Path $steam 'config\loginusers.vdf'
if (Test-Path -LiteralPath $loginUsers) {
    $text = [IO.File]::ReadAllText($loginUsers)
    foreach ($m in [regex]::Matches($text, '"(7656\d{13})"\s*\{[^}]*?"AccountName"\s*"([^"]*)"')) {
        $id = [uint64]$m.Groups[1].Value - 76561197960265728
        $dir = Join-Path $steam "userdata\$id"
        if (Test-Path -LiteralPath $dir) { $users += @{ Name = $m.Groups[2].Value; Dir = $dir } }
    }
}
if (-not $users) {
    foreach ($d in Get-ChildItem -LiteralPath (Join-Path $steam 'userdata') -Directory) {
        if ($d.Name -match '^\d+$' -and $d.Name -ne '0') { $users += @{ Name = $d.Name; Dir = $d.FullName } }
    }
}
if (-not $users) { Fail 'No Steam accounts found. Start Steam and log in once first.' }

# --- binary VDF (shortcuts.vdf) ---------------------------------------------
# A map is an ArrayList of @{T = type; K = key; V = value}, in file order, so
# everything Steam wrote comes back out byte for byte.

$utf8 = New-Object System.Text.UTF8Encoding($false)

function ReadCStr {
    $start = $script:pos
    while ($script:buf[$script:pos] -ne 0) { $script:pos++ }
    $s = $utf8.GetString($script:buf, $start, $script:pos - $start)
    $script:pos++
    $s
}
function ReadRaw($n) {
    $r = New-Object byte[] $n
    [Array]::Copy($script:buf, $script:pos, $r, 0, $n)
    $script:pos += $n
    , $r
}
function ReadMap($top) {
    $list = New-Object System.Collections.ArrayList
    while ($true) {
        if ($script:pos -ge $script:buf.Length) {
            if ($top) { return , $list }
            throw 'the file ends early'
        }
        $t = $script:buf[$script:pos]; $script:pos++
        if ($t -eq 8) { return , $list }
        $k = ReadCStr
        switch ($t) {
            0       { $v = ReadMap $false }
            1       { $v = ReadCStr }
            2       { $v = [BitConverter]::ToInt32($script:buf, $script:pos); $script:pos += 4 }
            3       { $v = ReadRaw 4 }
            7       { $v = ReadRaw 8 }
            10      { $v = ReadRaw 8 }
            default { throw "unknown entry type $t" }
        }
        [void]$list.Add(@{ T = [int]$t; K = $k; V = $v })
    }
}
function WriteCStr($ms, $s) {
    $b = $utf8.GetBytes($s)
    $ms.Write($b, 0, $b.Length)
    $ms.WriteByte(0)
}
function WriteMap($ms, $list) {
    foreach ($e in $list) {
        $ms.WriteByte($e.T)
        WriteCStr $ms $e.K
        switch ($e.T) {
            0       { WriteMap $ms $e.V }
            1       { WriteCStr $ms $e.V }
            2       { $b = [BitConverter]::GetBytes([int]$e.V); $ms.Write($b, 0, 4) }
            default { $ms.Write($e.V, 0, $e.V.Length) }
        }
    }
    $ms.WriteByte(8)
}
function Field($map, $key) {
    foreach ($e in $map) { if ($e.K -ieq $key) { return $e } }
    $null
}
function SetField($map, $t, $key, $val) {
    $e = Field $map $key
    if ($e) { $e.V = $val } else { [void]$map.Add(@{ T = $t; K = $key; V = $val }) }
}

# Steam's id for a shortcut: crc32(exe + name) with the top bit set.
function ShortcutId($exe, $name) {
    $c = [long]4294967295
    foreach ($x in $utf8.GetBytes($exe + $name)) {
        $c = $c -bxor $x
        for ($i = 0; $i -lt 8; $i++) {
            if ($c -band 1) { $c = ($c -shr 1) -bxor 3988292384 } else { $c = $c -shr 1 }
        }
    }
    (($c -bxor 4294967295) -bor 2147483648) - 4294967296   # as the signed int32 Steam stores
}

function ExePath($exe) {
    $exe = $exe.Trim()
    if ($exe.StartsWith('"')) { $end = $exe.IndexOf('"', 1); if ($end -gt 0) { $exe = $exe.Substring(1, $end - 1) } }
    try { [IO.Path]::GetFullPath($exe) } catch { $exe }
}

# --- the game's art ---------------------------------------------------------

function FindArt($kind) {
    foreach ($u in $users) {
        foreach ($ext in $imageExts) {
            $f = Join-Path $u.Dir "config\grid\$($kind.Custom)$ext"
            if (Test-Path -LiteralPath $f) { return $f }
        }
    }
    $cache = Join-Path $steam 'appcache\librarycache'
    foreach ($name in $kind.Cache) {
        $f = Join-Path $cache "$gameId\$name"
        if (Test-Path -LiteralPath $f) { return $f }
        $f = Join-Path $cache "${gameId}_$name"
        if (Test-Path -LiteralPath $f) { return $f }
        if (Test-Path -LiteralPath (Join-Path $cache $gameId)) {
            $f = Get-ChildItem -LiteralPath (Join-Path $cache $gameId) -Recurse -File -Filter $name | Select-Object -First 1
            if ($f) { return $f.FullName }
        }
    }
    $null
}

function ClearArt($grid, $uid) {
    foreach ($kind in $artKinds) {
        foreach ($ext in $imageExts) {
            $f = Join-Path $grid "$uid$($kind.Suffix)$ext"
            if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f }
        }
    }
}

$art = @{}
if (-not $remove) {
    foreach ($kind in $artKinds) {
        $f = FindArt $kind
        if ($f) { $art[$kind.Suffix] = $f }
    }
    if (-not $art.Count) {
        Write-Host 'No library art for the game was found - open its page in Steam once so Steam caches it.' -ForegroundColor Yellow
    }
}

# --- Steam must be closed: it rewrites shortcuts.vdf when it exits ----------

$restart = $false
$running = @(Get-Process steam -ErrorAction SilentlyContinue | Where-Object {
    -not $_.Path -or ([IO.Path]::GetFullPath($_.Path) -ieq $steamExe) })
if ($running) {
    Write-Host 'Steam has to be closed while its shortcuts are changed. It is started again afterwards.'
    $answer = Read-Host 'Close Steam now? [Y/N]'
    if ($answer -notmatch '^\s*[Yy]') { Fail 'Nothing was changed.' }
    Start-Process -FilePath $steamExe -ArgumentList '-shutdown'
    $deadline = (Get-Date).AddSeconds(60)
    while (Get-Process -Id $running.Id -ErrorAction SilentlyContinue) {
        if ((Get-Date) -gt $deadline) { Fail 'Steam did not close within a minute. Close it yourself and run this again.' }
        Start-Sleep -Milliseconds 500
    }
    $restart = $true
}

# --- per account --------------------------------------------------------------

$failed = $false
foreach ($u in $users) {
    $config = Join-Path $u.Dir 'config'
    $file   = Join-Path $config 'shortcuts.vdf'
    $grid   = Join-Path $config 'grid'
    try {
        if (Test-Path -LiteralPath $file) {
            $script:buf = [IO.File]::ReadAllBytes($file); $script:pos = 0
            $root = ReadMap $true
        } else {
            $root = New-Object System.Collections.ArrayList
        }
        $shortcuts = Field $root 'shortcuts'
        if (-not $shortcuts) {
            $shortcuts = @{ T = 0; K = 'shortcuts'; V = (New-Object System.Collections.ArrayList) }
            [void]$root.Add($shortcuts)
        }
        $list = $shortcuts.V
        $done = @()

        foreach ($l in $launchers) {
            $entry = $null
            foreach ($e in @($list)) {
                if ($e.T -ne 0) { continue }
                $exe = Field $e.V 'exe'
                if ($exe -and ((ExePath $exe.V) -ieq $l.Path -or (ExePath $exe.V) -ieq $l.OldPath)) {
                    if ($remove) {
                        $id = Field $e.V 'appid'
                        if ($id -and (Test-Path -LiteralPath $grid)) { ClearArt $grid ([uint32]([long]$id.V -band 4294967295)) }
                        $list.Remove($e)
                        $done += "removed $((Field $e.V 'appname').V)"
                    } elseif (-not $entry) {
                        $entry = $e.V
                    }
                }
            }
            if ($remove) { continue }

            $exe = '"' + $l.Path + '"'
            if ($entry) {
                $what = 'updated'
                SetField $entry 1 'exe' $exe
                SetField $entry 1 'StartDir' ($here + '\')
            } else {
                $what = 'added'
                $entry = New-Object System.Collections.ArrayList
                $fields = @(
                    @(2, 'appid', (ShortcutId $exe $l.Name)), @(1, 'appname', $l.Name), @(1, 'exe', $exe),
                    @(1, 'StartDir', ($here + '\')),@(1, 'icon', ''), @(1, 'ShortcutPath', ''),
                    @(1, 'LaunchOptions', ''), @(2, 'IsHidden', 0), @(2, 'AllowDesktopConfig', 1),
                    @(2, 'AllowOverlay', 1), @(2, 'OpenVR', 0), @(2, 'Devkit', 0), @(1, 'DevkitGameID', ''),
                    @(2, 'DevkitOverrideAppID', 0), @(2, 'LastPlayTime', 0), @(1, 'FlatpakAppID', ''),
                    @(1, 'sortas', ''), @(0, 'tags', (New-Object System.Collections.ArrayList))
                )
                foreach ($f in $fields) { [void]$entry.Add(@{ T = $f[0]; K = $f[1]; V = $f[2] }) }
                [void]$list.Add(@{ T = 0; K = ''; V = $entry })
            }
            $name = (Field $entry 'appname').V
            if (-not (Field $entry 'appid')) { $entry.Insert(0, @{ T = 2; K = 'appid'; V = (ShortcutId $exe $name) }) }
            $icon = Field $entry 'icon'
            if (-not $icon -or -not $icon.V) { SetField $entry 1 'icon' (Join-Path $game 'bin\DBXV2.exe') }

            $uid = [uint32]([long](Field $entry 'appid').V -band 4294967295)
            if ($art.Count) {
                if (-not (Test-Path -LiteralPath $grid)) { New-Item -ItemType Directory -Path $grid | Out-Null }
                ClearArt $grid $uid
                foreach ($suffix in $art.Keys) {
                    $src = $art[$suffix]
                    Copy-Item -LiteralPath $src -Destination (Join-Path $grid "$uid$suffix$([IO.Path]::GetExtension($src).ToLower())")
                }
            }
            $done += "$what $name"
        }

        # entries are keyed 0, 1, 2 ...
        for ($i = 0; $i -lt $list.Count; $i++) { $list[$i].K = [string]$i }

        $ms = New-Object System.IO.MemoryStream
        WriteMap $ms $root
        if (-not (Test-Path -LiteralPath $config)) { New-Item -ItemType Directory -Path $config | Out-Null }
        if (Test-Path -LiteralPath $file) { Copy-Item -LiteralPath $file -Destination "$file.bak" -Force }
        [IO.File]::WriteAllBytes($file, $ms.ToArray())

        if (-not $done) { $done = @('nothing to remove') }
        Write-Host "$($u.Name): $($done -join ', ')"
    } catch {
        Write-Host "$($u.Name): FAILED, left unchanged - $($_.Exception.Message)" -ForegroundColor Red
        $failed = $true
    }
}

if ($restart) { Start-Process -FilePath $steamExe }
if ($failed) { exit 1 }
if (-not $remove) { Write-Host "Done. Library art: $($art.Count) of $($artKinds.Count) images found." }
exit 0
