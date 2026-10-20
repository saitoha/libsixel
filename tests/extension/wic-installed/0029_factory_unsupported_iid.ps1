# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# Observe factory unsupported iid in the actual MSI component DLL.
param($Context)
Invoke-WicLifetime -Context $Context -Target 'factory' -Phase 'unsupported' -FailAt 0
