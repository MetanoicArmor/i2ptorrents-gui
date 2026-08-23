Param(
    [Alias('QtPrefix', 'QtPath')]
    [string]$QtDir,
    [switch]$InstallQt,
    [string]$QtVersion = '6.8.3',
    [string]$QtInstallRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot
. (Join-Path $PSScriptRoot 'qt-env.ps1')

$qtBin = Initialize-QtCargoEnvironment -PersistUserPath -QtPrefix $QtDir -InstallIfMissing:$InstallQt `
    -QtVersion $QtVersion -QtInstallRoot $QtInstallRoot
$qtPrefix = Split-Path -Parent $qtBin
Write-Host "Qt bin: $qtBin"
Write-Host "Qt prefix: $qtPrefix"
Write-Host 'Added Qt to user PATH (new terminals pick it up automatically).'
Write-Host ''
Write-Host 'Now run in this terminal:'
Write-Host "  cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$qtPrefix"
Write-Host '  cmake --build build --config Release'
Write-Host '  .\build\bin\Release\I2PTorrents.exe'
Write-Host ''
Write-Host 'If cl.exe is missing, use Developer PowerShell for VS.'
Write-Host ''
Write-Host 'First-time setup without Qt installed:'
Write-Host '  .\scripts\setup-windows-dev.ps1 -InstallQt'
