# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# Same-version rebuilds must replace the previous package.
param($Context)
$actual = Read-MsiField $Context.Database 'SELECT `ActionProperty` FROM `Upgrade` WHERE `Attributes` = 512'
if ($actual -ne 'WIC_OLDER') {
    throw 'Same-version rebuilds must replace the previous package.'
}
