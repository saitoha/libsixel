#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify APNG SOURCE and OVER composition stays exact.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0138_loader_builtin_apng_blend_over_digest" 1>&2 || {
    echo "not ok" 1 - "APNG SOURCE and OVER composition stays exact"
    exit 0
}

echo "ok" 1 - "APNG SOURCE and OVER composition stays exact"

exit 0
