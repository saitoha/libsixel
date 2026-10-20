# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# Decode a literal SIXEL pixel through Windows' installed codec discovery.
param($Context)
if (-not $Context.InstalledDll) { throw 'No installed WIC component path.' }
$savedPath = $env:LIBSIXEL_WIC_INSTALLED_DLL
try {
    $env:LIBSIXEL_WIC_INSTALLED_DLL = $Context.InstalledDll
    & $Context.Probe | ForEach-Object { Write-Output "# $_" }
    if ($LASTEXITCODE -ne 0) { throw 'Installed WIC decoder did not return the expected red pixel.' }
} finally { $env:LIBSIXEL_WIC_INSTALLED_DLL = $savedPath }
