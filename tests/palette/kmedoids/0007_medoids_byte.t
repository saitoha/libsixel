#!/bin/sh
# True byte inputs retain byte storage and exact representatives.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/medoids_byte" || {
    echo "not ok 1 - True byte inputs retain byte storage and exact representatives"
    exit 0
}
echo "ok 1 - True byte inputs retain byte storage and exact representatives"
exit 0
