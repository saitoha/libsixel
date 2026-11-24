#!/bin/sh
# K-center exact bins distinguish extended coordinates.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/center_exact" || {
    printf '\n%s\n' "not ok 1 - K-center exact bins distinguish extended coordinates"
    exit 0
}
printf '\n%s\n' "ok 1 - K-center exact bins distinguish extended coordinates"
exit 0
