# Download Inter static OTF files (SIL Open Font License) into assets/fonts.
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Dest = Join-Path $RepoRoot "assets\fonts"
$InterVersion = "4.1"
$Archive = Join-Path $Dest ".inter-$InterVersion.zip"
$Fonts = @(
    "Inter-Regular.otf",
    "Inter-Medium.otf",
    "Inter-SemiBold.otf",
    "Inter-Bold.otf"
)

New-Item -ItemType Directory -Path $Dest -Force | Out-Null

$missing = $Fonts | Where-Object { -not (Test-Path -LiteralPath (Join-Path $Dest $_)) }
if (-not $missing) {
    Write-Host "==> Inter UI fonts already present in $Dest"
    exit 0
}

Write-Host "==> Downloading Inter $InterVersion"
$Url = "https://github.com/rsms/inter/releases/download/v$InterVersion/Inter-$InterVersion.zip"
Invoke-WebRequest -Uri $Url -OutFile $Archive -UseBasicParsing

$Tmp = Join-Path ([IO.Path]::GetTempPath()) ("inter-" + [Guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $Tmp -Force | Out-Null
try {
    Expand-Archive -LiteralPath $Archive -DestinationPath $Tmp -Force
    foreach ($font in $Fonts) {
        $match = Get-ChildItem -LiteralPath $Tmp -Filter $font -Recurse -File -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if (-not $match) {
            throw "missing $font in Inter archive"
        }
        Copy-Item -LiteralPath $match.FullName -Destination (Join-Path $Dest $font) -Force
        Write-Host "    $font"
    }
    foreach ($licenseName in @("LICENSE.txt", "OFL.txt")) {
        $license = Get-ChildItem -LiteralPath $Tmp -Filter $licenseName -Recurse -File -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($license) {
            Copy-Item -LiteralPath $license.FullName -Destination (Join-Path $Dest "Inter-OFL.txt") -Force
            break
        }
    }
}
finally {
    Remove-Item -LiteralPath $Tmp -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "Inter UI fonts ready (SIL Open Font License)"
