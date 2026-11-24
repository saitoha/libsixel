#!/bin/sh
# Verify float K-center retains its typed half-open hard-grid representatives.
# Policy: docs/encoder/output/output-colorspace.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0043_kcenter_float_grid_values" || {
    printf '\n%s\n' "not ok 1 - K-center float hard-grid regression"
    exit 0
}

printf '\n%s\n' "ok 1 - K-center float hard-grid regression"
exit 0
