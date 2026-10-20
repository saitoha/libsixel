# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# Observe writer held in the actual MSI component DLL.
param($Context)
Invoke-WicLifetime -Context $Context -Target 'writer' -Phase 'held' -FailAt 0
