# SPDX-License-Identifier: MIT
# Test-plan: docs/testing/companion-programs-coverage.md
# The MSI file size must match the built DLL.
param($Context)
$actual = Read-MsiField $Context.Database 'SELECT `FileSize` FROM `File` WHERE `File` = ''CodecDll'''
if ($actual -ne $Context.FileSize) {
    throw 'The MSI file size must match the built DLL.'
}
