Param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$AppName = "I2PTorrents"
$CargoBin = "i2ptorrents-gui"

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
    Get-Process -Name "I2PTorrents", "i2ptorrents-gui" -ErrorAction SilentlyContinue | ForEach-Object {
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

function Find-Windeployqt {
    $fromPath = Get-Command windeployqt -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }
    $candidates = @()
    foreach ($root in @($env:QTDIR, $env:Qt6_DIR, $env:QT_ROOT)) {
        if ($root) {
            $candidates += (Join-Path $root "bin\windeployqt.exe")
            $candidates += (Join-Path $root "windeployqt.exe")
        }
    }
    foreach ($qtRoot in @("C:\Qt", "D:\Qt")) {
        if (-not (Test-Path -LiteralPath $qtRoot)) {
            continue
        }
        Get-ChildItem -LiteralPath $qtRoot -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            Get-ChildItem -LiteralPath $_.FullName -Directory -ErrorAction SilentlyContinue | ForEach-Object {
                $candidates += (Join-Path $_.FullName "bin\windeployqt.exe")
            }
        }
    }
    foreach ($path in $candidates) {
        if ($path -and (Test-Path -LiteralPath $path)) {
            return $path
        }
    }
    return $null
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

$ArchSuffix = if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") { "arm64" } else { "x64" }

Write-Host "==> Building $AppName $ReleaseVersion for Windows $ArchSuffix"

Write-Host "==> Generating icons from image.png"
Copy-Item -Force "image.png" "icon.png"
$Magick = Get-Command magick -ErrorAction SilentlyContinue
if ($Magick) {
    Invoke-NativeChecked $Magick.Source @("icon.png", "-define", "icon:auto-resize=256,128,64,48,32,24,16", "I2PTorrents.ico")
}

$Windeploy = Find-Windeployqt
if (-not $Windeploy) {
    throw "windeployqt not found. Install Qt 6 and add its bin directory to PATH."
}
$QtBin = Split-Path -Parent $Windeploy
$env:PATH = "$QtBin;$env:PATH"

Write-Host "==> Building release binary"
Invoke-NativeChecked cargo @("build", "--release", "--features", "gui")
$Bin = Join-Path $RepoRoot "target\release\$CargoBin.exe"
if (-not (Test-Path -LiteralPath $Bin)) {
    throw "missing binary $Bin"
}

$Stage = Join-Path $RepoRoot "dist\$AppName"
Write-Host "==> Staging $Stage"
Stop-I2PTorrentsProcesses
Remove-PathWithRetry -Path $Stage
New-Item -ItemType Directory -Path $Stage | Out-Null
Copy-Item $Bin (Join-Path $Stage "$AppName.exe")
Copy-Item "image.png" (Join-Path $Stage "image.png")
Copy-Item "icon.png" (Join-Path $Stage "icon.png")
if (Test-Path -LiteralPath "I2PTorrents.ico") {
    Copy-Item "I2PTorrents.ico" (Join-Path $Stage "I2PTorrents.ico")
}
Copy-Item "VERSION" (Join-Path $Stage "VERSION")
Copy-Item "AUTHORS" (Join-Path $Stage "AUTHORS")
Copy-Item "LICENSE" (Join-Path $Stage "LICENSE")

Write-Host "==> Bundling Qt with windeployqt"
Invoke-NativeChecked $Windeploy @(
    "--release",
    "--compiler-runtime",
    "--no-translations",
    (Join-Path $Stage "$AppName.exe")
)

$ZipFile = Join-Path $RepoRoot "$AppName-windows-$ArchSuffix-v$ReleaseVersion.zip"
if (Test-Path -LiteralPath $ZipFile) {
    Remove-Item -LiteralPath $ZipFile -Force
}
Compress-Archive -Path $Stage -DestinationPath $ZipFile -CompressionLevel Optimal

Write-Host ""
Write-Host "GUI binary: dist\$AppName\$AppName.exe"
Write-Host "Packed: $ZipFile"
