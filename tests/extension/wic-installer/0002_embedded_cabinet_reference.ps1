# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# The MSI must reference its embedded cabinet.
param($Context)
$actual = Read-MsiField $Context.Database 'SELECT `Cabinet` FROM `Media`'
if ($actual -ne '#payload.cab') {
    throw 'The MSI must reference its embedded cabinet.'
}
