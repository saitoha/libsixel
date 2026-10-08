#!/bin/sh
# Compatibility projection rounding cannot replace K-center float values.
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/retention_kcenter_boundary" || {
    echo "not ok 1 - preserve K-center floats at byte rounding boundaries"
    exit 0
}
echo "ok 1 - preserve K-center floats at byte rounding boundaries"
exit 0
