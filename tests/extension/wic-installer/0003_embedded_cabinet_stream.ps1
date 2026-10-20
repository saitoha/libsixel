# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# The MSI must contain its payload cabinet stream.
param($Context)
$actual = Read-MsiField $Context.Database 'SELECT `Name` FROM `_Streams` WHERE `Name` = ''payload.cab'''
if ($actual -ne 'payload.cab') {
    throw 'The MSI must contain its payload cabinet stream.'
}
