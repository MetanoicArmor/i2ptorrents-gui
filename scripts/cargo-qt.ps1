Param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$CargoArgs
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot
. (Join-Path $PSScriptRoot "qt-env.ps1")

Initialize-QtCargoEnvironment | Out-Null

if ($null -eq $CargoArgs -or $CargoArgs.Count -eq 0) {
    $CargoArgs = @("run", "--release", "--features", "gui")
}

& cargo @CargoArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
