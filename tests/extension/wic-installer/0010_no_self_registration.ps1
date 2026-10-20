# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# The MSI must not use DLL self-registration.
param($Context)
$actual = Read-MsiField $Context.Database 'SELECT `Name` FROM `_Tables` WHERE `Name` = ''SelfReg'''
if ($actual -ne $null) {
    throw 'The MSI must not use DLL self-registration.'
}
