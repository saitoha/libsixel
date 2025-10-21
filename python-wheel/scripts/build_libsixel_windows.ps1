[CmdletBinding()]
param(
    [string]$Triplet = $(if ($env:LIBSIXEL_VCPKG_TRIPLET) { $env:LIBSIXEL_VCPKG_TRIPLET } else { "x64-windows" })
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")
$stageDir = Join-Path $repoRoot "python-wheel\.stage"
$buildDir = Join-Path $repoRoot "python-wheel\.build-windows"
$binariesDir = Join-Path $repoRoot "python-wheel\src\libsixel_wheel\.binaries"

if (-not $env:VCPKG_ROOT) {
    throw "VCPKG_ROOT environment variable is not set."
}

$vcpkgExe = Join-Path $env:VCPKG_ROOT "vcpkg.exe"
if (-not (Test-Path $vcpkgExe)) {
    throw "vcpkg executable not found at $vcpkgExe"
}

& $vcpkgExe install libsixel --triplet $Triplet --recurse

$installedDir = Join-Path $env:VCPKG_ROOT "installed\$Triplet"
if (-not (Test-Path $installedDir)) {
    throw "vcpkg installed directory not found: $installedDir"
}

if (Test-Path $stageDir) {
    Remove-Item $stageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $stageDir | Out-Null

if (Test-Path $buildDir) {
    Remove-Item $buildDir -Recurse -Force
}
New-Item -ItemType Directory -Path $buildDir | Out-Null

Copy-Item -Recurse -Force (Join-Path $installedDir "bin") -Destination $stageDir
Copy-Item -Recurse -Force (Join-Path $installedDir "lib") -Destination $stageDir
Copy-Item -Recurse -Force (Join-Path $installedDir "include") -Destination $stageDir

if (-not (Test-Path $binariesDir)) {
    New-Item -ItemType Directory -Path $binariesDir | Out-Null
}

Get-ChildItem -Path (Join-Path $installedDir "bin") -Filter "libsixel*.dll" | ForEach-Object {
    Copy-Item $_.FullName -Destination $binariesDir -Force
}
