#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify range nominal boundaries.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0026_range_nominal_boundaries" || {
    echo "not ok 1 - range nominal boundaries"
    exit 0
}
echo "ok 1 - range nominal boundaries"
exit 0
