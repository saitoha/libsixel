#!/bin/sh
# Verify native Heckbert ward refine.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0016_heckbert_ward_refine >/dev/null || {
    echo "not ok 1 - native Heckbert ward refine"
    exit 0
}

echo "ok 1 - native Heckbert ward refine"
exit 0
