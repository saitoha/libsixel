# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# The component registry view must match the DLL architecture.
param($Context)
$expected = if ($Context.Architecture -eq 'x86') { '0' } else { '256' }
$actual = Read-MsiField $Context.Database 'SELECT `Attributes` FROM `Component` WHERE `Component` = ''Codec'''
if ($actual -ne $expected) {
    throw 'The component registry view must match the DLL architecture.'
}
