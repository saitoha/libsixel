[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")
$wheelDir = Join-Path $repoRoot "python-wheel"
$stageDir = Join-Path $wheelDir ".stage"
$distDir = Join-Path $wheelDir "dist"
$wheelhouseDir = Join-Path $wheelDir "wheelhouse"
$binariesDir = Join-Path $wheelDir "src\libsixel_wheel\.binaries"

if (-not (Test-Path $stageDir)) {
    throw "stage directory not found: $stageDir"
}

if (Test-Path $binariesDir) {
    Get-ChildItem -Path $binariesDir -File | Remove-Item
} else {
    New-Item -ItemType Directory -Path $binariesDir | Out-Null
}

Get-ChildItem -Path (Join-Path $stageDir "bin") -Filter "libsixel*.dll" -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item $_.FullName -Destination $binariesDir -Force
}
Get-ChildItem -Path (Join-Path $stageDir "lib") -Filter "libsixel*.dll" -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item $_.FullName -Destination $binariesDir -Force
}

if (-not (Get-ChildItem -Path $binariesDir -Filter "libsixel*.dll" -ErrorAction SilentlyContinue)) {
    throw "no libsixel DLL found under $stageDir"
}

if (Test-Path $distDir) {
    Remove-Item $distDir -Recurse -Force
}
New-Item -ItemType Directory -Path $distDir | Out-Null

if (Test-Path $wheelhouseDir) {
    Remove-Item $wheelhouseDir -Recurse -Force
}
New-Item -ItemType Directory -Path $wheelhouseDir | Out-Null

python -m build --wheel --outdir $distDir $wheelDir
$wheelPath = Get-ChildItem -Path $distDir -Filter "libsixel_wheel-*.whl" | Select-Object -First 1
if (-not $wheelPath) {
    throw "wheel build did not produce an artifact"
}
Copy-Item $wheelPath.FullName -Destination $wheelhouseDir -Force
