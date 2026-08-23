Param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot
. (Join-Path $PSScriptRoot "qt-env.ps1")

$qtBin = Initialize-QtCargoEnvironment -PersistUserPath
$qtPrefix = Split-Path -Parent $qtBin
Write-Host "Qt bin: $qtBin"
Write-Host "Added Qt to user PATH (new terminals pick it up automatically)."
Write-Host ""
Write-Host "Now run in this terminal:"
Write-Host "  cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$qtPrefix"
Write-Host "  cmake --build build --config Release"
Write-Host "  .\build\bin\I2PTorrents.exe"
Write-Host ""
Write-Host "If cl.exe is missing, use Developer PowerShell for VS."
