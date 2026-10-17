Param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$AppName = "I2PTorrents"

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

function Copy-MsvcCrt {
    param(
        [Parameter(Mandatory = $true)][string]$Destination,
        [Parameter(Mandatory = $true)][string]$Arch
    )
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "vswhere.exe not found; cannot locate the MSVC CRT"
    }
    $installPath = (& $vswhere -latest -products * -property installationPath | Select-Object -First 1)
    if (-not $installPath) {
        throw "Visual Studio installation not found"
    }
    $installPath = $installPath.Trim()
    $redistRoot = Join-Path $installPath "VC\Redist\MSVC"
    $versionDir = Get-ChildItem -LiteralPath $redistRoot -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        Select-Object -First 1
    if (-not $versionDir) {
        throw "MSVC redist not found under $redistRoot"
    }
    $archDir = Join-Path $versionDir.FullName $Arch
    $crt = Get-ChildItem -LiteralPath $archDir -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "Microsoft.VC*.CRT" -and $_.Name -notlike "*Debug*" } |
        Sort-Object Name -Descending |
        Select-Object -First 1
    if (-not $crt) {
        throw "MSVC CRT directory not found in $archDir"
    }
    Write-Host "==> Copying MSVC CRT from $($crt.FullName)"
    $names = @(
        "vcruntime140.dll",
        "vcruntime140_1.dll",
        "msvcp140.dll",
        "msvcp140_1.dll",
        "msvcp140_2.dll",
        "concrt140.dll",
        "vccorlib140.dll"
    )
    $required = @("vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll")
    foreach ($name in $names) {
        $src = Join-Path $crt.FullName $name
        if (Test-Path -LiteralPath $src) {
            Copy-Item -LiteralPath $src -Destination (Join-Path $Destination $name) -Force
        }
        elseif ($required -contains $name) {
            throw "Required CRT DLL missing: $src"
        }
    }
    Get-ChildItem -LiteralPath $Destination -Filter "vc_redist.*.exe" -File -ErrorAction SilentlyContinue |
        Remove-Item -Force
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

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $RepoRoot
. (Join-Path $RepoRoot "scripts\qt-env.ps1")

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
    try {
        & $Magick.Source @("icon.png", "-define", "icon:auto-resize=256,128,64,48,32,24,16", "I2PTorrents.ico")
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "ImageMagick failed to build I2PTorrents.ico (exit $LASTEXITCODE); continuing without .ico"
        }
    } catch {
        Write-Warning "ImageMagick failed to build I2PTorrents.ico: $_; continuing without .ico"
    }
}

$SyncFonts = Join-Path $RepoRoot "scripts\sync-inter-fonts.ps1"
if (Test-Path -LiteralPath $SyncFonts) {
    Write-Host "==> Syncing Inter UI fonts"
    & $SyncFonts
}

$QtBin = Initialize-QtCargoEnvironment
$QtPrefix = Split-Path -Parent $QtBin
$Windeploy = Join-Path $QtBin "windeployqt.exe"
if (-not (Test-Path -LiteralPath $Windeploy)) {
    throw "windeployqt not found in $QtBin. Install Qt 6 with Qt Tools."
}

Write-Host "==> Building release binary"
$BuildDir = Join-Path $RepoRoot "build"
if (Test-Path -LiteralPath $BuildDir) {
    Remove-PathWithRetry -Path $BuildDir
}
Invoke-NativeChecked cmake @("-S", $RepoRoot, "-B", $BuildDir, "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_PREFIX_PATH=$QtPrefix")
Invoke-NativeChecked cmake @("--build", $BuildDir, "--config", "Release")
$BinCandidates = @(
    (Join-Path $BuildDir "bin\Release\$AppName.exe"),
    (Join-Path $BuildDir "bin\$AppName.exe"),
    (Join-Path $BuildDir "Release\$AppName.exe")
)
$Bin = $BinCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $Bin) {
    throw "missing binary (checked: $($BinCandidates -join ', '))"
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
$FontsSrc = Join-Path $RepoRoot "assets\fonts"
if (Test-Path -LiteralPath $FontsSrc) {
    $FontsDest = Join-Path $Stage "fonts"
    New-Item -ItemType Directory -Path $FontsDest -Force | Out-Null
    Get-ChildItem -LiteralPath $FontsSrc -Filter "Inter-*.otf" -File -ErrorAction SilentlyContinue |
        Copy-Item -Destination $FontsDest -Force
    Get-ChildItem -LiteralPath $FontsSrc -Filter "Inter-*.ttf" -File -ErrorAction SilentlyContinue |
        Copy-Item -Destination $FontsDest -Force
    $Ofl = Join-Path $FontsSrc "Inter-OFL.txt"
    if (Test-Path -LiteralPath $Ofl) {
        Copy-Item -LiteralPath $Ofl -Destination $FontsDest -Force
    }
}

Write-Host "==> Bundling Qt with windeployqt"
Invoke-NativeChecked $Windeploy @(
    "--release",
    "--compiler-runtime",
    "--no-translations",
    (Join-Path $Stage "$AppName.exe")
)
# windeployqt ships vc_redist.*.exe instead of the CRT DLLs. winget and a
# plain zip launch the exe without running that installer, so Windows exits
# with STATUS_DLL_NOT_FOUND (0xC0000135) looking for vcruntime140.dll.
Copy-MsvcCrt -Destination $Stage -Arch $ArchSuffix

$ZipFile = Join-Path $RepoRoot "$AppName-windows-$ArchSuffix-v$ReleaseVersion.zip"
if (Test-Path -LiteralPath $ZipFile) {
    Remove-Item -LiteralPath $ZipFile -Force
}
Compress-Archive -Path $Stage -DestinationPath $ZipFile -CompressionLevel Optimal

Write-Host ""
Write-Host "GUI binary: dist\$AppName\$AppName.exe"
Write-Host "Packed: $ZipFile"
