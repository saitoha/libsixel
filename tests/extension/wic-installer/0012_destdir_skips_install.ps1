# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# DESTDIR must skip installation before looking up the MSI or running msiexec.
param($Context)
$oldDestdir = $env:DESTDIR
try {
    $env:DESTDIR = 'wic-staging-test'
    Install-WicMsi 'nonexistent-package.msi' 6>$null
} finally { $env:DESTDIR = $oldDestdir }
