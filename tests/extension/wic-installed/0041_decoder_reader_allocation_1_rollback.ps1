# SPDX-License-Identifier: MIT
# Policy: docs/programs/wic-codec.md
# Test-plan: docs/testing/companion-programs-coverage.md
# Observe decoder reader allocation 1 rollback in the actual MSI component DLL.
param($Context)
Invoke-WicLifetime -Context $Context -Target 'decoder_reader' -Phase 'allocation' -FailAt 1
