# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# The MSI must detect a newer installed version.
param($Context)
$actual = Read-MsiField $Context.Database 'SELECT `ActionProperty` FROM `Upgrade` WHERE `Attributes` = 2'
if ($actual -ne 'WIC_NEWER') {
    throw 'The MSI must detect a newer installed version.'
}
