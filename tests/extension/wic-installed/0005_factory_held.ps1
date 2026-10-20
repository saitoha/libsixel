# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# Observe factory held in the actual MSI component DLL.
param($Context)
Invoke-WicLifetime -Context $Context -Target 'factory' -Phase 'held' -FailAt 0
