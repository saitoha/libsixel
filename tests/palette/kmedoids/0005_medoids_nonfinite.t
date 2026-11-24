#!/bin/sh
# Invalid float coordinates fail without a lossy retry.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/medoids_nonfinite" || {
    printf '\n%s\n' "not ok 1 - Invalid float coordinates fail without a lossy retry"
    exit 0
}
printf '\n%s\n' "ok 1 - Invalid float coordinates fail without a lossy retry"
exit 0
