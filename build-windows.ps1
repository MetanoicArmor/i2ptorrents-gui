Param(
    [string]$VenvDir = ".venv"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-NativeChecked {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter()][string[]]$Arguments = @()
    )
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        $argsText = if ($Arguments.Count -gt 0) { " " + ($Arguments -join " ") } else { "" }
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath$argsText"
    }
}

function Stop-I2PTorrentsProcesses {
    Get-Process -Name "I2PTorrents" -ErrorAction SilentlyContinue | ForEach-Object {
        Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Milliseconds 500
}

function Remove-PathWithRetry {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [int]$Attempts = 6
    )
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    $delayMs = 250
    for ($i = 0; $i -lt $Attempts; $i++) {
        try {
            Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction Stop
            return
        }
        catch {
            if ($i -eq $Attempts - 1) {
                throw "Cannot remove '$Path' after $Attempts attempts: $($_.Exception.Message)"
            }
            Start-Sleep -Milliseconds $delayMs
            $delayMs = [Math]::Min(2000, $delayMs + 250)
        }
    }
}

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $RepoRoot

$VersionFile = Join-Path $RepoRoot "VERSION"
if (-not (Test-Path -LiteralPath $VersionFile)) {
    throw "VERSION file not found: $VersionFile"
}
$ReleaseVersion = (Get-Content -LiteralPath $VersionFile -Raw).Trim()
if (-not $ReleaseVersion) {
    throw "VERSION file is empty: $VersionFile"
}

Write-Host "==> Building I2PTorrents $ReleaseVersion for Windows"

$PythonCmd = Join-Path $VenvDir "Scripts\python.exe"
if (-not (Test-Path -LiteralPath $PythonCmd)) {
    $PythonBin = $env:I2PTORRENTS_PYTHON
    if (-not $PythonBin) {
        $fromPath = Get-Command python -ErrorAction SilentlyContinue
        if (-not $fromPath) {
            throw "python not found. Install Python 3.10+ or set I2PTORRENTS_PYTHON."
        }
        $PythonBin = $fromPath.Source
    }
    Write-Host "==> Creating $VenvDir"
    Invoke-NativeChecked $PythonBin @("-m", "venv", $VenvDir)
}

Write-Host "==> Installing build dependencies"
Invoke-NativeChecked $PythonCmd @("-m", "pip", "install", "-U", "pip")
Invoke-NativeChecked $PythonCmd @("-m", "pip", "install", "-e", ".", "pyinstaller", "pillow")

Write-Host "==> Generating icons from image.png"
Invoke-NativeChecked $PythonCmd @("make_icon.py")

Write-Host "==> Building onedir (PyInstaller)"
Stop-I2PTorrentsProcesses
Remove-PathWithRetry -Path "dist\I2PTorrents"
Remove-PathWithRetry -Path "build\I2PTorrents"
Invoke-NativeChecked $PythonCmd @("-m", "PyInstaller", "--clean", "-y", "I2PTorrents.spec")

$ZipFile = "I2PTorrents-windows-x64-v$ReleaseVersion.zip"
if (Test-Path -LiteralPath $ZipFile) {
    Remove-Item -LiteralPath $ZipFile -Force
}
$ZipStage = "dist\I2PTorrents-windows-x64-v$ReleaseVersion"
if (Test-Path -LiteralPath $ZipStage) {
    Remove-PathWithRetry -Path $ZipStage
}
New-Item -ItemType Directory -Path $ZipStage | Out-Null
Copy-Item -Recurse "dist\I2PTorrents" "$ZipStage\I2PTorrents"
Compress-Archive -Path "$ZipStage\*" -DestinationPath $ZipFile -CompressionLevel Optimal
Remove-PathWithRetry -Path $ZipStage

Write-Host ""
Write-Host "GUI binary: dist\I2PTorrents\I2PTorrents.exe"
Write-Host "Packed: $ZipFile"
