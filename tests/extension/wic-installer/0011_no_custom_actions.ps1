# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# The MSI must not execute custom actions.
param($Context)
$actual = Read-MsiField $Context.Database 'SELECT `Name` FROM `_Tables` WHERE `Name` = ''CustomAction'''
if ($actual -ne $null) {
    throw 'The MSI must not execute custom actions.'
}
