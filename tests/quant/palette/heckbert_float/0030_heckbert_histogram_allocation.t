#!/bin/sh
# Verify native Heckbert histogram allocation.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0030_heckbert_histogram_allocation >/dev/null || {
    echo "not ok 1 - native Heckbert histogram allocation"
    exit 0
}

echo "ok 1 - native Heckbert histogram allocation"
exit 0
