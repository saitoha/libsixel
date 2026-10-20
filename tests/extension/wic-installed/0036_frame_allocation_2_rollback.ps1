# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# Observe frame allocation 2 rollback in the actual MSI component DLL.
param($Context)
Invoke-WicLifetime -Context $Context -Target 'frame' -Phase 'allocation' -FailAt 2
