# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# Observe frame released in the actual MSI component DLL.
param($Context)
Invoke-WicLifetime -Context $Context -Target 'frame' -Phase 'released' -FailAt 0
