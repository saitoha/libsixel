# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# Compare the installed component with the payload used to build this MSI.
param($Context)
if (-not $Context.InstalledDll) { throw 'No installed WIC component path.' }
$expected = (Get-FileHash -LiteralPath $Context.Payload -Algorithm SHA256).Hash
$actual = (Get-FileHash -LiteralPath $Context.InstalledDll -Algorithm SHA256).Hash
if ($actual -ne $expected) { throw 'Installed WIC DLL differs from the MSI payload.' }
