# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# COM registration must point at the MSI-owned DLL.
param($Context)
$actual = Read-MsiField $Context.Database 'SELECT `Value` FROM `Registry` WHERE `Key` = ''CLSID\{15B9B4DA-B155-4977-8571-CF005884BCB9}\InprocServer32'' AND `Name` IS NULL'
if ($actual -ne '[#CodecDll]') {
    throw 'COM registration must point at the MSI-owned DLL.'
}
