#!/bin/sh
# Verify native Heckbert ward cielab.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0021_heckbert_ward_cielab >/dev/null || {
    printf '\n%s\n' "not ok 1 - native Heckbert ward cielab"
    exit 0
}

printf '\n%s\n' "ok 1 - native Heckbert ward cielab"
exit 0
