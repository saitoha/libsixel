# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# The WIC package must be per-machine.
param($Context)
$actual = Read-MsiField $Context.Database 'SELECT `Value` FROM `Property` WHERE `Property` = ''ALLUSERS'''
if ($actual -ne '1') {
    throw 'The WIC package must be per-machine.'
}
