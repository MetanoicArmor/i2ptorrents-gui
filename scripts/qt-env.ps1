Set-StrictMode -Version Latest

$script:DefaultQtVersion = '6.8.3'
$script:DefaultQtArch = 'win64_msvc2022_64'
$script:DefaultQtKitDir = 'msvc2022_64'

function Resolve-QtBinFromPrefix {
    param(
        [Parameter(Mandatory = $true)][string]$QtPrefix
    )
    $normalized = $QtPrefix.Trim().TrimEnd('\', '/')
    if (-not $normalized) {
        throw 'Qt prefix path is empty.'
    }
    $asBin = Join-Path $normalized 'bin\qmake.exe'
    if (Test-Path -LiteralPath $asBin) {
        return Join-Path $normalized 'bin'
    }
    $asQmake = Join-Path $normalized 'qmake.exe'
    if (Test-Path -LiteralPath $asQmake) {
        return $normalized
    }
    throw "No qmake.exe under '$QtPrefix' (expected bin\qmake.exe)."
}

function Get-QtBinRank {
    param(
        [Parameter(Mandatory = $true)][string]$QtBin
    )
    $lower = $QtBin.ToLowerInvariant()
    if ($lower -match 'msvc2022_64') { return 0 }
    if ($lower -match 'msvc2019_64') { return 1 }
    if ($lower -match 'msvc') { return 2 }
    if ($lower -match 'mingw') { return 3 }
    return 4
}

function Find-QtBinDirectory {
    param(
        [string]$QtPrefix
    )
    if ($QtPrefix) {
        return Resolve-QtBinFromPrefix -QtPrefix $QtPrefix
    }
    $candidates = @()
    $fromPath = Get-Command qmake -ErrorAction SilentlyContinue
    if ($fromPath) {
        $candidates += (Split-Path -Parent $fromPath.Source)
    }
    foreach ($root in @($env:QTDIR, $env:Qt6_DIR, $env:QT_ROOT)) {
        if ($root) {
            try {
                $candidates += Resolve-QtBinFromPrefix -QtPrefix $root
            }
            catch {
            }
        }
    }
    foreach ($qtRoot in @('C:\Qt', 'D:\Qt', (Join-Path $env:LOCALAPPDATA 'Qt'))) {
        if (-not (Test-Path -LiteralPath $qtRoot)) {
            continue
        }
        Get-ChildItem -LiteralPath $qtRoot -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            Get-ChildItem -LiteralPath $_.FullName -Directory -ErrorAction SilentlyContinue | ForEach-Object {
                $candidates += (Join-Path $_.FullName 'bin\qmake.exe')
            }
        }
    }
    $ranked = foreach ($path in ($candidates | Select-Object -Unique)) {
        $bin = $null
        if ($path -like '*\qmake.exe') {
            $bin = Split-Path -Parent $path
        }
        elseif (Test-Path -LiteralPath (Join-Path $path 'qmake.exe')) {
            $bin = $path
        }
        elseif (Test-Path -LiteralPath (Join-Path $path 'bin\qmake.exe')) {
            $bin = Join-Path $path 'bin'
        }
        if (-not $bin) {
            continue
        }
        [PSCustomObject]@{
            Rank = Get-QtBinRank -QtBin $bin
            Path = $bin
        }
    }
    return ($ranked | Sort-Object Rank | Select-Object -First 1 -ExpandProperty Path)
}

function Get-PythonCommand {
    foreach ($candidate in @('python', 'py')) {
        $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($cmd) {
            return $cmd.Source
        }
    }
    return $null
}

function Resolve-QtInstallRoot {
    param(
        [string]$InstallRoot
    )
    if ($InstallRoot) {
        return $InstallRoot.TrimEnd('\', '/')
    }
    $localQt = Join-Path $env:LOCALAPPDATA 'Qt'
    foreach ($candidate in @('C:\Qt', $localQt)) {
        if ($candidate -eq 'C:\Qt') {
            if (Test-Path -LiteralPath 'C:\') {
                return $candidate
            }
            continue
        }
        return $candidate
    }
    return $localQt
}

function Install-QtWithAqt {
    param(
        [string]$Version = $script:DefaultQtVersion,
        [string]$Arch = $script:DefaultQtArch,
        [string]$InstallRoot
    )
    $python = Get-PythonCommand
    if (-not $python) {
        throw @'
Python 3 not found. Install Python 3, or install Qt yourself and rerun with:
  .\scripts\setup-windows-dev.ps1 -QtDir 'C:\Qt\6.8.3\msvc2022_64'
'@
    }
    $root = Resolve-QtInstallRoot -InstallRoot $InstallRoot
    New-Item -ItemType Directory -Path $root -Force | Out-Null
    Write-Host "==> Installing Qt $Version ($Arch) under $root via aqtinstall"
    & $python -m pip install --upgrade aqtinstall
    if ($LASTEXITCODE -ne 0) {
        throw "pip install aqtinstall failed with exit code $LASTEXITCODE"
    }
    & $python -m aqt install-qt windows desktop $Version $Arch -O $root -m qtbase qttools
    if ($LASTEXITCODE -ne 0) {
        throw "aqt install-qt failed with exit code $LASTEXITCODE"
    }
    $prefix = Join-Path $root "$Version\$($script:DefaultQtKitDir)"
    if (-not (Test-Path -LiteralPath (Join-Path $prefix 'bin\qmake.exe'))) {
        throw "Qt install finished but qmake.exe is missing under $prefix"
    }
    return $prefix
}

function Ensure-MsvcDevEnvironment {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        return
    }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'cl.exe not found. Install Visual Studio Build Tools with the C++ workload.'
    }
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) {
        throw 'Visual Studio C++ build tools not found. Install the Desktop development with C++ workload.'
    }
    Import-Module (Join-Path $vsPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
    Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=amd64' | Out-Null
}

function Add-QtToUserPath {
    param(
        [Parameter(Mandatory = $true)][string]$QtBin
    )
    $userPath = [Environment]::GetEnvironmentVariable('PATH', 'User')
    if ($null -eq $userPath) {
        $userPath = ''
    }
    $parts = $userPath -split ';' | Where-Object { $_ -and ($_ -ne $QtBin) }
    $updated = @($QtBin) + $parts
    $joined = ($updated -join ';').TrimEnd(';')
    [Environment]::SetEnvironmentVariable('PATH', $joined, 'User')
    if ($env:PATH -notlike "*$QtBin*") {
        $env:PATH = "$QtBin;$env:PATH"
    }
}

function Initialize-QtCargoEnvironment {
    param(
        [switch]$PersistUserPath,
        [string]$QtPrefix,
        [switch]$InstallIfMissing,
        [string]$QtVersion = $script:DefaultQtVersion,
        [string]$QtInstallRoot,
        [switch]$AllowNonMsvcKit
    )
    $qtBin = Find-QtBinDirectory -QtPrefix $QtPrefix
    if (-not $qtBin -and $InstallIfMissing) {
        $installedPrefix = Install-QtWithAqt -Version $QtVersion -InstallRoot $QtInstallRoot
        $qtBin = Find-QtBinDirectory -QtPrefix $installedPrefix
    }
    if (-not $qtBin) {
        throw @"
Qt 6 not found.

Options:
  1) Auto-install (needs Python 3 + network):
       .\scripts\setup-windows-dev.ps1 -InstallQt
  2) Point to an existing kit prefix:
       .\scripts\setup-windows-dev.ps1 -QtDir 'C:\Qt\6.8.3\msvc2022_64'
  3) Set QTDIR for this terminal, then rerun setup:
       `$env:QTDIR = 'C:\Qt\6.8.3\msvc2022_64'
"@
    }
    $rank = Get-QtBinRank -QtBin $qtBin
    if (-not $AllowNonMsvcKit -and $rank -ge 3) {
        $kit = Split-Path -Parent $qtBin
        throw @"
Found Qt at '$kit', but this project expects an MSVC 64-bit kit (for example msvc2022_64).

Install the MSVC kit with:
  .\scripts\setup-windows-dev.ps1 -InstallQt

Or point to the correct kit:
  .\scripts\setup-windows-dev.ps1 -QtDir 'C:\Qt\6.8.3\msvc2022_64'
"@
    }
    if ($rank -eq 2 -and -not $AllowNonMsvcKit) {
        Write-Warning "Using Qt kit '$qtBin'. Prefer msvc2022_64 when available."
    }
    $env:PATH = "$qtBin;$env:PATH"
    $prefix = Split-Path -Parent $qtBin
    $env:QTDIR = $prefix
    if ($qtBin -match '\\msvc') {
        Ensure-MsvcDevEnvironment
    }
    if ($PersistUserPath) {
        Add-QtToUserPath -QtBin $qtBin
    }
    return $qtBin
}
