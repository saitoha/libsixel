# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# Upgrade removal must be inside the installation transaction.
param($Context)
$actual = Read-MsiField $Context.Database 'SELECT `Sequence` FROM `InstallExecuteSequence` WHERE `Action` = ''RemoveExistingProducts'''
if ($actual -ne '1510') {
    throw 'Upgrade removal must be inside the installation transaction.'
}
