# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# The exact built MSI must be installed, not merely advertised or registered.
param($Context)
if ($Context.ProductState -ne 5) {
    throw "The built WIC MSI is not installed (state $($Context.ProductState)); run install first."
}
