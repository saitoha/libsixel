#!/bin/sh
# Verify native Heckbert range axis.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0012_heckbert_range_axis >/dev/null || {
    echo "not ok 1 - native Heckbert range axis"
    exit 0
}

echo "ok 1 - native Heckbert range axis"
exit 0
