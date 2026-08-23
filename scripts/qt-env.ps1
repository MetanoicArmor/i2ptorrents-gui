Set-StrictMode -Version Latest

function Find-QtBinDirectory {
    $fromPath = Get-Command qmake -ErrorAction SilentlyContinue
    if ($fromPath) {
        return Split-Path -Parent $fromPath.Source
    }
    $candidates = @()
    foreach ($root in @($env:QTDIR, $env:Qt6_DIR, $env:QT_ROOT)) {
        if ($root) {
            $candidates += (Join-Path $root "bin\qmake.exe")
        }
    }
    foreach ($qtRoot in @("C:\Qt", "D:\Qt")) {
        if (-not (Test-Path -LiteralPath $qtRoot)) {
            continue
        }
        Get-ChildItem -LiteralPath $qtRoot -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            Get-ChildItem -LiteralPath $_.FullName -Directory -ErrorAction SilentlyContinue | ForEach-Object {
                $candidates += (Join-Path $_.FullName "bin\qmake.exe")
            }
        }
    }
    $ranked = foreach ($path in $candidates) {
        if (-not (Test-Path -LiteralPath $path)) {
            continue
        }
        $lower = $path.ToLowerInvariant()
        $rank = if ($lower -match 'msvc2022_64') { 0 }
                elseif ($lower -match 'msvc2019_64') { 1 }
                elseif ($lower -match 'msvc') { 2 }
                elseif ($lower -match 'mingw') { 3 }
                else { 4 }
        [PSCustomObject]@{ Rank = $rank; Path = (Split-Path -Parent $path) }
    }
    $best = $ranked | Sort-Object Rank | Select-Object -First 1
    if ($best) {
        return $best.Path
    }
    return $null
}

function Ensure-MsvcDevEnvironment {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        return
    }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "cl.exe not found. Install Visual Studio Build Tools with the C++ workload."
    }
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) {
        throw "Visual Studio C++ build tools not found. Install the Desktop development with C++ workload."
    }
    Import-Module (Join-Path $vsPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
    Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments "-arch=amd64" | Out-Null
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
        [switch]$PersistUserPath
    )
    $qtBin = Find-QtBinDirectory
    if (-not $qtBin) {
        throw "Qt 6 not found. Install Qt 6 or set QTDIR to your kit prefix (for example C:\Qt\6.8.3\msvc2022_64)."
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
