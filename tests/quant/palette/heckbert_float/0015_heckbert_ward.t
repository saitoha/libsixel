#!/bin/sh
# Verify native Heckbert ward.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0015_heckbert_ward >/dev/null || {
    echo "not ok 1 - native Heckbert ward"
    exit 0
}

echo "ok 1 - native Heckbert ward"
exit 0
