# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# Observe frame held in the actual MSI component DLL.
param($Context)
Invoke-WicLifetime -Context $Context -Target 'frame' -Phase 'held' -FailAt 0
