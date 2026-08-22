Param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot
. (Join-Path $PSScriptRoot "qt-env.ps1")

$qtBin = Initialize-QtCargoEnvironment -PersistUserPath
Write-Host "Qt bin: $qtBin"
Write-Host "Added Qt to user PATH (new terminals pick it up automatically)."
Write-Host "Wrote .cargo/config.toml.local (CXXFLAGS, QTDIR)."
Write-Host ""
Write-Host "Now run in this terminal:"
Write-Host "  cargo run --release --features gui"
Write-Host "  cargo gui"
Write-Host ""
Write-Host "If cl.exe is missing, use scripts/cargo-qt.ps1 or Developer PowerShell for VS."
