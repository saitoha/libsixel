# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# Observe transform released in the actual MSI component DLL.
param($Context)
Invoke-WicLifetime -Context $Context -Target 'transform' -Phase 'released' -FailAt 0
