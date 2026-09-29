#!/bin/sh
# Verify native Heckbert representative allocation.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0031_heckbert_representative_allocation >/dev/null || {
    echo "not ok 1 - native Heckbert representative allocation"
    exit 0
}

echo "ok 1 - native Heckbert representative allocation"
exit 0
