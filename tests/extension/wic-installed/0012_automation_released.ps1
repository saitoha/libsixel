# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# Observe automation released in the actual MSI component DLL.
param($Context)
Invoke-WicLifetime -Context $Context -Target 'automation' -Phase 'released' -FailAt 0
