#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify PSD missing-composite reconstruction with an exact RGB digest.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0096_loader_builtin_psd_missing_composite_rgb8_digest" || {
    echo "not ok 1 - PSD missing-composite exact decoded RGB digest"
    exit 0
}

echo "ok 1 - PSD missing-composite exact decoded RGB digest"
exit 0
