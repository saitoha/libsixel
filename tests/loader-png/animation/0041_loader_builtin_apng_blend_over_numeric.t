#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png/README.md
# Verify APNG SOURCE and OVER composition matches independent linear values.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0138_loader_builtin_apng_blend_over_numeric" 1>&2 || {
    echo "not ok" 1 - "APNG SOURCE and OVER composition matches independent linear values"
    exit 0
}

echo "ok" 1 - "APNG SOURCE and OVER composition matches independent linear values"

exit 0
