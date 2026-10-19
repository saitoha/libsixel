#!/bin/sh
# Axis splits order fractional means inside integer buckets.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_policy/bucket_fractional_axis >/dev/null || {
    echo "not ok 1 - bucket_fractional_axis"
    exit 0
}

echo "ok 1 - bucket_fractional_axis"
exit 0
