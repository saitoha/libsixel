#!/bin/sh
# Verify native Heckbert histogram.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0003_heckbert_histogram >/dev/null || {
    echo "not ok 1 - native Heckbert histogram"
    exit 0
}

echo "ok 1 - native Heckbert histogram"
exit 0
