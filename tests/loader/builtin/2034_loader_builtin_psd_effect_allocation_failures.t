#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify PSD layer-effect allocation failures leave no live allocation.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0168_loader_builtin_psd_effect_allocation_failures" 1>&2 || {
    echo "not ok" 1 - "PSD effect allocation failure cleanup"
    exit 0
}
echo "ok" 1 - "PSD effect allocation failure cleanup"
exit 0
