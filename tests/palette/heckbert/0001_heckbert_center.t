#!/bin/sh
# Verify native Heckbert center.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0001_heckbert_center >/dev/null || {
    printf '\n%s\n' "not ok 1 - native Heckbert center"
    exit 0
}

printf '\n%s\n' "ok 1 - native Heckbert center"
exit 0
